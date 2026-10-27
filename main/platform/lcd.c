#include "lcd.h"

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "panel_st7789t.h"

static const char *TAG = "lcd";

#define LCD_SPI         SPI3_HOST
#define LCD_PIXEL_HZ    (12 * 1000 * 1000)
#define PIN_SCLK        40
#define PIN_MOSI        45
#define PIN_DC          41
#define PIN_RST         39
#define PIN_CS          42
#define PIN_BACKLIGHT   46

#define BL_TIMER        LEDC_TIMER_0
#define BL_CHANNEL      LEDC_CHANNEL_0
#define BL_MODE         LEDC_LOW_SPEED_MODE
#define BL_RES          LEDC_TIMER_13_BIT
#define BL_FREQ_HZ      4000

static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = BL_MODE,
        .timer_num = BL_TIMER,
        .duty_resolution = BL_RES,
        .freq_hz = BL_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t channel = {
        .speed_mode = BL_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = PIN_BACKLIGHT,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void lcd_set_backlight(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint32_t max = (1u << BL_RES) - 1;
    ESP_ERROR_CHECK(ledc_set_duty(BL_MODE, BL_CHANNEL, max * (uint32_t)percent / 100));
    ESP_ERROR_CHECK(ledc_update_duty(BL_MODE, BL_CHANNEL));
}

void lcd_init(esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io)
{
    ESP_LOGI(TAG, "init ST7789T on SPI3");
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SCLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = LCD_PIXEL_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI, &io_cfg, io));

    esp_lcd_panel_dev_st7789t_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789t(*io, &panel_cfg, panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(*panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel, true));

    backlight_init();
}
