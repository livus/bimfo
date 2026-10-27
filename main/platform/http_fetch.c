#include "http_fetch.h"

#include <stdbool.h>  // esp_crt_bundle.h uses bool without including it
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "http";

typedef struct {
    http_buf_t *buf;
    size_t cap;
    size_t max;
    bool overflow;
} fetch_ctx_t;

static void *big_realloc(void *p, size_t n)
{
    void *r = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return r ? r : realloc(p, n);
}

static esp_err_t on_event(esp_http_client_event_t *evt)
{
    fetch_ctx_t *ctx = evt->user_data;
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        // A redirect starts over: drop the body of the previous response.
        ctx->buf->len = 0;
        ctx->overflow = false;
        break;
    case HTTP_EVENT_ON_DATA: {
        int status = esp_http_client_get_status_code(evt->client);
        if (ctx->overflow || evt->data_len <= 0 || (status >= 300 && status < 400)) {
            break;  // redirect bodies are not part of the result
        }
        size_t need = ctx->buf->len + (size_t)evt->data_len + 1;
        if (need > ctx->max) {
            ctx->overflow = true;
            break;
        }
        if (need > ctx->cap) {
            size_t cap = ctx->cap ? ctx->cap : 8192;
            while (cap < need) cap *= 2;
            if (cap > ctx->max) cap = ctx->max;
            char *p = big_realloc(ctx->buf->data, cap);
            if (!p) {
                ctx->overflow = true;
                break;
            }
            ctx->buf->data = p;
            ctx->cap = cap;
        }
        memcpy(ctx->buf->data + ctx->buf->len, evt->data, (size_t)evt->data_len);
        ctx->buf->len += (size_t)evt->data_len;
        ctx->buf->data[ctx->buf->len] = 0;
        break;
    }
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t http_get(const char *url, size_t max_len, int timeout_ms, http_buf_t *out)
{
    memset(out, 0, sizeof(*out));
    fetch_ctx_t ctx = {.buf = out, .max = max_len};
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = timeout_ms,
        .event_handler = on_event,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .max_redirection_count = 3,
        .user_agent = "bimfo-esp32/1.0",
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_err_t err = esp_http_client_perform(client);
    out->status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (!out->data) {
        // Always hand back a valid empty string.
        out->data = calloc(1, 1);
    }
    if (err == ESP_OK && ctx.overflow) {
        err = ESP_ERR_INVALID_SIZE;
    } else if (err == ESP_OK && (out->status < 200 || out->status >= 300)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "GET %.80s -> %s (status %d, %u bytes)", url, esp_err_to_name(err), out->status,
                 (unsigned)out->len);
    }
    return err;
}

void http_buf_free(http_buf_t *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
}

const char *http_strerror(esp_err_t err, const http_buf_t *b, char *buf, size_t n)
{
    if (err == ESP_ERR_INVALID_RESPONSE) {
        snprintf(buf, n, "HTTP %d", b ? b->status : 0);
    } else if (err == ESP_ERR_INVALID_SIZE) {
        snprintf(buf, n, "Antwort zu gross");
    } else if (err == ESP_ERR_HTTP_CONNECT || err == ESP_ERR_HTTP_EAGAIN || err == ESP_ERR_TIMEOUT) {
        snprintf(buf, n, "Server nicht erreichbar");
    } else {
        snprintf(buf, n, "%s", esp_err_to_name(err));
    }
    return buf;
}
