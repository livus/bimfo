// The only task that touches the LCD and LVGL. Turns app state snapshots into
// UI updates, drives the backlight (incl. night mode) and the BOOT button:
//   short press  -> switch theme (Wiener Linien / OEBB)
//   hold 3 s     -> open the setup access point for 10 minutes
#pragma once

void display_task_start(void);
