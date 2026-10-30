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
} ui_theme_t;

extern const ui_theme_t ui_theme_wl;
extern const ui_theme_t ui_theme_compact;
extern const ui_theme_t ui_theme_oebb;

// Single-line label at a fixed position; `w` 0 means size to content.
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w,
                   lv_text_align_t align, lv_label_long_mode_t mode);
lv_obj_t *ui_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);
void ui_set_text(lv_obj_t *label, const char *text);
void ui_set_color(lv_obj_t *obj, uint32_t color);
void ui_set_hidden(lv_obj_t *obj, bool hidden);

// Vienna local time, "HH:MM" (or "HH:MM:SS"); "--:--" while the clock is unknown.
void ui_fmt_clock(int64_t t, bool seconds, char *buf, size_t n);
