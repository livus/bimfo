// Wiener Linien OGD realtime API ("monitor" endpoint) and stop list helpers.
// https://www.wienerlinien.at/ogd_realtime/monitor?stopId=...|diva=...
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_config.h"
#include "board.h"

#define WL_HOST "https://www.wienerlinien.at"
#define WL_STATIONS_CSV_URL WL_HOST "/ogd_realtime/doku/ogd/wienerlinien-ogd-haltestellen.csv"
#define WL_MAX_PER_LINE 4   // departures kept per line and direction

typedef enum {
    WL_OK = 0,
    WL_ERR_JSON,        // not JSON / unexpected structure
    WL_ERR_API,         // API reported an error (message.messageCode != 1)
} wl_status_t;

typedef struct {
    char line[DEP_LINE_LEN];
    char towards[DEP_TOWARDS_LEN];
    char dir;
    int32_t rbl;
    uint8_t type;
} wl_line_info_t;

typedef struct {
    int32_t diva;
    char name[48];
} wl_station_t;

// Builds the monitor URL for a configured stop: the selected platforms (RBL)
// when lines were picked, otherwise the whole station (DIVA).
void wl_build_url(const cfg_stop_t *stop, char *buf, size_t n);

// Appends the departures matching the stop's line selection to `out`.
// `server_time` (optional) receives message.serverTime.
wl_status_t wl_parse_monitor(const char *json, const cfg_stop_t *stop, uint8_t stop_idx,
                             board_t *out, int64_t *server_time, int *api_code);

// Lists the distinct line/direction/platform combinations of a monitor
// response (used by the config page to offer choices).
int wl_list_lines(const char *json, wl_line_info_t *out, int max);

// Searches the "haltestellen" CSV (DIVA;PlatformText;...) for `query`
// (accent/case-insensitive). Prefix matches rank first.
int wl_search_stations(const char *csv, const char *query, wl_station_t *out, int max);

uint8_t wl_vehicle_type(const char *type);
