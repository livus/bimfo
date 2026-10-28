#include "updater.h"

#include <stdbool.h>  // esp_crt_bundle.h uses bool without including it
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "app_state.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_secure_boot.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "update_policy.h"

// A downloaded image is installed only if it belongs to this project, is a
// newer release than the running one, is not the version that was just rolled
// back, and is signed with the key of the running firmware. esp_ota_end()
// checks the signature (CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT); the
// rest is checked here from the image header before anything is written.

static const char *TAG = "update";

#define TICK_US (60LL * 1000000)

static SemaphoreHandle_t s_mutex;   // guards s_status
static SemaphoreHandle_t s_write;   // firmware write lock (binary: any task may give)
static update_status_t s_status = {.progress = -1};
static bool s_signed;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

static bool running_is_signed(void)
{
#if CONFIG_SECURE_SIGNED_ON_UPDATE && (CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME || CONFIG_SECURE_SIGNED_APPS_ECDSA_V2_SCHEME)
    esp_image_sig_public_key_digests_t keys = {0};
    return esp_secure_boot_get_signature_blocks_for_running_app(true, &keys) == ESP_OK && keys.num_digests > 0;
#else
    return false;   // without signature checks nothing is installed from the network
#endif
}

static void set_progress(int p)
{
    LOCK();
    s_status.progress = p;
    UNLOCK();
}

static void set_message(const char *msg)
{
    LOCK();
    strlcpy(s_status.message, msg, sizeof(s_status.message));
    UNLOCK();
}

static void http_defaults(esp_http_client_config_t *c, const char *url)
{
    *c = (esp_http_client_config_t){
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,     // GitHub redirects to a ~1 kB download URL
        .user_agent = "bimfo-esp32/1.0",
    };
}

static esp_err_t on_location(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_HEADER && strcasecmp(evt->header_key, "Location") == 0) {
        char **loc = evt->user_data;
        free(*loc);
        *loc = strdup(evt->header_value);
    }
    return ESP_OK;
}

// Follows the redirects (GitHub: latest -> tagged release -> file storage)
// with HEAD requests, each on a new connection, and returns the final URL
// (malloc'd). esp_https_ota would reuse the connection for a redirect to the
// same host even if the server closed it.
static esp_err_t resolve_url(const char *url, char **out, int *status)
{
    char *cur = strdup(url);
    for (int hop = 0; cur && hop < 5; hop++) {
        char *loc = NULL;
        esp_http_client_config_t c;
        http_defaults(&c, cur);
        c.method = HTTP_METHOD_HEAD;
        c.disable_auto_redirect = true;
        c.event_handler = on_location;
        c.user_data = &loc;
        esp_http_client_handle_t client = esp_http_client_init(&c);
        if (!client) {
            break;
        }
        esp_err_t e = esp_http_client_perform(client);
        *status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        if (e != ESP_OK) {
            free(loc);
            free(cur);
            return e;
        }
        if (*status >= 300 && *status < 400 && loc && loc[0]) {
            char *next = loc;
            if (loc[0] == '/') {   // relative: keep scheme and host
                const char *host = strstr(cur, "://");
                const char *path = host ? strchr(host + 3, '/') : NULL;
                int keep = path ? (int)(path - cur) : (int)strlen(cur);
                if (asprintf(&next, "%.*s%s", keep, cur, loc) < 0) {
                    next = NULL;
                }
                free(loc);
            }
            free(cur);
            cur = next;
            continue;
        }
        free(loc);
        if (*status == 200) {
            *out = cur;
            return ESP_OK;
        }
        free(cur);
        return ESP_ERR_INVALID_RESPONSE;
    }
    free(cur);
    return ESP_FAIL;
}

// Returns true once the new version is installed and set as boot partition;
// `current` tells "nothing newer" apart from a failure.
static bool download_and_install(char *msg, size_t n, bool *current)
{
    const esp_app_desc_t *running = esp_app_get_description();
    char *url = NULL;
    int status = 0;
    esp_err_t e = resolve_url(CONFIG_APP_UPDATE_URL, &url, &status);
    if (e != ESP_OK) {
        if (e == ESP_ERR_HTTP_CONNECT) {
            snprintf(msg, n, "Server nicht erreichbar");
        } else if (e == ESP_ERR_INVALID_RESPONSE && status == 404) {
            snprintf(msg, n, "Keine veröffentlichte Version gefunden");
        } else if (e == ESP_ERR_INVALID_RESPONSE) {
            snprintf(msg, n, "Update-Datei nicht abrufbar (HTTP %d)", status);
        } else {
            snprintf(msg, n, "Update-Datei nicht abrufbar (%s)", esp_err_to_name(e));
        }
        return false;
    }
    esp_http_client_config_t http;
    http_defaults(&http, url);
    esp_https_ota_config_t cfg = {.http_config = &http};
    esp_https_ota_handle_t h = NULL;
    e = esp_https_ota_begin(&cfg, &h);
    free(url);   // the client keeps its own copy
    if (e != ESP_OK) {
        snprintf(msg, n, "Update-Datei nicht abrufbar (%s)", esp_err_to_name(e));
        return false;
    }

    // Only the image header has been read so far.
    esp_app_desc_t desc;
    if (esp_https_ota_get_img_desc(h, &desc) != ESP_OK) {
        snprintf(msg, n, "Keine gültige Firmware-Datei");
        goto abort;
    }
    desc.version[sizeof(desc.version) - 1] = 0;
    if (strncmp(desc.project_name, running->project_name, sizeof(desc.project_name)) != 0) {
        snprintf(msg, n, "Firmware gehört zu einem anderen Projekt");
        goto abort;
    }
    if (!upd_is_newer(desc.version, running->version)) {
        snprintf(msg, n, "Aktuell (neueste Version %s)", desc.version);
        *current = true;
        goto abort;
    }
    // A version that did not start properly was rolled back: do not install
    // it again every night.
    const esp_partition_t *bad = esp_ota_get_last_invalid_partition();
    esp_app_desc_t bad_desc;
    if (bad && esp_ota_get_partition_description(bad, &bad_desc) == ESP_OK &&
        strncmp(bad_desc.version, desc.version, sizeof(desc.version)) == 0) {
        snprintf(msg, n, "Version %s ist nicht gestartet und wird übersprungen", desc.version);
        goto abort;
    }

    ESP_LOGI(TAG, "downloading %s (running %s)", desc.version, running->version);
    snprintf(msg, n, "Lade Version %s", desc.version);
    set_message(msg);
    set_progress(0);
    int total = esp_https_ota_get_image_size(h);
    while ((e = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        if (total > 0) {
            set_progress((int)(100LL * esp_https_ota_get_image_len_read(h) / total));
        }
    }
    if (e != ESP_OK || !esp_https_ota_is_complete_data_received(h)) {
        snprintf(msg, n, "Download abgebrochen (%s)", esp_err_to_name(e));
        goto abort;
    }
    e = esp_https_ota_finish(h);   // checks hash and signature, sets the boot partition
    if (e == ESP_ERR_OTA_VALIDATE_FAILED) {
        snprintf(msg, n, "Version %s abgelehnt: Signatur oder Prüfsumme ungültig", desc.version);
        return false;
    }
    if (e != ESP_OK) {
        snprintf(msg, n, "Version %s nicht installiert (%s)", desc.version, esp_err_to_name(e));
        return false;
    }
    snprintf(msg, n, "Version %s installiert, Neustart", desc.version);
    return true;

abort:
    esp_https_ota_abort(h);
    return false;
}

static void check_task(void *arg)
{
    char msg[sizeof(s_status.message)];
    bool current = false;
    bool installed = download_and_install(msg, sizeof(msg), &current);
    if (installed || current) {
        ESP_LOGI(TAG, "%s", msg);
    } else {
        ESP_LOGW(TAG, "%s", msg);
    }
    app_note_stack();
    LOCK();
    s_status.busy = false;
    s_status.progress = -1;
    s_status.restarting = installed;
    s_status.last_check = app_now();
    strlcpy(s_status.message, msg, sizeof(s_status.message));
    UNLOCK();
    if (installed) {
        vTaskDelay(pdMS_TO_TICKS(3000));   // let an open settings page notice
        esp_restart();
    }
    updater_unlock();
    vTaskDelete(NULL);
}

const char *updater_check_now(void)
{
    if (!s_signed) {
        return "Diese Firmware ist nicht signiert (eigener Build): Updates nur über USB";
    }
    if (!(xEventGroupGetBits(app_events()) & APP_EV_STA_CONNECTED)) {
        return "Keine Internetverbindung";
    }
    if (!updater_lock()) {
        return "Es läuft bereits ein Update";
    }
    LOCK();
    s_status.busy = true;
    s_status.progress = -1;
    strlcpy(s_status.message, "Prüfe", sizeof(s_status.message));
    UNLOCK();
    // TLS needs a large stack; it only exists while a check runs.
    if (xTaskCreatePinnedToCore(check_task, "update", 8192, NULL, 3, NULL, 0) != pdPASS) {
        LOCK();
        s_status.busy = false;
        UNLOCK();
        updater_unlock();
        return "Zu wenig Speicher";
    }
    return NULL;
}

static void tick(void *arg)
{
    app_config_t *cfg = malloc(sizeof(*cfg));
    if (!cfg) {
        return;
    }
    app_get_config(cfg);
    bool on = cfg->auto_update;
    free(cfg);
    int64_t now = app_now();

    LOCK();
    bool due = false;
    if (!on || !now || !s_signed) {
        s_status.next_check = 0;
    } else if (!s_status.next_check) {
        s_status.next_check = upd_next_check(now, esp_random());
    } else if (now >= s_status.next_check) {
        due = true;
        s_status.next_check = upd_next_check(now, esp_random());
    }
    UNLOCK();
    if (due) {
        const char *why = updater_check_now();
        if (why) {
            ESP_LOGW(TAG, "automatic check skipped: %s", why);
            LOCK();
            strlcpy(s_status.message, why, sizeof(s_status.message));
            s_status.last_check = now;
            UNLOCK();
        }
    }
}

void updater_start(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_write = xSemaphoreCreateBinary();
    xSemaphoreGive(s_write);
    s_signed = running_is_signed();
    s_status.available = s_signed;
    ESP_LOGI(TAG, "firmware %s, updates from %s", s_signed ? "signed" : "NOT signed (no updates)",
             CONFIG_APP_UPDATE_URL);

    const esp_timer_create_args_t a = {.callback = tick, .name = "update"};
    esp_timer_handle_t t;
    if (esp_timer_create(&a, &t) == ESP_OK) {
        esp_timer_start_periodic(t, TICK_US);
    }
}

void updater_get_status(update_status_t *out)
{
    LOCK();
    *out = s_status;
    UNLOCK();
}

bool updater_signed(void)
{
    return s_signed;
}

bool updater_lock(void)
{
    return xSemaphoreTake(s_write, 0) == pdTRUE;
}

void updater_unlock(void)
{
    xSemaphoreGive(s_write);
}
