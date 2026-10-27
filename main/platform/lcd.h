// The board's 1.47" ST7789T panel (172 x 320, SPI) and its PWM backlight.
#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#define LCD_H_RES   172     // portrait, as the panel is wired
#define LCD_V_RES   320
#define LCD_GAP     34      // the 172 visible columns sit in the middle of the controller's 240

// Sets up SPI, the panel and the backlight (off until lcd_set_backlight()).
void lcd_init(esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io);

// 0 = off ... 100 = full brightness.
void lcd_set_backlight(int percent);
