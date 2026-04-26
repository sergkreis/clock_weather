#include "display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "display";

#define LCD_HOST         SPI2_HOST

// Cheap Yellow Display pins (ESP32-2432S028R).
#define PIN_LCD_SCLK     14
#define PIN_LCD_MOSI     13
#define PIN_LCD_CS       15
#define PIN_LCD_DC       2
#define PIN_LCD_RST      -1
#define PIN_LCD_BK       21

#define LCD_HRES         240
#define LCD_VRES         320

esp_err_t display_init(void)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_HRES * 80 * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io_handle));
    ESP_LOGI(TAG, "io_handle=%p", io_handle);

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    // Enable if colors look inverted on a display variant.
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

    // Enable and tune if the image is shifted on a display variant.
    // ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 0, 34));

    gpio_config_t bk = {
        .pin_bit_mask = 1ULL << PIN_LCD_BK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bk));
    gpio_set_level(PIN_LCD_BK, 1);

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel,
        .buffer_size = LCD_HRES * 40,
        .double_buffer = true,
        .hres = LCD_HRES,
        .vres = LCD_VRES,
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };

    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Display OK (%dx%d)", LCD_HRES, LCD_VRES);
    return ESP_OK;
}
