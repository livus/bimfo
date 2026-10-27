// LVGL on the panel: draw buffers, flush, tick, and the landscape orientation.
#pragma once

#include <stdbool.h>

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// Call from the task that runs lv_timer_handler(). Without a panel (NULL, the
// QEMU build) LVGL still renders, and the frames are dropped.
void lvgl_port_init(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io);

// Landscape (320 x 172), optionally turned by 180 degrees.
void lvgl_port_set_flip(bool flip);
