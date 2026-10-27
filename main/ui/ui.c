#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "timeutil.h"
#include "ui_priv.h"

static struct {
    lv_obj_t *scr;
    int screen;             // ui_screen_t currently built, -1 = none
    int theme;
    char setup_key[288];    // what the setup screen was built for (ssid|pass|url|status)
    lv_obj_t *boot_status;
    lv_obj_t *boot_url;
} s_ui = {.screen = -1, .theme = -1};

static const ui_theme_t *theme_for(uint8_t theme)
{
    return theme == THEME_OEBB ? &ui_theme_oebb : &ui_theme_wl;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w,
                   lv_text_align_t align, lv_label_long_mode_t mode)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_long_mode(l, mode);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_size(l, w, font->line_height);
    }
    lv_label_set_text(l, "");
    return l;
}

lv_obj_t *ui_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void ui_set_text(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

void ui_set_color(lv_obj_t *obj, uint32_t color)
{
    lv_color_t c = lv_color_hex(color);
    if (lv_obj_get_style_text_color(obj, 0).full != c.full) {
        lv_obj_set_style_text_color(obj, c, 0);
    }
}

void ui_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) != hidden) {
        if (hidden) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_fmt_clock(int64_t t, bool seconds, char *buf, size_t n)
{
    if (t <= 0) {
        snprintf(buf, n, seconds ? "--:--:--" : "--:--");
        return;
    }
    tu_fields_t f;
    tu_vienna_fields(t, &f);
    if (seconds) {
        snprintf(buf, n, "%02d:%02d:%02d", f.hour, f.min, f.sec);
    } else {
        snprintf(buf, n, "%02d:%02d", f.hour, f.min);
    }
}

void ui_init(void)
{
    s_ui.scr = lv_scr_act();
    lv_obj_clear_flag(s_ui.scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_ui.scr, LV_OPA_COVER, 0);
    s_ui.screen = -1;
    s_ui.theme = -1;
}

static void build_boot(const ui_theme_t *th)
{
    lv_obj_t *title = ui_label(s_ui.scr, th->title_font, th->accent, 0, 44, UI_W, LV_TEXT_ALIGN_CENTER,
                               LV_LABEL_LONG_CLIP);
    lv_label_set_text(title, "Bimfo");
    s_ui.boot_status = ui_label(s_ui.scr, th->text_font, th->fg, 8, 92, UI_W - 16, LV_TEXT_ALIGN_CENTER,
                                LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s_ui.boot_status, th->text_font->line_height * 2 + 2);
    s_ui.boot_url = ui_label(s_ui.scr, &font_sans_14, th->muted, 8, 136, UI_W - 16, LV_TEXT_ALIGN_CENTER,
                             LV_LABEL_LONG_DOT);
}

static void build_setup(const ui_theme_t *th, const ui_state_t *st)
{
    char qr[160];
    if (st->ap_pass[0]) {
        snprintf(qr, sizeof(qr), "WIFI:T:WPA;S:%s;P:%s;;", st->ap_ssid, st->ap_pass);
    } else {
        snprintf(qr, sizeof(qr), "WIFI:T:nopass;S:%s;;", st->ap_ssid);
    }
    ui_rect(s_ui.scr, 6, 6, 160, 160, 0xFFFFFF);
    lv_obj_t *code = lv_qrcode_create(s_ui.scr, 148, lv_color_black(), lv_color_white());
    lv_obj_set_pos(code, 12, 12);
    lv_qrcode_update(code, qr, strlen(qr));

    const int x = 176, w = UI_W - x - 4;
    lv_obj_t *l = ui_label(s_ui.scr, &font_sans_bold_16, th->accent, x, 6, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, "WLAN-Setup");
    l = ui_label(s_ui.scr, &font_sans_12, th->fg, x, 32, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, "QR-Code scannen oder");
    l = ui_label(s_ui.scr, &font_sans_12, th->fg, x, 46, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, "mit WLAN verbinden:");
    l = ui_label(s_ui.scr, &font_sans_bold_14, th->accent, x, 62, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, st->ap_ssid);
    int y = 80;
    if (st->ap_pass[0]) {
        l = ui_label(s_ui.scr, &font_sans_12, th->fg, x, y, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
        lv_label_set_text_fmt(l, "Passwort: %s", st->ap_pass);
        y += 16;
    }
    l = ui_label(s_ui.scr, &font_sans_12, th->fg, x, y + 14, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, "Danach im Browser:");
    l = ui_label(s_ui.scr, &font_sans_bold_14, th->accent, x, y + 30, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, st->url);
    if (st->status[0]) {
        l = ui_label(s_ui.scr, &font_sans_12, th->muted, x, UI_H - 30, w, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_WRAP);
        lv_obj_set_height(l, 2 * font_sans_12.line_height);
        lv_label_set_text(l, st->status);
    }
}

void ui_update(const ui_state_t *st, const board_t *b, int64_t now)
{
    const ui_theme_t *th = theme_for(st->theme);
    char setup_key[sizeof(s_ui.setup_key)] = "";
    if (st->screen == UI_SCREEN_SETUP) {
        snprintf(setup_key, sizeof(setup_key), "%s|%s|%s|%s", st->ap_ssid, st->ap_pass, st->url, st->status);
    }
    if ((int)st->screen != s_ui.screen || st->theme != s_ui.theme || strcmp(setup_key, s_ui.setup_key) != 0) {
        lv_obj_clean(s_ui.scr);
        lv_obj_set_style_bg_color(s_ui.scr, lv_color_hex(th->bg), 0);
        switch (st->screen) {
        case UI_SCREEN_BOOT:
            build_boot(th);
            break;
        case UI_SCREEN_SETUP:
            build_setup(th, st);
            break;
        case UI_SCREEN_BOARD:
            th->build_board(s_ui.scr);
            break;
        }
        s_ui.screen = st->screen;
        s_ui.theme = st->theme;
        strcpy(s_ui.setup_key, setup_key);
    }
    switch (st->screen) {
    case UI_SCREEN_BOOT:
        ui_set_text(s_ui.boot_status, st->status);
        ui_set_text(s_ui.boot_url, st->url);
        break;
    case UI_SCREEN_SETUP:
        break;
    case UI_SCREEN_BOARD:
        th->update_board(st, b, now);
        break;
    }
}
