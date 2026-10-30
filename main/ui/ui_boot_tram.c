// Start screen of the LED styles: while the device connects, a low-floor tram
// (ULF) waits at the stop under the overhead wire, pulls away to the left and
// the next one arrives from the right, over and over. It moves in whole LEDs,
// as a picture on the real dot matrix would. The status sits below.
#include "ui_priv.h"

#define WL_AMBER    0xFFD02A

// Geometry in LEDs, as in ui_theme_wl.c: one LED is 2x2 pixels.
#define LED         2
#define PANEL_W     160
#define TRAM_W      124
#define TRAM_H      23
#define WIRE_Y      14              // the pantograph touches the wire from below
#define TRAM_Y      (WIRE_Y + 1)
#define RAIL_Y      (TRAM_Y + TRAM_H + 1)
#define STATUS_Y    48              // two text rows
#define URL_Y       72
#define STOP_X      ((PANEL_W - TRAM_W) / 2)

// One round, in ms: waiting at the stop, pulling away, an empty track, arriving.
#define T_STAND     2500
#define T_OUT       2500
#define T_GAP       600
#define T_IN        3000
#define T_ROUND     (T_STAND + T_OUT + T_GAP + T_IN)

static struct {
    lv_obj_t *tram, *status, *url;
} s;

// Left edge of the tram (LEDs) `t` ms into a round: it speeds up as it pulls
// away and slows down as it arrives.
static int32_t tram_x(int32_t t)
{
    if (t < T_STAND) {
        return STOP_X;
    }
    t -= T_STAND;
    if (t < T_OUT) {
        int64_t u = (int64_t)t * 1024 / T_OUT;
        return STOP_X - (int32_t)((STOP_X + TRAM_W) * u * u >> 20);
    }
    t -= T_OUT;
    if (t < T_GAP) {
        return PANEL_W;
    }
    t -= T_GAP;
    int64_t u = 1024 - (int64_t)t * 1024 / T_IN;
    return STOP_X + (int32_t)((PANEL_W - STOP_X) * u * u >> 20);
}

static void drive(void *var, int32_t t)
{
    int32_t x = tram_x(t) * LED;
    if (lv_obj_get_x(var) != x) {
        lv_obj_set_x(var, x);
    }
}

// Overhead wire or rail across the panel, from two halves (a glyph is at most
// 255 px wide).
static void line_label(lv_obj_t *scr, int y)
{
    for (int x = 0; x < PANEL_W; x += PANEL_W / 2) {
        lv_obj_t *l = ui_label(scr, &font_wl_tram, WL_AMBER, x * LED, y * LED, 0, LV_TEXT_ALIGN_LEFT,
                               LV_LABEL_LONG_CLIP);
        lv_label_set_text(l, WL_SYMBOL_WIRE);
    }
}

void ui_tram_boot_build(lv_obj_t *scr, const lv_img_dsc_t *bg)
{
    if (bg) {
        lv_obj_t *img = lv_img_create(scr);
        lv_img_set_src(img, bg);
        lv_obj_set_pos(img, 0, 0);
    }
    line_label(scr, WIRE_Y);
    line_label(scr, RAIL_Y);
    s.tram = ui_label(scr, &font_wl_tram, WL_AMBER, STOP_X * LED, TRAM_Y * LED, 0, LV_TEXT_ALIGN_LEFT,
                      LV_LABEL_LONG_CLIP);
    lv_label_set_text(s.tram, WL_SYMBOL_TRAM);

    s.status = ui_label(scr, &font_wl_text, WL_AMBER, LED, STATUS_Y * LED, (PANEL_W - 2) * LED,
                        LV_TEXT_ALIGN_CENTER, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s.status, 2 * font_wl_text.line_height);
    s.url = ui_label(scr, &font_wl_text, WL_AMBER, LED, URL_Y * LED, (PANEL_W - 2) * LED, LV_TEXT_ALIGN_CENTER,
                     LV_LABEL_LONG_DOT);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s.tram);
    lv_anim_set_exec_cb(&a, drive);
    lv_anim_set_values(&a, 0, T_ROUND);
    lv_anim_set_time(&a, T_ROUND);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

void ui_tram_boot_update(const ui_state_t *st)
{
    ui_set_text(s.status, st->status);
    ui_set_text(s.url, st->url);
}
