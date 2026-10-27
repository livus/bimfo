// OEBB (Scotty/HAFAS) station board and station search.
//   board:  https://fahrplan.oebb.at/bin/stboard.exe/dn?...&outputMode=tickerDataOnly
//   search: https://fahrplan.oebb.at/bin/ajax-getstop.exe/dn?...&S=<name>
// Both return a JavaScript assignment wrapping a JSON object.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_config.h"
#include "board.h"

#define OEBB_HOST "https://fahrplan.oebb.at"

typedef struct {
    int32_t eva;
    char name[48];
} oebb_station_t;

// `now` is used for the board's start time (Vienna local).
void oebb_build_board_url(const cfg_stop_t *stop, int64_t now, int max_journeys, char *buf, size_t n);
void oebb_build_search_url(const char *query, char *buf, size_t n);

// Appends departures of a station board to `out`; returns the number added or
// -1 if the payload could not be parsed. `station` (optional) receives the
// station name reported by the board.
int oebb_parse_board(const char *payload, const cfg_stop_t *stop, uint8_t stop_idx,
                     board_t *out, char *station, size_t station_len);

int oebb_parse_search(const char *payload, oebb_station_t *out, int max);

// "Tram 52" -> "52", "Bus 13A" -> "13A", "S 50" stays.
void oebb_short_line(const char *product, char *out, size_t n);
uint8_t oebb_vehicle_type(const char *product);
