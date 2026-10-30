// Wiener Linien look: the LED countdown displays at tram and bus stops (see
// docs/research/wl_display). Three entries of two text rows each: the line in
// big digits (a trailing letter small, as in "13A"), a wheelchair for low-floor
// trams, the destination wrapped onto both rows with U-Bahn/S-Bahn symbols and
// the minutes in big digits - with a leading zero when the time is only from
// the timetable, and a square jumping between the bottom corners when the
// vehicle leaves now. More lines than fit are shown page by page. When the next
// tram is not low-floor, it takes turns with the next one that is, whose
// wheelchair then blinks. Without departures the display asks to read the
// printed timetable. The panel glows green behind the LEDs and is made of three
// modules with dark seams between them.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "textutil.h"
#include "ui_priv.h"

#define WL_AMBER        0xFFD02A
#define WL_GLOW_CENTRE  0x1E4426    // the backlit panel is brightest in the middle
#define WL_GLOW_EDGE    0x0A170D
#define WL_SEAM         0x030603    // gaps between the display modules

// Geometry in LEDs. One LED is 2x2 pixels, so the panel is 160 x 86 LEDs.
#define LED             2
#define MODULES         3
#define ENTRIES         3
#define ENTRY_Y0        4           // centres the entries vertically
#define ENTRY_H         28
#define ROW2_DY         11          // second text row; the big digits span both (20 LEDs)
#define X_LINE          1
#define LINE_W          30          // three big digits
#define X_DEST          33
#define X_MIN           136         // minutes, right-aligned up to LED 158
#define MIN_W           22
#define DEST_W          (X_MIN - 3 - X_DEST)
#define TICKER_DY       5

#define PAGE_S          10          // seconds per page when more lines wait than fit
#define STEP_FREE_S     20          // seconds for the next tram, then for the next low-floor one

// The "now" square is one text row high and jumps, once per second, between
// cells of a 2x2 grid over the minutes column: bottom left, top right.
static const struct {
    uint8_t x, dy;
} NOW_POS[] = {{X_MIN + 2, ROW2_DY}, {X_MIN + 12, 0}};

typedef struct {
    lv_obj_t *line, *suffix, *line1, *line2, *wheel, *dest1, *dest2, *min, *now;
} wl_entry_t;

static struct {
    wl_entry_t entries[ENTRIES];
    lv_obj_t *msg1, *msg2, *hint, *ticker;
} s;

static lv_img_dsc_t s_glow;

static int entry_y(int i)
{
    return (ENTRY_Y0 + i * ENTRY_H) * LED;
}

static int text_w(const char *text, const lv_font_t *font)
{
    lv_point_t size;
    lv_txt_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

static uint8_t dither(float v, int step, float t)
{
    int q = (int)(v / step + t) * step;
    return (uint8_t)(q > 255 ? 255 : q);
}

// The glow of the panel, computed once (RGB565 with ordered dithering against
// banding). Allocated for good: it lives as long as the program.
const lv_img_dsc_t *ui_wl_glow(void)
{
    if (s_glow.data) {
        return &s_glow;
    }
    lv_color_t *px = malloc(UI_W * UI_H * sizeof(lv_color_t));
    if (!px) {
        return NULL;
    }
    static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    const float cr = WL_GLOW_CENTRE >> 16, cg = WL_GLOW_CENTRE >> 8 & 0xFF, cb = WL_GLOW_CENTRE & 0xFF;
    const float er = WL_GLOW_EDGE >> 16, eg = WL_GLOW_EDGE >> 8 & 0xFF, eb = WL_GLOW_EDGE & 0xFF;
    for (int y = 0; y < UI_H; y++) {
        for (int x = 0; x < UI_W; x++) {
            float dx = (x - (UI_W - 1) / 2.0f) / (UI_W / 2.0f);
            float dy = (y - (UI_H - 1) / 2.0f) / (UI_H / 2.0f);
            // 1 in the middle, 0 in the corners
            float g = (1.0f / (1.0f + 1.5f * (dx * dx + dy * dy)) - 0.25f) / 0.75f;
            float t = (bayer[y & 3][x & 3] + 0.5f) / 16.0f;
            px[y * UI_W + x] = lv_color_make(dither(er + (cr - er) * g, 8, t), dither(eg + (cg - eg) * g, 4, t),
                                             dither(eb + (cb - eb) * g, 8, t));
        }
    }
    s_glow.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_glow.header.w = UI_W;
    s_glow.header.h = UI_H;
    s_glow.data_size = UI_W * UI_H * sizeof(lv_color_t);
    s_glow.data = (const uint8_t *)px;
    return &s_glow;
}

static lv_obj_t *text_label(lv_obj_t *scr, int x, int y, int w, lv_label_long_mode_t mode)
{
    return ui_label(scr, &font_wl_text, WL_AMBER, x * LED, y, w * LED, LV_TEXT_ALIGN_LEFT, mode);
}

static void build(lv_obj_t *scr)
{
    const lv_img_dsc_t *bg = ui_wl_glow();
    if (bg) {
        lv_obj_t *img = lv_img_create(scr);
        lv_img_set_src(img, bg);
        lv_obj_set_pos(img, 0, 0);
    }
    for (int i = 0; i < ENTRIES; i++) {
        wl_entry_t *e = &s.entries[i];
        int y = entry_y(i), y2 = y + ROW2_DY * LED;
        e->line = ui_label(scr, &font_wl_big, WL_AMBER, X_LINE * LED, y, LINE_W * LED, LV_TEXT_ALIGN_LEFT,
                           LV_LABEL_LONG_CLIP);
        e->suffix = text_label(scr, X_LINE, y2, 0, LV_LABEL_LONG_CLIP);
        e->line1 = text_label(scr, X_LINE, y, LINE_W, LV_LABEL_LONG_CLIP);
        e->line2 = text_label(scr, X_LINE, y2, LINE_W, LV_LABEL_LONG_CLIP);
        e->wheel = text_label(scr, X_LINE, y2, 0, LV_LABEL_LONG_CLIP);
        lv_label_set_text(e->wheel, LED_SYMBOL_WHEELCHAIR);
        e->dest1 = text_label(scr, X_DEST, y, DEST_W, LV_LABEL_LONG_CLIP);
        e->dest2 = text_label(scr, X_DEST, y2, DEST_W, LV_LABEL_LONG_CLIP);
        e->min = ui_label(scr, &font_wl_big, WL_AMBER, X_MIN * LED, y, MIN_W * LED, LV_TEXT_ALIGN_RIGHT,
                          LV_LABEL_LONG_CLIP);
        e->now = text_label(scr, NOW_POS[0].x, y + NOW_POS[0].dy * LED, 0, LV_LABEL_LONG_CLIP);
        lv_label_set_text(e->now, WL_SYMBOL_NOW);
    }
    s.msg1 = text_label(scr, X_DEST, entry_y(0), 160 - X_DEST, LV_LABEL_LONG_CLIP);
    s.msg2 = text_label(scr, X_DEST, entry_y(0) + ROW2_DY * LED, 160 - X_DEST, LV_LABEL_LONG_CLIP);
    s.hint = text_label(scr, X_LINE, entry_y(1), 160 - 2 * X_LINE, LV_LABEL_LONG_SCROLL_CIRCULAR);
    s.ticker = text_label(scr, X_LINE, entry_y(ENTRIES - 1) + TICKER_DY * LED, 160 - 2 * X_LINE,
                          LV_LABEL_LONG_SCROLL_CIRCULAR);
    // Seams between the modules, created last so they cross the text, as on the real display.
    for (int i = 1; i < MODULES; i++) {
        ui_rect(scr, 160 * i / MODULES * LED, 0, LED, UI_H, WL_SEAM);
    }
}

static void set_x(lv_obj_t *obj, int x)
{
    if (lv_obj_get_x(obj) != x) {
        lv_obj_set_x(obj, x);
    }
}

// The wheelchair and the square are shown by show_entry().
static void set_entry_hidden(wl_entry_t *e, bool hidden)
{
    lv_obj_t *objs[] = {e->line, e->suffix, e->line1, e->line2, e->dest1, e->dest2, e->min};
    for (size_t i = 0; i < sizeof(objs) / sizeof(objs[0]); i++) {
        ui_set_hidden(objs[i], hidden);
    }
    if (hidden) {
        ui_set_hidden(e->wheel, true);
        ui_set_hidden(e->now, true);
    }
}

// Big digits for the line, a trailing letter small ("13A" -> "13" + "A").
// False if the name needs characters the big face lacks or is too wide
// ("VAB 1", "RJX 162"); spaces are dropped ("S 45" -> "S45").
static bool split_line(const char *line, char *big, char *suffix, size_t n)
{
    size_t len = 0;
    for (const char *p = line; *p && len + 1 < n; p++) {
        if (*p == ' ') continue;
        if (!((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z'))) return false;
        big[len++] = *p;
    }
    big[len] = 0;
    size_t cut = len;
    while (cut > 0 && big[cut - 1] >= 'A' && big[cut - 1] <= 'Z') cut--;
    bool has_digit = false;
    for (size_t i = 0; i < cut; i++) has_digit |= big[i] >= '0' && big[i] <= '9';
    if (!has_digit || cut == 0) cut = len;
    strcpy(suffix, big + cut);
    big[cut] = 0;
    int w = text_w(big, &font_wl_big) + (suffix[0] ? text_w(suffix, &font_wl_text) : 0);
    return len > 0 && w <= LINE_W * LED;
}

// Returns the width the line takes, in pixels.
static int show_line(wl_entry_t *e, const char *line)
{
    char big[DEP_LINE_LEN], suffix[DEP_LINE_LEN];
    if (split_line(line, big, suffix, sizeof(big))) {
        int w = text_w(big, &font_wl_big);
        ui_set_text(e->line, big);
        ui_set_text(e->suffix, suffix);
        set_x(e->suffix, X_LINE * LED + w);
        ui_set_text(e->line1, "");
        ui_set_text(e->line2, "");
        return w + (suffix[0] ? text_w(suffix, &font_wl_text) : 0);
    }
    // Long names (trains, airport buses) in the text face, split at the first space.
    char row1[DEP_LINE_LEN];
    tx_copy(row1, line, sizeof(row1));
    char *sp = strchr(row1, ' ');
    const char *row2 = "";
    if (sp) {
        *sp = 0;
        row2 = line + (sp - row1) + 1;
    }
    ui_set_text(e->line, "");
    ui_set_text(e->suffix, "");
    ui_set_text(e->line1, row1);
    ui_set_text(e->line2, row2);
    return LINE_W * LED;
}

// The destination on one row if it fits, else split at a space (or after a
// hyphen) so that both rows fit, the most even split winning; the U-/S-Bahn
// symbols stay with the last word. Returns false if a row is still too wide.
static bool wrap(const char *name, const char *syms, char *row1, char *row2, size_t n)
{
    char full[DEP_TOWARDS_LEN + 24];
    snprintf(full, sizeof(full), "%s%s", name, syms);
    row2[0] = 0;
    tx_copy(row1, full, n);
    if (text_w(full, &font_wl_text) <= DEST_W * LED) {
        return true;
    }
    int best = -1, best_score = 0;
    for (int i = 1; name[i]; i++) {
        bool space = name[i] == ' ';
        if (!space && name[i - 1] != '-') continue;
        char a[DEP_TOWARDS_LEN], b[sizeof(full)];
        memcpy(a, name, i);
        a[i] = 0;
        snprintf(b, sizeof(b), "%s%s", name + i + space, syms);
        int wa = text_w(a, &font_wl_text), wb = text_w(b, &font_wl_text);
        if (wa > DEST_W * LED) break;   // later splits only get longer
        // Both rows fitting beats everything, then spaces beat hyphens.
        int score = (wb <= DEST_W * LED ? 0 : 1 << 20) + (space ? 0 : 1 << 16) + (wa > wb ? wa : wb);
        if (best < 0 || score < best_score) {
            best = i;
            best_score = score;
        }
    }
    if (best < 0) {
        return false;
    }
    tx_copy(row1, name, (size_t)best + 1 < n ? (size_t)best + 1 : n);
    snprintf(row2, n, "%s%s", name + best + (name[best] == ' '), syms);
    return best_score < 1 << 20;
}

// Shortened forms for destinations that do not fit, as on the real displays
// ("Wolkersbergenstr.").
static const struct {
    const char *full, *abbr;
} ABBREVIATIONS[] = {
    {"stra\xC3\x9F" "e", "str."}, {"Stra\xC3\x9F" "e", "Str."}, {"gasse", "g."}, {"Gasse", "G."},
};

static bool abbreviate(char *name)
{
    bool changed = false;
    for (size_t k = 0; k < sizeof(ABBREVIATIONS) / sizeof(ABBREVIATIONS[0]); k++) {
        size_t lf = strlen(ABBREVIATIONS[k].full), la = strlen(ABBREVIATIONS[k].abbr);
        char *p;
        while ((p = strstr(name, ABBREVIATIONS[k].full)) != NULL) {
            memcpy(p, ABBREVIATIONS[k].abbr, la);
            memmove(p + la, p + lf, strlen(p + lf) + 1);
            changed = true;
        }
    }
    return changed;
}

// Rows still too wide after shortening scroll (see show_entry).
static void wrap_dest(const char *towards, char *row1, char *row2, size_t n)
{
    char name[DEP_TOWARDS_LEN], marks[4], syms[24] = "";
    tx_split_transfers(towards, name, sizeof(name), marks, sizeof(marks));
    for (const char *m = marks; *m; m++) {
        strcat(syms, *m == 'U' ? " " WL_SYMBOL_UBAHN : " " WL_SYMBOL_SBAHN);
    }
    if (!wrap(name, syms, row1, row2, n) && abbreviate(name)) {
        wrap(name, syms, row1, row2, n);
    }
}

static void set_scrolling(lv_obj_t *label, const char *text)
{
    lv_label_long_mode_t mode = text_w(text, &font_wl_text) > DEST_W * LED ? LV_LABEL_LONG_SCROLL_CIRCULAR
                                                                            : LV_LABEL_LONG_CLIP;
    if (lv_label_get_long_mode(label) != mode) {
        lv_label_set_long_mode(label, mode);
    }
    ui_set_text(label, text);
}

static void show_entry(wl_entry_t *e, const board_row_t *row, bool step_free, int64_t now)
{
    char row1[DEP_TOWARDS_LEN + 24], row2[sizeof(row1)];
    int64_t t = now > 0 ? now : 0;
    const departure_t *d = row->dep;
    // A tram that is not low-floor takes turns with the next one that is.
    bool alt = step_free && d->type == VT_TRAM && row->step_free && t / STEP_FREE_S % 2;
    if (alt) {
        d = row->step_free;
    }
    int line_w = show_line(e, d->line);
    // Every bus is low-floor; the displays only mark the trams. The wheelchair
    // blinks while the later low-floor tram is shown.
    bool wheel = d->type == VT_TRAM && (d->flags & DEP_F_BARRIER_FREE) && (!alt || t % 2 == 0) &&
                 X_LINE * LED + line_w + text_w(LED_SYMBOL_WHEELCHAIR, &font_wl_text) <= X_DEST * LED;
    ui_set_hidden(e->wheel, !wheel);
    set_x(e->wheel, X_LINE * LED + line_w);

    wrap_dest(d->towards, row1, row2, sizeof(row1));
    set_scrolling(e->dest1, row1);
    set_scrolling(e->dest2, row2);

    int m = dep_minutes(d, now);
    if (m <= 0) {
        ui_set_text(e->min, "");
        ui_set_hidden(e->now, false);
        int p = (int)(t % (int64_t)(sizeof(NOW_POS) / sizeof(NOW_POS[0])));
        set_x(e->now, NOW_POS[p].x * LED);
        // The line number label marks the top of the entry.
        int y = lv_obj_get_y(e->line) + NOW_POS[p].dy * LED;
        if (lv_obj_get_y(e->now) != y) {
            lv_obj_set_y(e->now, y);
        }
    } else {
        char buf[8];
        // Times only from the timetable get a leading zero.
        snprintf(buf, sizeof(buf), (d->flags & DEP_F_REALTIME) ? "%d" : "%02d", m);
        ui_set_text(e->min, buf);
        ui_set_hidden(e->now, true);
    }
}

static void update(const ui_state_t *st, const board_t *b, int64_t now)
{
    bool ticker = st->wl_notice && b->notice[0] && !st->no_stops;
    int per_page = ticker ? ENTRIES - 1 : ENTRIES;
    static board_row_t rows[BOARD_MAX_DEPS];
    // One entry per line and direction, like the real displays: no grouping setting.
    int n = st->no_stops ? 0
                         : board_rows(b, now, true, st->walk_min, rows, st->wl_paging ? BOARD_MAX_DEPS : per_page);
    for (int i = 0; i < n; i++) {
        // They count down from 99 at most; later rows are later still.
        if (dep_minutes(rows[i].dep, now) > 99) {
            n = i;
            break;
        }
    }
    int pages = (n + per_page - 1) / per_page;
    if (st->wl_paging) {
        // Paging: every line keeps its place, in the order of the line numbers.
        // Without it the next departures come first.
        board_rows_by_line(rows, n);
    }
    int first = pages > 1 ? (int)((now > 0 ? now : 0) / PAGE_S % pages) * per_page : 0;
    for (int i = 0; i < ENTRIES; i++) {
        wl_entry_t *e = &s.entries[i];
        bool shown = i < per_page && first + i < n;
        set_entry_hidden(e, !shown);
        if (shown) {
            show_entry(e, &rows[first + i], st->wl_step_free, now);
        }
    }

    const char *msg1 = "", *msg2 = "", *hint = "";
    if (st->no_stops) {
        msg1 = "Keine Haltestelle";
        msg2 = "eingerichtet";
        hint = st->url;
    } else if (n == 0) {
        msg1 = "Fahrplanaushang";
        msg2 = "beachten";
    }
    ui_set_text(s.msg1, msg1);
    ui_set_text(s.msg2, msg2);
    ui_set_text(s.hint, hint);
    ui_set_hidden(s.hint, !hint[0]);
    ui_set_text(s.ticker, ticker ? b->notice : "");
    ui_set_hidden(s.ticker, !ticker);
}

static void build_boot(lv_obj_t *scr)
{
    ui_tram_boot_build(scr, ui_wl_glow());
}

const ui_theme_t ui_theme_wl = {
    .bg = WL_GLOW_EDGE,
    .fg = WL_AMBER,
    .accent = WL_AMBER,
    .muted = 0x9C7F1A,
    .title_font = &font_led_24,
    .text_font = &font_led_16,
    .strong_font = &font_led_16,
    .build_board = build,
    .update_board = update,
    .build_boot = build_boot,
    .update_boot = ui_tram_boot_update,
};
