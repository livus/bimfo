#include "fetcher.h"

#include <stdio.h>
#include <string.h>

#include "app_state.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "http_fetch.h"
#include "oebb_parse.h"
#include "wl_parse.h"

static const char *TAG = "fetch";

#define STALE_KEEP_S        (15 * 60)
#define WL_MAX_BODY         (384 * 1024)
#define OEBB_MAX_BODY       (64 * 1024)
#define HTTP_TIMEOUT_MS     15000
#define OEBB_JOURNEYS       16
#define WL_RATE_LIMITED     316     // messageCode for "too many requests"

EXT_RAM_BSS_ATTR static board_t s_work;     // board being assembled
EXT_RAM_BSS_ATTR static board_t s_stop;     // one stop's departures
EXT_RAM_BSS_ATTR static board_t s_prev;     // last published board
static int64_t s_stop_ok_us[CFG_MAX_STOPS];  // esp_timer time of last success per stop

static int64_t uptime_s(void)
{
    return esp_timer_get_time() / 1000000;
}

static bool fetch_wl(const cfg_stop_t *st, uint8_t idx, board_t *out, char *err, size_t n, bool *rate_limited)
{
    char url[512];
    wl_build_url(st, url, sizeof(url));
    http_buf_t buf;
    esp_err_t e = http_get(url, WL_MAX_BODY, HTTP_TIMEOUT_MS, &buf);
    if (e != ESP_OK) {
        char why[48];
        snprintf(err, n, "%s: %s", st->name, http_strerror(e, &buf, why, sizeof(why)));
        http_buf_free(&buf);
        return false;
    }
    int64_t server_time = 0;
    int code = 0;
    wl_status_t r = wl_parse_monitor(buf.data, st, idx, out, &server_time, &code);
    http_buf_free(&buf);
    if (server_time) {
        app_set_time_from_server(server_time);
    }
    if (r == WL_ERR_JSON) {
        snprintf(err, n, "%s: ungültige Antwort", st->name);
        return false;
    }
    if (r == WL_ERR_API) {
        *rate_limited = code == WL_RATE_LIMITED;
        snprintf(err, n, "%s: %s (Code %d)", st->name, *rate_limited ? "Anfragelimit" : "API-Fehler", code);
        return false;
    }
    return true;
}

static bool fetch_oebb(const cfg_stop_t *st, uint8_t idx, board_t *out, char *err, size_t n)
{
    int64_t now = app_now();
    if (!now) {
        // The board query and the countdown need the local time.
        snprintf(err, n, "%s: warte auf Uhrzeit", st->name);
        return false;
    }
    char url[512];
    oebb_build_board_url(st, now, OEBB_JOURNEYS, url, sizeof(url));
    http_buf_t buf;
    esp_err_t e = http_get(url, OEBB_MAX_BODY, HTTP_TIMEOUT_MS, &buf);
    if (e != ESP_OK) {
        char why[48];
        snprintf(err, n, "%s: %s", st->name, http_strerror(e, &buf, why, sizeof(why)));
        http_buf_free(&buf);
        return false;
    }
    int added = oebb_parse_board(buf.data, st, idx, out, NULL, 0);
    http_buf_free(&buf);
    if (added < 0) {
        snprintf(err, n, "%s: ungültige Antwort", st->name);
        return false;
    }
    return true;
}

static uint32_t next_delay_s(const app_config_t *cfg, const fetch_status_t *fs, bool rate_limited)
{
    if (fs->consecutive_errors == 0) {
        return cfg->refresh_s;
    }
    uint32_t d = 15u << (fs->consecutive_errors > 5 ? 5 : fs->consecutive_errors - 1);
    if (rate_limited && d < 120) {
        d = 120;
    }
    return d > 300 ? 300 : d;
}

static void fetch_task(void *arg)
{
    EventGroupHandle_t ev = app_events();
    fetch_status_t fs = {0};
    EXT_RAM_BSS_ATTR static app_config_t cfg;

    for (;;) {
        xEventGroupWaitBits(ev, APP_EV_STA_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
        xEventGroupClearBits(ev, APP_EV_CONFIG_CHANGED);
        app_get_config(&cfg);
        // Kept departures are indexed by stop position: forget them when the
        // stop list changes (added, removed, reordered, edited).
        EXT_RAM_BSS_ATTR static cfg_stop_t prev_stops[CFG_MAX_STOPS];
        static uint8_t prev_nstops = 0xFF;
        if (prev_nstops != cfg.nstops || memcmp(prev_stops, cfg.stops, sizeof(prev_stops)) != 0) {
            memcpy(prev_stops, cfg.stops, sizeof(prev_stops));
            prev_nstops = cfg.nstops;
            memset(s_stop_ok_us, 0, sizeof(s_stop_ok_us));
            board_clear(&s_prev);
        }

        board_clear(&s_work);
        cfg_board_title(&cfg, s_work.title, sizeof(s_work.title));
        char err[96] = "";
        bool rate_limited = false;
        int failed = 0;
        // Every stop gets an equal share of the board, so a big station
        // cannot crowd out the others.
        int share = cfg.nstops ? BOARD_MAX_DEPS / cfg.nstops : BOARD_MAX_DEPS;
        for (uint8_t i = 0; i < cfg.nstops; i++) {
            const cfg_stop_t *st = &cfg.stops[i];
            char e[96] = "";
            board_clear(&s_stop);
            bool ok = st->kind == STOP_OEBB ? fetch_oebb(st, i, &s_stop, e, sizeof(e))
                                            : fetch_wl(st, i, &s_stop, e, sizeof(e), &rate_limited);
            if (ok) {
                s_stop_ok_us[i] = esp_timer_get_time();
            } else {
                failed++;
                if (!err[0]) {
                    strlcpy(err, e, sizeof(err));
                }
                ESP_LOGW(TAG, "%s", e);
                // Fall back to the stop's last good departures while they are
                // reasonably fresh (stale ones drop off by their time anyway).
                board_clear(&s_stop);
                if (s_stop_ok_us[i] && uptime_s() - s_stop_ok_us[i] / 1000000 < STALE_KEEP_S) {
                    board_copy_stop(&s_stop, &s_prev, i);
                }
            }
            board_append_soonest(&s_work, &s_stop, share);
        }
        board_sort(&s_work);

        int64_t now = app_now();
        fs.last_attempt = now;
        if (failed == 0) {
            fs.ok_count++;
            fs.consecutive_errors = 0;
            fs.last_ok = now;
            fs.last_error[0] = 0;
            s_work.fetched_at = now ? now : 1;
        } else {
            fs.error_count++;
            if (fs.consecutive_errors < 255) fs.consecutive_errors++;
            strlcpy(fs.last_error, err, sizeof(fs.last_error));
            // Keep the "loaded" marker if anything (fresh or kept) is shown.
            s_work.fetched_at = failed < cfg.nstops ? (now ? now : 1) : s_prev.fetched_at;
        }
        app_set_board(&s_work);
        app_set_fetch(&fs);
        app_note_stack();
        s_prev = s_work;

        uint32_t delay = next_delay_s(&cfg, &fs, rate_limited);
        ESP_LOGI(TAG, "%d departures, %d/%d stops failed, next in %lus", s_work.count, failed, cfg.nstops,
                 (unsigned long)delay);
        // Sleep, but wake up immediately when the configuration changes.
        xEventGroupWaitBits(ev, APP_EV_CONFIG_CHANGED, pdFALSE, pdFALSE, pdMS_TO_TICKS(delay * 1000));
    }
}

void fetcher_start(void)
{
    // TLS handshakes need a generous stack.
    xTaskCreatePinnedToCore(fetch_task, "fetch", 10240, NULL, 4, NULL, 0);
}
