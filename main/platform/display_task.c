#include "display_task.h"

#include <stdio.h>
#include <string.h>

#include "app_state.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "lcd.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "timeutil.h"
#include "ui.h"
#include "wifi_mgr.h"

static const char *TAG = "display";

#define BUTTON_GPIO         GPIO_NUM_0      // BOOT button, active low
#define LONG_PRESS_MS       3000
#define UI_PERIOD_MS        200
#define MARK_VALID_AFTER_S  30

EXT_RAM_BSS_ATTR static board_t s_board;
EXT_RAM_BSS_ATTR static app_config_t s_cfg;

static void build_state(ui_state_t *st, const app_config_t *cfg)
{
    net_status_t net;
    fetch_status_t fs;
    app_get_net(&net);
    app_get_fetch(&fs);

    memset(st, 0, sizeof(*st));
    st->theme = cfg->theme;
    st->group_lines = cfg->group_lines;
    st->no_stops = cfg->nstops == 0;
    for (int i = 0; i < cfg->nstops && i < BOARD_MAX_STOPS; i++) {
        st->walk_min[i] = cfg->stops[i].walk_min;
    }

    if (net.ap_active) {
        st->screen = UI_SCREEN_SETUP;
        strlcpy(st->ap_ssid, net.ap_ssid, sizeof(st->ap_ssid));
        snprintf(st->url, sizeof(st->url), "http://%s", WIFI_AP_IP);
        if (net.sta_configured && !net.sta_connected) {
            snprintf(st->status, sizeof(st->status), "WLAN \xE2\x80\x9E%s\xE2\x80\x9C nicht erreichbar", net.sta_ssid);
        } else if (net.sta_connected) {
            snprintf(st->status, sizeof(st->status), "Im Heimnetz: http://%s", net.ip);
        }
    } else if (!net.sta_connected) {
        st->screen = UI_SCREEN_BOOT;
        if (net.sta_configured) {
            snprintf(st->status, sizeof(st->status), "Verbinde mit WLAN\n\xE2\x80\x9E%s\xE2\x80\x9C ...", net.sta_ssid);
        } else {
            strlcpy(st->status, "Starte ...", sizeof(st->status));
        }
    } else {
        st->screen = UI_SCREEN_BOARD;
        snprintf(st->url, sizeof(st->url), "http://%s.local  \xC2\xB7  %s", cfg->hostname, net.ip);
        // Warn once fetching keeps failing (a single hiccup is not shown).
        if (fs.consecutive_errors >= 2 && cfg->nstops > 0) {
            if (fs.last_ok) {
                tu_fields_t f;
                tu_vienna_fields(fs.last_ok, &f);
                snprintf(st->status, sizeof(st->status), "Offline - Stand %02d:%02d", f.hour, f.min);
            } else {
                strlcpy(st->status, "Keine Verbindung zum Server", sizeof(st->status));
            }
        }
    }
}

typedef struct {
    bool down;
    int64_t since_us;
    bool long_fired;
} button_t;

static void poll_button(button_t *b)
{
    bool down = gpio_get_level(BUTTON_GPIO) == 0;
    int64_t t = esp_timer_get_time();
    if (down && !b->down) {
        b->since_us = t;
        b->long_fired = false;
    } else if (down && !b->long_fired && t - b->since_us > LONG_PRESS_MS * 1000LL) {
        b->long_fired = true;
        ESP_LOGI(TAG, "button held: opening setup access point");
        wifi_mgr_force_setup(10 * 60);
    } else if (!down && b->down && !b->long_fired && t - b->since_us > 30 * 1000) {
        // Short press (debounced): next theme, remembered across restarts.
        app_get_config(&s_cfg);
        s_cfg.theme = (uint8_t)((s_cfg.theme + 1) % THEME_COUNT);
        app_set_config(&s_cfg);
        ESP_LOGI(TAG, "button: theme %s", cfg_theme_name(s_cfg.theme));
    }
    b->down = down;
}

static void display_task(void *arg)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t panel_io = NULL;
#if CONFIG_APP_QEMU
    // QEMU emulates neither the SPI panel nor the backlight PWM: run LVGL and
    // the UI against a panel-less driver so everything else can be tested.
    ESP_LOGW(TAG, "QEMU build: no LCD, frames are rendered and dropped");
#else
    lcd_init(&panel, &panel_io);
#endif
    lvgl_port_init(panel, panel_io);
    ui_init();

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);

    ui_state_t st;
    button_t button = {0};
    uint32_t board_gen = UINT32_MAX;
    int64_t next_ui = 0, next_backlight = 0;
    int last_brightness = -1;
    bool marked_valid = false;

    for (;;) {
        int64_t t = esp_timer_get_time();
        if (t >= next_ui) {
            next_ui = t + UI_PERIOD_MS * 1000;
            app_get_config(&s_cfg);
            if (app_board_generation() != board_gen) {
                app_get_board(&s_board, &board_gen);
            }
            build_state(&st, &s_cfg);
            lvgl_port_set_flip(s_cfg.flip);
            ui_update(&st, &s_board, app_now());
        }
        if (t >= next_backlight) {
            next_backlight = t + 1000 * 1000;
            int b = cfg_brightness(&s_cfg, app_now());
            if (b != last_brightness) {
#if !CONFIG_APP_QEMU
                lcd_set_backlight(b);
#endif
                last_brightness = b;
            }
            app_note_stack();
            // The new firmware drew its UI for a while: keep it (OTA rollback).
            if (!marked_valid && t > MARK_VALID_AFTER_S * 1000000LL) {
                esp_ota_mark_app_valid_cancel_rollback();
                marked_valid = true;
            }
        }
        poll_button(&button);
        uint32_t wait = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(wait < 10 ? wait + 1 : 10));
    }
}

void display_task_start(void)
{
    // Core 1 keeps rendering smooth while WiFi/TLS run on core 0.
    xTaskCreatePinnedToCore(display_task, "display", 8192, NULL, 5, NULL, 1);
}
