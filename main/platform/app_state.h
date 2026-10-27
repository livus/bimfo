// Shared application state. Tasks never share pointers into it: every access
// copies in or out under one mutex, so the UI, fetcher and web server can run
// concurrently without locking each other for long.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "board.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Event bits (app_events())
#define APP_EV_STA_CONNECTED    BIT0    // station has an IP address
#define APP_EV_CONFIG_CHANGED   BIT1    // fetcher should reload and refetch now
#define APP_EV_TIME_SYNCED      BIT2

typedef struct {
    bool sta_connected;
    bool sta_configured;
    char sta_ssid[33];
    char ip[16];
    int8_t rssi;
    bool ap_active;
    char ap_ssid[33];
    uint8_t ap_clients;
    uint32_t disconnects;
} net_status_t;

typedef struct {
    int64_t last_ok;            // unix time of last fully successful fetch
    int64_t last_attempt;
    uint32_t ok_count;
    uint32_t error_count;
    uint8_t consecutive_errors;
    char last_error[96];
} fetch_status_t;

void app_state_init(void);
EventGroupHandle_t app_events(void);

void app_get_config(app_config_t *out);
// Stores (NVS) and publishes a new configuration; wakes the fetcher.
esp_err_t app_set_config(const app_config_t *cfg);

void app_get_board(board_t *out, uint32_t *generation);
uint32_t app_board_generation(void);
void app_set_board(const board_t *b);

void app_get_net(net_status_t *out);
void app_update_net(void (*fn)(net_status_t *net, void *ctx), void *ctx);

void app_get_fetch(fetch_status_t *out);
void app_set_fetch(const fetch_status_t *f);

// Records the calling task's stack high-water mark (bytes never used) under
// its FreeRTOS task name; app_stack_report() lists the minimum seen per task.
void app_note_stack(void);
int app_stack_report(char names[][16], uint32_t *free_bytes, int max);

// Unix time if the clock is set (SNTP or API server time), else 0.
int64_t app_now(void);
void app_set_time_from_server(int64_t unix_time);
