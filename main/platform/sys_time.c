#include "sys_time.h"

#include "app_state.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "time";
static bool s_started;

static void on_sync(struct timeval *tv)
{
    ESP_LOGI(TAG, "clock synchronised via SNTP");
    xEventGroupSetBits(app_events(), APP_EV_TIME_SYNCED);
}

void sys_time_start(void)
{
    if (s_started) {
        return;
    }
    s_started = true;
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("at.pool.ntp.org");
    cfg.sync_cb = on_sync;
    esp_netif_sntp_init(&cfg);
}
