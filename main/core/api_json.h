// JSON representations served by the device's web API (main/www/index.html
// and host/mock_server.py rely on these shapes).
#pragma once

#include "board.h"
#include "cJSON.h"
#include "oebb_parse.h"
#include "wl_parse.h"

// {"title","notice","fetched_at","now","departures":[{line,towards,platform,dir,
//  type,minutes,planned,expected,delayed,cancelled,barrier_free,realtime,stop}]}
cJSON *api_board_json(const board_t *b, int64_t now);

// [{line,towards,dir,rbl,type}] sorted by vehicle type, line, direction.
cJSON *api_wl_lines_json(wl_line_info_t *lines, int n);
cJSON *api_wl_stations_json(const wl_station_t *st, int n);
cJSON *api_oebb_stations_json(const oebb_station_t *st, int n);

const char *api_vehicle_type_name(uint8_t type);
