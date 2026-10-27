#include "lvgl_port.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lcd.h"
#include "lvgl.h"

static const char *TAG = "lvgl";

#define BUF_PIXELS  (LCD_H_RES * LCD_V_RES / 10)
#define TICK_MS     2

static lv_color_t s_buf1[BUF_PIXELS];
static lv_color_t s_buf2[BUF_PIXELS];
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_drv;
static lv_disp_t *s_disp;

static void tick(void *arg)
{
    lv_tick_inc(TICK_MS);
}

// SPI transfer finished: LVGL may reuse the buffer.
static bool flush_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    lv_disp_flush_ready((lv_disp_drv_t *)ctx);
    return false;
}

static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    esp_lcd_panel_handle_t panel = drv->user_data;
    if (!panel) {
        lv_disp_flush_ready(drv);
        return;
    }
    // In landscape, y runs along the panel's short side, where the gap is.
    esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1 + LCD_GAP, a->x2 + 1, a->y2 + 1 + LCD_GAP, px);
}

// LVGL changed the rotation: turn the panel's scan direction to match.
static void rotated(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel = drv->user_data;
    if (!panel) {
        return;
    }
    bool swap = drv->rotated == LV_DISP_ROT_90 || drv->rotated == LV_DISP_ROT_270;
    bool mirror_x = drv->rotated == LV_DISP_ROT_NONE || drv->rotated == LV_DISP_ROT_90;
    bool mirror_y = drv->rotated == LV_DISP_ROT_90 || drv->rotated == LV_DISP_ROT_180;
    esp_lcd_panel_swap_xy(panel, swap);
    esp_lcd_panel_mirror(panel, mirror_x, mirror_y);
}

void lvgl_port_set_flip(bool flip)
{
    lv_disp_rot_t rot = flip ? LV_DISP_ROT_270 : LV_DISP_ROT_90;
    if (lv_disp_get_rotation(s_disp) != rot) {
        lv_disp_set_rotation(s_disp, rot);
    }
}

void lvgl_port_init(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io)
{
    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, BUF_PIXELS);
    lv_disp_drv_init(&s_drv);
    s_drv.hor_res = LCD_H_RES;
    s_drv.ver_res = LCD_V_RES;
    s_drv.flush_cb = flush;
    s_drv.drv_update_cb = rotated;
    s_drv.draw_buf = &s_draw_buf;
    s_drv.user_data = panel;
    if (io) {
        const esp_lcd_panel_io_callbacks_t cbs = {.on_color_trans_done = flush_done};
        ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io, &cbs, &s_drv));
    }
    s_disp = lv_disp_drv_register(&s_drv);
    lv_disp_set_rotation(s_disp, LV_DISP_ROT_90);

    const esp_timer_create_args_t args = {.callback = tick, .name = "lvgl_tick"};
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, TICK_MS * 1000));
    ESP_LOGI(TAG, "LVGL ready (%s)", panel ? "ST7789T" : "no panel");
}
