// Wiener Linien look: yellow-amber LED dot-matrix like the countdown displays at
// tram and bus stops, on the faint glow of their backlit panel, which is made of
// three modules with dark seams between them. Rows: line | destination | minutes,
// with a blinking star when the vehicle is due and a wheelchair for low-floor vehicles.
#include <stdio.h>
#include <string.h>

#include "ui_priv.h"

#define WL_BG       0x17150A    // glow of the panel between the LEDs
#define WL_SEAM     0x000000    // gaps between the display modules
#define WL_AMBER    0xFFD02A    // the LEDs are yellower than they look in memory
#define WL_DIM      0x9C7F1A

#define MODULES     3
#define SEAM_W      2

#define ROWS        5
#define ROW_Y0      22
#define ROW_H       30
#define TEXT_DY     ((ROW_H - 16) / 2)

#define X_LINE      4
#define W_LINE_MIN  36          // three LED digits
#define W_LINE_MAX  66
#define W_MIN       28          // one minutes column ("12" or "*")
#define W_TIME      62          // "12:57" for departures beyond 99 minutes
#define X_MIN2      (UI_W - 4 - W_MIN)
#define X_MIN1      (X_MIN2 - 6 - W_MIN)
#define X_WHEEL     (X_MIN1 - 16)

typedef struct {
    lv_obj_t *line, *dest, *wheel, *min1, *min2;
} wl_row_t;

static struct {
    lv_obj_t *title, *clock, *msg, *hint, *ticker;
    wl_row_t rows[ROWS];
    int dest_x;
} s;

static void build(lv_obj_t *scr)
{
    s.title = ui_label(scr, &font_led_16, WL_DIM, 4, 2, 236, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    s.clock = ui_label(scr, &font_led_16, WL_DIM, UI_W - 4 - 64, 2, 64, LV_TEXT_ALIGN_RIGHT, LV_LABEL_LONG_CLIP);
    s.dest_x = X_LINE + W_LINE_MIN + 8;
    for (int i = 0; i < ROWS; i++) {
        int y = ROW_Y0 + i * ROW_H + TEXT_DY;
        wl_row_t *r = &s.rows[i];
        r->line = ui_label(scr, &font_led_16, WL_AMBER, X_LINE, y, W_LINE_MAX, LV_TEXT_ALIGN_LEFT,
                           LV_LABEL_LONG_CLIP);
        r->dest = ui_label(scr, &font_led_16, WL_AMBER, s.dest_x, y, X_WHEEL - 4 - s.dest_x, LV_TEXT_ALIGN_LEFT,
                           LV_LABEL_LONG_SCROLL_CIRCULAR);
        r->wheel = ui_label(scr, &font_led_16, WL_AMBER, X_WHEEL, y, 14, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_CLIP);
        r->min1 = ui_label(scr, &font_led_16, WL_AMBER, X_MIN1, y, W_MIN, LV_TEXT_ALIGN_RIGHT, LV_LABEL_LONG_CLIP);
        // Wide enough for a clock time; empty space is transparent.
        r->min2 = ui_label(scr, &font_led_16, WL_AMBER, UI_W - 4 - W_TIME, y, W_TIME, LV_TEXT_ALIGN_RIGHT,
                           LV_LABEL_LONG_CLIP);
    }
    s.msg = ui_label(scr, &font_led_16, WL_AMBER, 4, ROW_Y0 + ROW_H, UI_W - 8, LV_TEXT_ALIGN_CENTER,
                     LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s.msg, 16 * 2 + 4);
    s.hint = ui_label(scr, &font_sans_14, WL_DIM, 4, ROW_Y0 + ROW_H * 3, UI_W - 8, LV_TEXT_ALIGN_CENTER,
                      LV_LABEL_LONG_DOT);
    s.ticker = ui_label(scr, &font_led_16, WL_AMBER, 4, ROW_Y0 + (ROWS - 1) * ROW_H + TEXT_DY, UI_W - 8,
                        LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // Seams between the modules, created last so they cross the text, as on the real display.
    for (int i = 1; i < MODULES; i++) {
        ui_rect(scr, UI_W * i / MODULES - SEAM_W / 2, 0, SEAM_W, UI_H, WL_SEAM);
    }
}

static void fmt_minutes(const departure_t *d, int64_t now, bool blink_on, char *buf, size_t n)
{
    int m = dep_minutes(d, now);
    if (m <= 0) {
        snprintf(buf, n, "%s", blink_on ? "*" : "");
    } else if (m > 99 && now > 0) {
        ui_fmt_clock(dep_time(d), false, buf, n);
    } else {
        snprintf(buf, n, "%d", m > 99 ? 99 : m);
    }
}

static void set_row_hidden(wl_row_t *r, bool hidden)
{
    ui_set_hidden(r->line, hidden);
    ui_set_hidden(r->dest, hidden);
    ui_set_hidden(r->wheel, hidden);
    ui_set_hidden(r->min1, hidden);
    ui_set_hidden(r->min2, hidden);
}

// The line column is as wide as the widest visible line name ("6" ... "VAB 1").
static void layout_columns(const board_row_t *rows, int n)
{
    lv_coord_t widest = W_LINE_MIN;
    for (int i = 0; i < n; i++) {
        lv_point_t size;
        lv_txt_get_size(&size, rows[i].dep->line, &font_led_16, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > widest) widest = size.x;
    }
    if (widest > W_LINE_MAX) widest = W_LINE_MAX;
    int dest_x = X_LINE + widest + 8;
    if (dest_x == s.dest_x) {
        return;
    }
    s.dest_x = dest_x;
    for (int i = 0; i < ROWS; i++) {
        lv_obj_set_x(s.rows[i].dest, dest_x);
        lv_obj_set_width(s.rows[i].dest, X_WHEEL - 4 - dest_x);
    }
}

static void update(const ui_state_t *st, const board_t *b, int64_t now)
{
    char buf[64];
    bool blink_on = (now & 1) == 0;

    ui_set_text(s.title, st->status[0] ? st->status : b->title);
    ui_set_color(s.title, st->status[0] ? WL_AMBER : WL_DIM);
    ui_fmt_clock(now, false, buf, sizeof(buf));
    ui_set_text(s.clock, buf);

    bool ticker = b->notice[0] != 0;
    int max_rows = ticker ? ROWS - 1 : ROWS;
    board_row_t rows[ROWS];
    int n = st->no_stops ? 0 : board_rows(b, now, st->group_lines, st->walk_min, rows, max_rows);
    layout_columns(rows, n);

    for (int i = 0; i < ROWS; i++) {
        wl_row_t *r = &s.rows[i];
        if (i >= n) {
            set_row_hidden(r, true);
            continue;
        }
        set_row_hidden(r, false);
        const departure_t *d = rows[i].dep;
        ui_set_text(r->line, d->line);
        ui_set_text(r->dest, d->towards);
        // U-Bahn and trains are always step-free; mark low-floor trams and buses.
        bool wheel = (d->flags & DEP_F_BARRIER_FREE) && (d->type == VT_TRAM || d->type == VT_BUS ||
                                                         d->type == VT_NIGHTBUS);
        ui_set_text(r->wheel, wheel ? LED_SYMBOL_WHEELCHAIR : "");

        fmt_minutes(d, now, blink_on, buf, sizeof(buf));
        bool far = strchr(buf, ':') != NULL;
        if (st->group_lines && !far) {
            ui_set_text(r->min1, buf);
            int next = rows[i].next_minutes;
            if (next >= 0 && next <= 99) {
                snprintf(buf, sizeof(buf), "%d", next);
            } else {
                buf[0] = 0;
            }
            ui_set_text(r->min2, buf);
            ui_set_color(r->min2, WL_DIM);
        } else {
            ui_set_text(r->min1, "");
            ui_set_text(r->min2, buf);
            ui_set_color(r->min2, WL_AMBER);
        }
    }

    const char *msg = "", *hint = "";
    if (st->no_stops) {
        msg = "Keine Haltestelle eingerichtet";
        hint = st->url;
    } else if (n == 0) {
        msg = b->fetched_at ? "Derzeit keine Abfahrten" : "Lade Abfahrten ...";
    }
    ui_set_text(s.msg, msg);
    ui_set_hidden(s.msg, !msg[0]);
    ui_set_text(s.hint, hint);
    ui_set_hidden(s.hint, !hint[0]);

    ui_set_text(s.ticker, ticker ? b->notice : "");
    ui_set_hidden(s.ticker, !ticker);
}

const ui_theme_t ui_theme_wl = {
    .bg = WL_BG,
    .fg = WL_AMBER,
    .accent = WL_AMBER,
    .muted = WL_DIM,
    .title_font = &font_led_24,
    .text_font = &font_led_16,
    .strong_font = &font_led_16,
    .build_board = build,
    .update_board = update,
};
