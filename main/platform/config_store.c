#include "config_store.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "config";
#define NVS_NS  "abfahrt"   // the project's old name; kept so devices keep their settings
#define NVS_KEY "config"

void config_store_load(app_config_t *cfg)
{
    cfg_defaults(cfg);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = 0;
        if (nvs_get_str(h, NVS_KEY, NULL, &len) == ESP_OK && len > 0) {
            char *json = malloc(len);
            if (json && nvs_get_str(h, NVS_KEY, json, &len) == ESP_OK) {
                char err[48];
                if (!cfg_from_json(cfg, json, err, sizeof(err))) {
                    ESP_LOGE(TAG, "stored config unreadable (%s), using defaults", err);
                }
            }
            free(json);
        }
        nvs_close(h);
    }
#ifdef CONFIG_APP_DEV_WIFI_SSID
    // Developer convenience: credentials from menuconfig (kept in the
    // untracked sdkconfig) when nothing was configured on the device yet.
    if (!cfg->wifi_ssid[0] && CONFIG_APP_DEV_WIFI_SSID[0]) {
        strlcpy(cfg->wifi_ssid, CONFIG_APP_DEV_WIFI_SSID, sizeof(cfg->wifi_ssid));
        strlcpy(cfg->wifi_pass, CONFIG_APP_DEV_WIFI_PASSWORD, sizeof(cfg->wifi_pass));
    }
#endif
    ESP_LOGI(TAG, "loaded: wifi '%s', %d stop(s), theme %s", cfg->wifi_ssid, cfg->nstops,
             cfg_theme_name(cfg->theme));
}

esp_err_t config_store_save(const app_config_t *cfg)
{
    char *json = cfg_to_json(cfg, true);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_str(h, NVS_KEY, json);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    free(json);
    return err;
}

esp_err_t config_store_erase(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_erase_all(h);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    return err;
}
