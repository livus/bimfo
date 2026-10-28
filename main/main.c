// Bimfo: live departures of Wiener Linien and OEBB stops on the
// Waveshare ESP32-S3-LCD-1.47B. See README.md for the architecture.
#include <stdlib.h>

#include "app_state.h"
#include "cJSON.h"
#include "display_task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "fetcher.h"
#include "nvs_flash.h"
#include "updater.h"
#include "web_server.h"
#include "wifi_mgr.h"

static const char *TAG = "main";

// Parsing one station response creates thousands of small cJSON nodes. Keep
// them in PSRAM so they never eat the internal RAM that WiFi buffers need.
static void *json_malloc(size_t size)
{
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition reset (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    cJSON_Hooks hooks = {.malloc_fn = json_malloc, .free_fn = free};
    cJSON_InitHooks(&hooks);

    app_state_init();       // loads the configuration
    display_task_start();   // LCD, LVGL and UI live in their own task
    wifi_mgr_start();
    updater_start();        // nightly update check (when enabled)
    web_server_start();
    fetcher_start();
}
