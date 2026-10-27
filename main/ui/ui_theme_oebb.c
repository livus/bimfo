// OEBB look, after the station monitors (meine.oebb.at/abfahrtankunft webdisplay
// template): white Arial-style text on alternating blue rows, yellow headline
// and expected times, white notice strip with black text at the bottom.
#include <stdio.h>
#include <string.h>

#include "ui_priv.h"

#define OEBB_BG         0x000094
#define OEBB_ROW_ALT    0x0000CE
#define OEBB_YELLOW     0xFFFF00
#define OEBB_WHITE      0xFFFFFF
#define OEBB_MUTED      0xB4B4E6

#define HEAD_H      24
#define COLS_Y      24
#define COLS_H      16
#define ROWS        5
#define ROW_Y0      (COLS_Y + COLS_H)
#define ROW_H       26
#define NOTICE_H    22
#define TEXT_DY     ((ROW_H - 18) / 2)

#define X_TIME      4
#define W_TIME      42
#define X_EXP       50
#define W_EXP       46
#define X_LINE      99
#define W_LINE      60
#define X_DEST      162
#define W_TRACK     28
#define X_TRACK     (UI_W - 4 - W_TRACK)
#define W_DEST      (X_TRACK - 4 - X_DEST)

typedef struct {
    lv_obj_t *bg, *time, *exp, *line, *dest, *track;
} oebb_row_t;

static struct {
    lv_obj_t *title, *clock, *msg, *hint, *notice_bg, *notice;
    lv_obj_t *cols[5];   // column headers, hidden without departures
    oebb_row_t rows[ROWS];
} s;

static void build(lv_obj_t *scr)
{
    lv_obj_t *l = ui_label(scr, &font_sans_bold_16, OEBB_YELLOW, 4, 3, 0, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_CLIP);
    lv_label_set_text(l, "Abfahrt");
    s.title = ui_label(scr, &font_sans_italic_12, OEBB_YELLOW, 72, 6, 168, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    s.clock = ui_label(scr, &font_sans_bold_16, OEBB_WHITE, UI_W - 4 - 72, 3, 72, LV_TEXT_ALIGN_RIGHT,
                       LV_LABEL_LONG_CLIP);

    static const struct {
        const char *text;
        int x, w;
        lv_text_align_t align;
    } cols[] = {
        {"Zeit", X_TIME, W_TIME, LV_TEXT_ALIGN_LEFT},
        {"Erwartet", X_EXP, W_EXP + 2, LV_TEXT_ALIGN_LEFT},
        {"Zug", X_LINE, W_LINE, LV_TEXT_ALIGN_LEFT},
        {"Nach", X_DEST, W_DEST, LV_TEXT_ALIGN_LEFT},
        {"Steig", X_TRACK - 8, W_TRACK + 8, LV_TEXT_ALIGN_RIGHT},
    };
    for (size_t i = 0; i < sizeof(cols) / sizeof(cols[0]); i++) {
        s.cols[i] = ui_label(scr, &font_sans_12, OEBB_WHITE, cols[i].x, COLS_Y + 1, cols[i].w, cols[i].align,
                             LV_LABEL_LONG_CLIP);
        lv_label_set_text(s.cols[i], cols[i].text);
    }

    for (int i = 0; i < ROWS; i++) {
        int y = ROW_Y0 + i * ROW_H;
        oebb_row_t *r = &s.rows[i];
        r->bg = ui_rect(scr, 0, y, UI_W, ROW_H, i % 2 == 0 ? OEBB_ROW_ALT : OEBB_BG);
        y = TEXT_DY;
        r->time = ui_label(r->bg, &font_sans_16, OEBB_WHITE, X_TIME, y, W_TIME, LV_TEXT_ALIGN_LEFT,
                           LV_LABEL_LONG_CLIP);
        r->exp = ui_label(r->bg, &font_sans_16, OEBB_YELLOW, X_EXP, y, W_EXP, LV_TEXT_ALIGN_LEFT,
                          LV_LABEL_LONG_CLIP);
        r->line = ui_label(r->bg, &font_sans_bold_16, OEBB_WHITE, X_LINE, y, W_LINE, LV_TEXT_ALIGN_LEFT,
                           LV_LABEL_LONG_CLIP);
        r->dest = ui_label(r->bg, &font_sans_16, OEBB_WHITE, X_DEST, y, W_DEST, LV_TEXT_ALIGN_LEFT,
                           LV_LABEL_LONG_DOT);
        r->track = ui_label(r->bg, &font_sans_16, OEBB_WHITE, X_TRACK, y, W_TRACK, LV_TEXT_ALIGN_RIGHT,
                            LV_LABEL_LONG_CLIP);
    }

    s.msg = ui_label(scr, &font_sans_16, OEBB_WHITE, 8, ROW_Y0 + ROW_H, UI_W - 16, LV_TEXT_ALIGN_CENTER,
                     LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s.msg, 2 * 19);
    s.hint = ui_label(scr, &font_sans_14, OEBB_MUTED, 8, ROW_Y0 + ROW_H * 3, UI_W - 16, LV_TEXT_ALIGN_CENTER,
                      LV_LABEL_LONG_DOT);
    s.notice_bg = ui_rect(scr, 0, UI_H - NOTICE_H, UI_W, NOTICE_H, OEBB_WHITE);
    s.notice = ui_label(s.notice_bg, &font_sans_14, 0x000000, 4, (NOTICE_H - 16) / 2, UI_W - 8,
                        LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_SCROLL_CIRCULAR);
}

static int64_t sched(const departure_t *d)
{
    return d->planned ? d->planned : dep_time(d);
}

// OEBB boards are ordered by scheduled time: the rows picked by "departs
// soonest" are shown in timetable order (a late train keeps its slot).
static void sort_by_schedule(board_row_t *rows, int n)
{
    for (int i = 1; i < n; i++) {
        board_row_t t = rows[i];
        int k = i - 1;
        while (k >= 0 && sched(rows[k].dep) > sched(t.dep)) {
            rows[k + 1] = rows[k];
            k--;
        }
        rows[k + 1] = t;
    }
}

static void update(const ui_state_t *st, const board_t *b, int64_t now)
{
    char buf[64];
    ui_set_text(s.title, st->status[0] ? st->status : b->title);
    ui_fmt_clock(now, true, buf, sizeof(buf));
    ui_set_text(s.clock, buf);

    bool notice = b->notice[0] != 0;
    int max_rows = notice ? ROWS - 1 : ROWS;
    board_row_t rows[ROWS];
    int n = st->no_stops ? 0 : board_rows(b, now, false, st->walk_min, rows, max_rows);
    sort_by_schedule(rows, n);
    for (size_t i = 0; i < sizeof(s.cols) / sizeof(s.cols[0]); i++) {
        ui_set_hidden(s.cols[i], n == 0);
    }
    // Cancelled trains alternate between destination and "Ausfall" (3 s each).
    bool show_cancel = (now / 3) % 2 == 1;

    for (int i = 0; i < ROWS; i++) {
        oebb_row_t *r = &s.rows[i];
        ui_set_hidden(r->bg, i >= n);
        if (i >= n) {
            continue;
        }
        const departure_t *d = rows[i].dep;
        bool cancelled = d->flags & DEP_F_CANCELLED;
        ui_fmt_clock(sched(d), false, buf, sizeof(buf));
        ui_set_text(r->time, buf);
        if (cancelled) {
            ui_set_text(r->exp, "");
        } else if (dep_is_delayed(d)) {
            ui_fmt_clock(d->expected, false, buf, sizeof(buf));
            ui_set_text(r->exp, buf);
        } else {
            ui_set_text(r->exp, "");
        }
        ui_set_text(r->line, d->line);
        // Long train names ("RJX 162") get the narrower font instead of clipping.
        lv_point_t size;
        lv_txt_get_size(&size, d->line, &font_sans_bold_16, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        const lv_font_t *lf = size.x > W_LINE - 2 ? &font_sans_bold_14 : &font_sans_bold_16;
        if (lv_obj_get_style_text_font(r->line, 0) != lf) {
            lv_obj_set_style_text_font(r->line, lf, 0);
            lv_obj_set_y(r->line, TEXT_DY + (lf == &font_sans_bold_14 ? 2 : 0));
        }
        ui_set_text(r->dest, cancelled && show_cancel ? "Ausfall" : d->towards);
        ui_set_color(r->dest, cancelled ? OEBB_YELLOW : OEBB_WHITE);
        ui_set_text(r->track, d->platform);
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

    ui_set_text(s.notice, notice ? b->notice : "");
    ui_set_hidden(s.notice_bg, !notice);
}

const ui_theme_t ui_theme_oebb = {
    .bg = OEBB_BG,
    .fg = OEBB_WHITE,
    .accent = OEBB_YELLOW,
    .muted = OEBB_MUTED,
    .title_font = &font_sans_bold_22,
    .text_font = &font_sans_16,
    .strong_font = &font_sans_bold_16,
    .build_board = build,
    .update_board = update,
};
