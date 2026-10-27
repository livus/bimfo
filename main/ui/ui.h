// Display UI. Only one task may call into this module (and LVGL); everything
// else hands data over via ui_state_t / board_t snapshots. Also built on the
// host by host/render.c to produce screenshots without the device.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board.h"

#define UI_W 320
#define UI_H 172

typedef enum {
    UI_SCREEN_BOOT,     // splash with a status line
    UI_SCREEN_SETUP,    // WiFi access point + QR code
    UI_SCREEN_BOARD,    // departures
} ui_screen_t;

typedef struct {
    ui_screen_t screen;
    uint8_t theme;                      // theme_t
    bool group_lines;
    bool no_stops;                      // board screen: nothing configured yet
    uint8_t walk_min[BOARD_MAX_STOPS];  // per configured stop
    char status[96];                    // boot line, or a warning on the board ("Offline")
    char ap_ssid[33];
    char ap_pass[65];                   // empty for an open access point
    char url[80];                       // where the config page lives
} ui_state_t;

void ui_init(void);

// Brings the screen in line with `st`/`b`. Only changed widgets are touched,
// so calling it a few times per second is cheap. `now` is unix time (0 if the
// clock is not known yet).
void ui_update(const ui_state_t *st, const board_t *b, int64_t now);
