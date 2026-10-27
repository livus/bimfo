#include "web_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_json.h"
#include "app_state.h"
#include "cJSON.h"
#include "config_store.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "http_fetch.h"
#include "textutil.h"
#include "wifi_mgr.h"

static const char *TAG = "web";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

#define MAX_CONFIG_BODY     8192
#define STATION_CSV_MAX     (512 * 1024)
#define STATION_CSV_TTL_US  (24LL * 3600 * 1000000)

static char *s_csv;              // cached Wiener Linien station list (PSRAM)
static int64_t s_csv_time_us;
static esp_timer_handle_t s_restart_timer;

// ---------------------------------------------------------------------------
// Helpers

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!s) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, s);
    free(s);
    return err;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *msg)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", msg);
    httpd_resp_set_status(req, status);
    return send_json(req, o);
}

static esp_err_t send_ok(httpd_req_t *req, bool restart)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "ok", true);
    cJSON_AddBoolToObject(o, "restart", restart);
    return send_json(req, o);
}

// Reads a query parameter, URL-decoded. Returns false if missing.
static bool query_param(httpd_req_t *req, const char *key, char *out, size_t n)
{
    char query[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, key, out, n) != ESP_OK) {
        return false;
    }
    tx_url_decode(out);
    tx_trim(out);
    return out[0] != 0;
}

// Reads the whole request body (NUL-terminated, malloc'd) or NULL.
static char *read_body(httpd_req_t *req, size_t max)
{
    if (req->content_len == 0 || req->content_len > max) {
        return NULL;
    }
    char *buf = malloc(req->content_len + 1);
    if (!buf) {
        return NULL;
    }
    size_t got = 0;
    int timeouts = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) {
            continue;
        }
        if (r <= 0) {
            free(buf);
            return NULL;
        }
        got += (size_t)r;
    }
    buf[got] = 0;
    return buf;
}

// State-changing requests must carry the X-Bimfo header. Browsers only send
// custom headers cross-origin after a CORS preflight, which this server never
// approves, so other web pages cannot reconfigure the device.
static bool csrf_ok(httpd_req_t *req)
{
    if (httpd_req_get_hdr_value_len(req, "X-Bimfo") > 0) {
        return true;
    }
    send_error(req, "403 Forbidden", "X-Bimfo header fehlt");
    return false;
}

static void restart_cb(void *arg)
{
    esp_restart();
}

static void schedule_restart(void)
{
    if (!s_restart_timer) {
        const esp_timer_create_args_t a = {.callback = restart_cb, .name = "restart"};
        esp_timer_create(&a, &s_restart_timer);
    }
    esp_timer_stop(s_restart_timer);
    esp_timer_start_once(s_restart_timer, 1500 * 1000);  // let the response go out first
}

// ---------------------------------------------------------------------------
// Handlers

static esp_err_t page_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

static esp_err_t status_get(httpd_req_t *req)
{
    net_status_t net;
    fetch_status_t fs;
    app_config_t *cfg = malloc(sizeof(*cfg));
    if (!cfg) return httpd_resp_send_500(req);
    app_get_net(&net);
    app_get_fetch(&fs);
    app_get_config(cfg);
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "version", app->version);
    cJSON_AddStringToObject(o, "build", app->date);
    cJSON_AddStringToObject(o, "idf", app->idf_ver);
    cJSON_AddStringToObject(o, "partition", running ? running->label : "?");
    cJSON_AddStringToObject(o, "hostname", cfg->hostname);
    cJSON_AddNumberToObject(o, "uptime", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(o, "heap", (double)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(o, "heap_min", (double)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(o, "psram", (double)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(o, "time", (double)app_now());
    cJSON_AddBoolToObject(o, "time_synced", (xEventGroupGetBits(app_events()) & APP_EV_TIME_SYNCED) != 0);

    cJSON *w = cJSON_AddObjectToObject(o, "wifi");
    cJSON_AddBoolToObject(w, "configured", net.sta_configured);
    cJSON_AddStringToObject(w, "ssid", net.sta_ssid);
    cJSON_AddBoolToObject(w, "connected", net.sta_connected);
    cJSON_AddStringToObject(w, "ip", net.ip);
    wifi_ap_record_t ap;
    cJSON_AddNumberToObject(w, "rssi", net.sta_connected && esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0);
    cJSON_AddNumberToObject(w, "disconnects", net.disconnects);

    cJSON *a = cJSON_AddObjectToObject(o, "ap");
    cJSON_AddBoolToObject(a, "active", net.ap_active);
    cJSON_AddStringToObject(a, "ssid", net.ap_ssid);
    cJSON_AddNumberToObject(a, "clients", net.ap_clients);

    // Stack head-room per task (bytes never used), for diagnostics.
    app_note_stack();
    char names[8][16];
    uint32_t free_bytes[8];
    int ns = app_stack_report(names, free_bytes, 8);
    cJSON *stacks = cJSON_AddObjectToObject(o, "stack_free");
    for (int i = 0; i < ns; i++) {
        cJSON_AddNumberToObject(stacks, names[i], free_bytes[i]);
    }

    cJSON *f = cJSON_AddObjectToObject(o, "fetch");
    cJSON_AddNumberToObject(f, "last_ok", (double)fs.last_ok);
    cJSON_AddNumberToObject(f, "last_attempt", (double)fs.last_attempt);
    cJSON_AddNumberToObject(f, "ok", fs.ok_count);
    cJSON_AddNumberToObject(f, "errors", fs.error_count);
    cJSON_AddNumberToObject(f, "consecutive_errors", fs.consecutive_errors);
    cJSON_AddStringToObject(f, "last_error", fs.last_error);
    free(cfg);
    return send_json(req, o);
}

static esp_err_t config_get(httpd_req_t *req)
{
    app_config_t *cfg = malloc(sizeof(*cfg));
    if (!cfg) return httpd_resp_send_500(req);
    app_get_config(cfg);
    char *json = cfg_to_json(cfg, false);
    free(cfg);
    if (!json) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, json);
    free(json);
    return err;
}

static esp_err_t config_post(httpd_req_t *req)
{
    if (!csrf_ok(req)) return ESP_OK;
    char *body = read_body(req, MAX_CONFIG_BODY);
    if (!body) {
        return send_error(req, "400 Bad Request", "Leere oder zu große Anfrage");
    }
    app_config_t *old = malloc(sizeof(*old));
    app_config_t *cfg = malloc(sizeof(*cfg));
    if (!old || !cfg) {
        free(body);
        free(old);
        free(cfg);
        return httpd_resp_send_500(req);
    }
    app_get_config(old);
    *cfg = *old;
    char err[48];
    bool ok = cfg_from_json(cfg, body, err, sizeof(err));
    free(body);
    esp_err_t res;
    if (!ok) {
        res = send_error(req, "400 Bad Request", err);
    } else if (app_set_config(cfg) != ESP_OK) {
        res = send_error(req, "500 Internal Server Error", "Speichern fehlgeschlagen");
    } else {
        // Network identity changes need a clean restart of WiFi and mDNS.
        bool restart = strcmp(old->wifi_ssid, cfg->wifi_ssid) != 0 || strcmp(old->wifi_pass, cfg->wifi_pass) != 0 ||
                       strcmp(old->hostname, cfg->hostname) != 0;
        ESP_LOGI(TAG, "configuration saved%s", restart ? ", restarting" : "");
        res = send_ok(req, restart);
        if (restart) {
            schedule_restart();
        }
    }
    free(old);
    free(cfg);
    return res;
}

static esp_err_t board_get(httpd_req_t *req)
{
    board_t *b = malloc(sizeof(*b));
    if (!b) return httpd_resp_send_500(req);
    app_get_board(b, NULL);
    cJSON *o = api_board_json(b, app_now());
    free(b);
    return send_json(req, o);
}

static esp_err_t scan_get(httpd_req_t *req)
{
    wifi_scan_item_t items[16];
    int n = wifi_mgr_scan(items, 16);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", items[i].ssid);
        cJSON_AddNumberToObject(o, "rssi", items[i].rssi);
        cJSON_AddBoolToObject(o, "secure", items[i].secure);
        cJSON_AddItemToArray(arr, o);
    }
    return send_json(req, arr);
}

static bool internet_up(httpd_req_t *req)
{
    if (xEventGroupGetBits(app_events()) & APP_EV_STA_CONNECTED) {
        return true;
    }
    send_error(req, "503 Service Unavailable", "Keine Internetverbindung (erst WLAN einrichten)");
    return false;
}

static esp_err_t wl_search_get(httpd_req_t *req)
{
    char q[64];
    if (!query_param(req, "q", q, sizeof(q))) {
        return send_error(req, "400 Bad Request", "Suchbegriff fehlt");
    }
    // The station list (~130 kB) is downloaded once and cached in PSRAM.
    if (!s_csv || esp_timer_get_time() - s_csv_time_us > STATION_CSV_TTL_US) {
        if (!internet_up(req)) return ESP_OK;
        http_buf_t buf;
        esp_err_t e = http_get(WL_STATIONS_CSV_URL, STATION_CSV_MAX, 20000, &buf);
        if (e != ESP_OK) {
            char why[48];
            http_strerror(e, &buf, why, sizeof(why));
            http_buf_free(&buf);
            if (!s_csv) {
                return send_error(req, "502 Bad Gateway", why);
            }
        } else {
            free(s_csv);
            s_csv = buf.data;
            s_csv_time_us = esp_timer_get_time();
        }
    }
    wl_station_t st[15];
    int n = wl_search_stations(s_csv, q, st, 15);
    app_note_stack();
    return send_json(req, api_wl_stations_json(st, n));
}

static esp_err_t wl_lines_get(httpd_req_t *req)
{
    char id[16];
    if (!query_param(req, "id", id, sizeof(id)) || atol(id) <= 0) {
        return send_error(req, "400 Bad Request", "Haltestelle fehlt");
    }
    if (!internet_up(req)) return ESP_OK;
    char url[160];
    snprintf(url, sizeof(url), WL_HOST "/ogd_realtime/monitor?diva=%ld", atol(id));
    http_buf_t buf;
    esp_err_t e = http_get(url, 384 * 1024, 15000, &buf);
    if (e != ESP_OK) {
        char why[48];
        http_strerror(e, &buf, why, sizeof(why));
        http_buf_free(&buf);
        return send_error(req, "502 Bad Gateway", why);
    }
    wl_line_info_t *lines = malloc(sizeof(wl_line_info_t) * 48);
    int n = lines ? wl_list_lines(buf.data, lines, 48) : -1;
    http_buf_free(&buf);
    if (n < 0) {
        free(lines);
        return send_error(req, "502 Bad Gateway", "Ungültige Antwort der Wiener Linien");
    }
    cJSON *arr = api_wl_lines_json(lines, n);
    free(lines);
    app_note_stack();
    return send_json(req, arr);
}

static esp_err_t oebb_search_get(httpd_req_t *req)
{
    char q[64];
    if (!query_param(req, "q", q, sizeof(q))) {
        return send_error(req, "400 Bad Request", "Suchbegriff fehlt");
    }
    if (!internet_up(req)) return ESP_OK;
    char url[256];
    oebb_build_search_url(q, url, sizeof(url));
    http_buf_t buf;
    esp_err_t e = http_get(url, 32 * 1024, 15000, &buf);
    if (e != ESP_OK) {
        char why[48];
        http_strerror(e, &buf, why, sizeof(why));
        http_buf_free(&buf);
        return send_error(req, "502 Bad Gateway", why);
    }
    oebb_station_t st[12];
    int n = oebb_parse_search(buf.data, st, 12);
    http_buf_free(&buf);
    return send_json(req, api_oebb_stations_json(st, n < 0 ? 0 : n));
}

static esp_err_t ota_post(httpd_req_t *req)
{
    if (!csrf_ok(req)) return ESP_OK;
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        return send_error(req, "500 Internal Server Error", "Keine OTA-Partition");
    }
    if (req->content_len < 1024 || req->content_len > part->size) {
        return send_error(req, "400 Bad Request", "Ungültige Dateigröße");
    }
    char *buf = malloc(4096);
    if (!buf) return httpd_resp_send_500(req);
    esp_ota_handle_t ota = 0;
    size_t remaining = req->content_len, done = 0;
    int timeouts = 0;
    const char *fail = NULL;
    while (remaining > 0 && !fail) {
        int r = httpd_req_recv(req, buf, remaining < 4096 ? remaining : 4096);
        if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 5) {
            continue;
        }
        if (r <= 0) {
            fail = "Upload abgebrochen";
            break;
        }
        if (done == 0) {
            // Refuse images of other projects before touching flash.
            const size_t off = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
            if ((size_t)r < off + sizeof(esp_app_desc_t) || (uint8_t)buf[0] != ESP_IMAGE_HEADER_MAGIC) {
                fail = "Keine gültige Firmware-Datei";
                break;
            }
            const esp_app_desc_t *desc = (const esp_app_desc_t *)(buf + off);
            if (strncmp(desc->project_name, esp_app_get_description()->project_name, sizeof(desc->project_name))) {
                fail = "Firmware gehört zu einem anderen Projekt";
                break;
            }
            if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) {
                fail = "OTA-Start fehlgeschlagen";
                break;
            }
        }
        if (esp_ota_write(ota, buf, (size_t)r) != ESP_OK) {
            fail = "Schreiben fehlgeschlagen";
            break;
        }
        done += (size_t)r;
        remaining -= (size_t)r;
    }
    free(buf);
    if (!fail) {
        esp_err_t e = esp_ota_end(ota);
        ota = 0;
        if (e != ESP_OK) {
            fail = e == ESP_ERR_OTA_VALIDATE_FAILED ? "Firmware beschädigt" : "OTA-Abschluss fehlgeschlagen";
        } else if (esp_ota_set_boot_partition(part) != ESP_OK) {
            fail = "Boot-Partition nicht gesetzt";
        }
    }
    if (ota) {
        esp_ota_abort(ota);
    }
    if (fail) {
        ESP_LOGE(TAG, "OTA failed: %s", fail);
        return send_error(req, "400 Bad Request", fail);
    }
    ESP_LOGI(TAG, "OTA ok (%u bytes) -> %s, restarting", (unsigned)done, part->label);
    send_ok(req, true);
    schedule_restart();
    return ESP_OK;
}

static esp_err_t reboot_post(httpd_req_t *req)
{
    if (!csrf_ok(req)) return ESP_OK;
    send_ok(req, true);
    schedule_restart();
    return ESP_OK;
}

static esp_err_t reset_post(httpd_req_t *req)
{
    if (!csrf_ok(req)) return ESP_OK;
    config_store_erase();
    send_ok(req, true);
    schedule_restart();
    return ESP_OK;
}

static esp_err_t setup_post(httpd_req_t *req)
{
    if (!csrf_ok(req)) return ESP_OK;
    wifi_mgr_force_setup(10 * 60);
    return send_ok(req, false);
}

// Captive portal: while the setup AP is up, send everything unknown (OS
// connectivity checks like /generate_204, /hotspot-detect.html) to the page.
static esp_err_t not_found(httpd_req_t *req, httpd_err_code_t code)
{
    net_status_t net;
    app_get_net(&net);
    if (net.ap_active && strncmp(req->uri, "/api/", 5) != 0) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://" WIFI_AP_IP "/");
        return httpd_resp_send(req, NULL, 0);
    }
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    return ESP_FAIL;
}

void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 12288;           // handlers make HTTPS requests
    cfg.max_uri_handlers = 20;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 10;
    cfg.send_wait_timeout = 10;
    cfg.core_id = 0;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "cannot start web server");
        return;
    }
    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = page_get},
        {.uri = "/index.html", .method = HTTP_GET, .handler = page_get},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get},
        {.uri = "/api/config", .method = HTTP_GET, .handler = config_get},
        {.uri = "/api/config", .method = HTTP_POST, .handler = config_post},
        {.uri = "/api/board", .method = HTTP_GET, .handler = board_get},
        {.uri = "/api/wifi/scan", .method = HTTP_GET, .handler = scan_get},
        {.uri = "/api/wl/search", .method = HTTP_GET, .handler = wl_search_get},
        {.uri = "/api/wl/lines", .method = HTTP_GET, .handler = wl_lines_get},
        {.uri = "/api/oebb/search", .method = HTTP_GET, .handler = oebb_search_get},
        {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_post},
        {.uri = "/api/reboot", .method = HTTP_POST, .handler = reboot_post},
        {.uri = "/api/reset", .method = HTTP_POST, .handler = reset_post},
        {.uri = "/api/setup", .method = HTTP_POST, .handler = setup_post},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found);
    ESP_LOGI(TAG, "web server running");
}
