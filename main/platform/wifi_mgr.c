#include "wifi_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_state.h"
#include "dns_server.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "mdns.h"
#include "sys_time.h"
#if CONFIG_APP_QEMU
#include "esp_eth.h"
#endif

static const char *TAG = "wifi";

#define STA_GRACE_US        (30LL * 1000000)    // try the station this long before opening the AP
#define RECONNECT_MIN_MS    2000
#define RECONNECT_MAX_MS    60000
#define NET_POLL_MS         5000

// Event handlers and timers only record what happened and wake the net task,
// which makes all access point / service decisions on its own stack.
static TaskHandle_t s_net_task;
static esp_timer_handle_t s_reconnect_timer;
static volatile int64_t s_grace_until_us;   // 0 = no pending grace period
static volatile int64_t s_force_ap_until_us;
static volatile bool s_got_ip;
static uint32_t s_backoff_ms = RECONNECT_MIN_MS;
static bool s_ap_on;
static bool s_sta_configured;
static bool s_mdns_started;
static char s_hostname[32];
static char s_ap_ssid[33];

static void set_connected(net_status_t *n, void *ctx)
{
    const esp_netif_ip_info_t *ip = ctx;
    n->sta_connected = ip != NULL;
    if (ip) {
        snprintf(n->ip, sizeof(n->ip), IPSTR, IP2STR(&ip->ip));
    } else {
        n->ip[0] = 0;
        n->disconnects++;
    }
}

static void set_ap(net_status_t *n, void *ctx)
{
    n->ap_active = s_ap_on;
    strlcpy(n->ap_ssid, s_ap_on ? s_ap_ssid : "", sizeof(n->ap_ssid));
    n->ap_clients = ctx ? *(uint8_t *)ctx : 0;
}

static void set_sta(net_status_t *n, void *ctx)
{
    const app_config_t *cfg = ctx;
    n->sta_configured = cfg->wifi_ssid[0] != 0;
    strlcpy(n->sta_ssid, cfg->wifi_ssid, sizeof(n->sta_ssid));
}

static void wake_net_task(void)
{
    if (s_net_task) {
        xTaskNotifyGive(s_net_task);
    }
}

static uint8_t ap_client_count(void)
{
    wifi_sta_list_t list;
    return s_ap_on && esp_wifi_ap_get_sta_list(&list) == ESP_OK ? (uint8_t)list.num : 0;
}

static void ap_set(bool on, bool sta_configured)
{
    if (on == s_ap_on) {
        return;
    }
    s_ap_on = on;
    if (on) {
        wifi_config_t ap = {
            .ap = {
                .channel = 1,
                .max_connection = 4,
                .authmode = WIFI_AUTH_OPEN,
            },
        };
        strlcpy((char *)ap.ap.ssid, s_ap_ssid, sizeof(ap.ap.ssid));
        ap.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);
        esp_wifi_set_mode(sta_configured ? WIFI_MODE_APSTA : WIFI_MODE_AP);
        esp_wifi_set_config(WIFI_IF_AP, &ap);
        dns_server_start(WIFI_AP_IP);
        ESP_LOGI(TAG, "setup access point '%s' on", s_ap_ssid);
    } else {
        dns_server_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
        ESP_LOGI(TAG, "setup access point off");
    }
}

static void start_mdns(void)
{
    if (s_mdns_started) {
        return;
    }
    if (mdns_init() == ESP_OK) {
        mdns_hostname_set(s_hostname);
        mdns_instance_name_set("Bimfo");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        s_mdns_started = true;
    }
}

// The AP is up while no network is configured, while setup is forced, when
// the station could not connect within the grace period, and as long as a
// phone is still attached to it (a running setup is never cut off).
static void net_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(NET_POLL_MS));
        if (s_got_ip) {
            s_got_ip = false;
            sys_time_start();
            start_mdns();
        }
        net_status_t n;
        app_get_net(&n);
        int64_t now = esp_timer_get_time();
        bool forced = now < s_force_ap_until_us;
        bool grace_over = s_grace_until_us && now > s_grace_until_us;
        uint8_t clients = ap_client_count();

        if (!n.sta_configured || forced || (!n.sta_connected && grace_over)) {
            if (!s_ap_on && grace_over) {
                ESP_LOGW(TAG, "station not connected, opening the setup access point");
            }
            ap_set(true, n.sta_configured);
        } else if (n.sta_connected && clients == 0) {
            ap_set(false, true);
        }
        app_update_net(set_ap, &clients);
        app_note_stack();
    }
}

static void reconnect_cb(void *arg)
{
    // A connect attempt scans all channels, which drops phones from the setup
    // access point; wait while someone is using it.
    if (!s_sta_configured) {
        return;
    }
    if (ap_client_count() > 0) {
        esp_timer_start_once(s_reconnect_timer, (uint64_t)RECONNECT_MAX_MS * 1000);
        return;
    }
    esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            // Also fires when a scan enables the station in pure AP mode.
            if (s_sta_configured) {
                esp_wifi_connect();
            }
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            const wifi_event_sta_disconnected_t *d = data;
            if (!s_sta_configured) {
                break;
            }
            bool was = xEventGroupGetBits(app_events()) & APP_EV_STA_CONNECTED;
            xEventGroupClearBits(app_events(), APP_EV_STA_CONNECTED);
            if (was) {
                app_update_net(set_connected, NULL);
                // Lost a working connection: give it the grace period again.
                s_grace_until_us = esp_timer_get_time() + STA_GRACE_US;
            }
            ESP_LOGW(TAG, "disconnected (reason %d), retry in %lu ms", d->reason, (unsigned long)s_backoff_ms);
            esp_timer_stop(s_reconnect_timer);
            esp_timer_start_once(s_reconnect_timer, (uint64_t)s_backoff_ms * 1000);
            s_backoff_ms = s_backoff_ms * 2 > RECONNECT_MAX_MS ? RECONNECT_MAX_MS : s_backoff_ms * 2;
            wake_net_task();
            break;
        }
        case WIFI_EVENT_AP_STACONNECTED:
        case WIFI_EVENT_AP_STADISCONNECTED:
            wake_net_task();
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && (id == IP_EVENT_STA_GOT_IP || id == IP_EVENT_ETH_GOT_IP)) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_backoff_ms = RECONNECT_MIN_MS;
        s_grace_until_us = 0;
        app_update_net(set_connected, (void *)&e->ip_info);
        // Connected again: let the fetcher refresh now instead of finishing its backoff.
        xEventGroupSetBits(app_events(), APP_EV_STA_CONNECTED | APP_EV_CONFIG_CHANGED);
        s_got_ip = true;
        wake_net_task();
    }
}

void wifi_mgr_force_setup(uint32_t seconds)
{
    s_force_ap_until_us = esp_timer_get_time() + (int64_t)seconds * 1000000;
    wake_net_task();
}

#if CONFIG_APP_QEMU
// Emulator only: QEMU has no WiFi radio but an "open_eth" Ethernet adapter
// with user-mode networking, which stands in for the station interface.
static void start_qemu_ethernet(void)
{
    esp_netif_config_t ncfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&ncfg);
    esp_netif_set_hostname(netif, s_hostname);
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 100;
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth));
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_eth_new_netif_glue(eth)));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, on_event, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth));
    ESP_LOGW(TAG, "QEMU build: using emulated Ethernet instead of WiFi");
}
#endif

void wifi_mgr_start(void)
{
    EXT_RAM_BSS_ATTR static app_config_t cfg;
    app_get_config(&cfg);
    strlcpy(s_hostname, cfg.hostname, sizeof(s_hostname));
    app_update_net(set_sta, &cfg);
    s_sta_configured = cfg.wifi_ssid[0] != 0;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "Bimfo-%02X%02X", mac[4], mac[5]);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_APP_QEMU
    s_sta_configured = false;  // no WiFi at all in the emulator
    static const app_config_t qemu_net = {.wifi_ssid = "QEMU Ethernet"};
    app_update_net(set_sta, (void *)&qemu_net);
    start_qemu_ethernet();
    xTaskCreatePinnedToCore(net_task, "net", 6144, NULL, 3, &s_net_task, 0);
    return;
#endif
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    esp_netif_set_hostname(sta_netif, s_hostname);

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    // Credentials live in our config; keep the driver's own NVS copy out of it.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    const esp_timer_create_args_t rc = {.callback = reconnect_cb, .name = "wifi_rc"};
    ESP_ERROR_CHECK(esp_timer_create(&rc, &s_reconnect_timer));

    if (cfg.wifi_ssid[0]) {
        wifi_config_t sta = {0};
        strlcpy((char *)sta.sta.ssid, cfg.wifi_ssid, sizeof(sta.sta.ssid));
        strlcpy((char *)sta.sta.password, cfg.wifi_pass, sizeof(sta.sta.password));
        // Accept anything from open to WPA3; the network decides.
        sta.sta.threshold.authmode = cfg.wifi_pass[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
        sta.sta.pmf_cfg.capable = true;
        sta.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        sta.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
        s_grace_until_us = esp_timer_get_time() + STA_GRACE_US;
        ESP_LOGI(TAG, "connecting to '%s'", cfg.wifi_ssid);
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    xTaskCreatePinnedToCore(net_task, "net", 6144, NULL, 3, &s_net_task, 0);
    wake_net_task();
}

int wifi_mgr_scan(wifi_scan_item_t *out, int max)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP) {
        // Scanning needs the station interface.
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        return 0;
    }
    uint16_t n = 20;
    wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
    if (!recs || esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) {
        free(recs);
        return 0;
    }
    int count = 0;
    for (int i = 0; i < n && count < max; i++) {
        if (!recs[i].ssid[0]) continue;
        bool dup = false;
        for (int k = 0; k < count && !dup; k++) dup = strcmp(out[k].ssid, (char *)recs[i].ssid) == 0;
        if (dup) continue;  // same network on several access points
        strlcpy(out[count].ssid, (char *)recs[i].ssid, sizeof(out[count].ssid));
        out[count].rssi = recs[i].rssi;
        out[count].secure = recs[i].authmode != WIFI_AUTH_OPEN;
        count++;
    }
    free(recs);
    return count;
}
