/* LVGL configuration for the host render harness. Mirrors the options the
 * firmware sets through Kconfig (sdkconfig.defaults) that affect rendering;
 * everything else falls back to LVGL's defaults. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0
#define LV_MEM_SIZE             (48U * 1024U)
#define LV_DPI_DEF              130
#define LV_USE_PERF_MONITOR     0
#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF           1
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_USER_DATA        1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14
#define LV_TXT_ENC              LV_TXT_ENC_UTF8
#define LV_USE_QRCODE           1
#define LV_BUILD_EXAMPLES       0
#define LV_USE_DEMO_WIDGETS     0

#endif
