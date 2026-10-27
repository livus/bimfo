// WiFi station with automatic reconnect plus a setup access point
// ("Bimfo-XXXX", open, captive portal) that comes up when no network is
// configured, the configured one is unreachable, or setup is forced.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define WIFI_AP_IP "192.168.4.1"

void wifi_mgr_start(void);
// Keeps the access point up for `seconds` even if the station is connected.
void wifi_mgr_force_setup(uint32_t seconds);

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t secure;
} wifi_scan_item_t;

// Blocking scan (a few seconds); returns the number of networks found.
int wifi_mgr_scan(wifi_scan_item_t *out, int max);
