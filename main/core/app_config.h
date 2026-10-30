// Device configuration: what the web page edits and NVS stores (as JSON).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"

#define CFG_MAX_STOPS   BOARD_MAX_STOPS
#define CFG_MAX_SEL     8

typedef enum { STOP_WL = 0, STOP_OEBB = 1 } stop_kind_t;
typedef enum { THEME_WL = 0, THEME_COMPACT = 1, THEME_OEBB = 2, THEME_COUNT } theme_t;

// One selected line at a stop. Wiener Linien: line + direction + platform
// (RBL number). OEBB: only `line` is used, as a filter.
typedef struct {
    char line[DEP_LINE_LEN];
    char dir;           // 'H', 'R' or 0 for any direction
    int32_t rbl;        // Wiener Linien platform id ("stopId"), 0 if unknown
} cfg_sel_t;

typedef struct {
    uint8_t kind;               // stop_kind_t
    char name[48];
    int32_t id;                 // Wiener Linien DIVA or OEBB EVA number
    uint8_t walk_min;           // hide departures you cannot reach anymore
    bool trains_only;           // OEBB: skip trams, buses and U-Bahn
    uint8_t nsel;               // 0 = show every line
    cfg_sel_t sel[CFG_MAX_SEL];
} cfg_stop_t;

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char hostname[32];
    uint8_t theme;              // theme_t
    bool group_lines;           // one row per line+direction with the next two times
    bool wl_notice;             // Wiener Linien style: show traffic info (the real displays do not)
    bool wl_paging;             // Wiener Linien style: page through more lines than fit
    bool wl_step_free;          // Wiener Linien style: alternate with the next low-floor tram
    uint8_t brightness;         // 0..100
    uint8_t night_brightness;   // 0..100
    uint16_t night_start;       // minutes after midnight; start == end disables
    uint16_t night_end;
    uint16_t refresh_s;
    bool flip;                  // rotate the display by 180 degrees
    bool auto_update;           // install new releases at night (off by default)
    uint8_t nstops;
    cfg_stop_t stops[CFG_MAX_STOPS];
} app_config_t;

void cfg_defaults(app_config_t *c);

// Applies the fields present in `json` on top of `c`. A missing or empty
// "pass" keeps the stored WiFi password. Returns false (and an error message)
// if the JSON is malformed; `c` is left untouched in that case.
bool cfg_from_json(app_config_t *c, const char *json, char *err, size_t errlen);

// Serialises to a malloc'd string (caller frees). Without secrets the WiFi
// password is replaced by "has_pass": true/false.
char *cfg_to_json(const app_config_t *c, bool include_secrets);

bool cfg_stop_accepts(const cfg_stop_t *s, const char *line, char dir);

// Backlight level (0..100) for unix time `now`: night_brightness inside the
// night window (Vienna local time, may span midnight), else brightness.
int cfg_brightness(const app_config_t *c, int64_t now);

// Board header: the stop name, or all names joined with " · ".
void cfg_board_title(const app_config_t *c, char *buf, size_t n);

const char *cfg_theme_name(uint8_t theme);
