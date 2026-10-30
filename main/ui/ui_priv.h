// Helpers shared by the UI themes.
#pragma once

#include <stddef.h>

#include "fonts/fonts.h"
#include "lvgl.h"
#include "ui.h"

typedef struct {
    uint32_t bg;            // screen background
    uint32_t fg;            // main text
    uint32_t accent;        // titles, highlights
    uint32_t muted;         // secondary text
    const lv_font_t *title_font;
    const lv_font_t *text_font;
    const lv_font_t *strong_font;
    void (*build_board)(lv_obj_t *scr);
    void (*update_board)(const ui_state_t *st, const board_t *b, int64_t now);
    // Optional own start screen; NULL for the common one.
    void (*build_boot)(lv_obj_t *scr);
    void (*update_boot)(const ui_state_t *st);
} ui_theme_t;

extern const ui_theme_t ui_theme_wl;
extern const ui_theme_t ui_theme_compact;
extern const ui_theme_t ui_theme_oebb;

// The green glow of the Wiener Linien panel (NULL if out of memory).
const lv_img_dsc_t *ui_wl_glow(void);

// Start screen of the LED styles (ui_boot_tram.c): a low-floor tram on the
// glow `bg` (or the plain screen background if NULL).
void ui_tram_boot_build(lv_obj_t *scr, const lv_img_dsc_t *bg);
void ui_tram_boot_update(const ui_state_t *st);

// Single-line label at a fixed position; `w` 0 means size to content.
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w,
                   lv_text_align_t align, lv_label_long_mode_t mode);
lv_obj_t *ui_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);
void ui_set_text(lv_obj_t *label, const char *text);
void ui_set_color(lv_obj_t *obj, uint32_t color);
void ui_set_hidden(lv_obj_t *obj, bool hidden);

// Vienna local time, "HH:MM" (or "HH:MM:SS"); "--:--" while the clock is unknown.
void ui_fmt_clock(int64_t t, bool seconds, char *buf, size_t n);
