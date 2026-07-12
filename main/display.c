#include "display.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_xpt2046.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "display";

#define LCD_HOST         SPI2_HOST
#define TOUCH_HOST       SPI3_HOST

// Cheap Yellow Display pins (ESP32-2432S028R).
#define PIN_LCD_SCLK     14
#define PIN_LCD_MOSI     13
#define PIN_LCD_CS       15
#define PIN_LCD_DC       2
#define PIN_LCD_RST      -1
#define PIN_LCD_BK       21

// XPT2046 touch pins on ESP32-2432S028R.
#define PIN_TOUCH_SCLK   25
#define PIN_TOUCH_MOSI   32
#define PIN_TOUCH_MISO   39
#define PIN_TOUCH_CS     33

#define LCD_HRES         240
#define LCD_VRES         320

// Typical calibrated ADC limits for the ESP32-2432S028R touch panel. The
// XPT2046 component has already scaled raw ADC values to LCD_HRES/LCD_VRES
// before this callback runs.
#define TOUCH_X_LEFT_SCALED     226
#define TOUCH_X_RIGHT_SCALED     16
#define TOUCH_Y_TOP_SCALED       27
#define TOUCH_Y_BOTTOM_SCALED   301

static uint16_t map_touch_axis(int32_t value, int32_t input_min, int32_t input_max,
                               uint16_t output_max)
{
    int32_t mapped = (value - input_min) * output_max / (input_max - input_min);
    if (mapped < 0) {
        return 0;
    }
    if (mapped > output_max) {
        return output_max;
    }
    return (uint16_t)mapped;
}

static void calibrate_touch(esp_lcd_touch_handle_t touch, uint16_t *x, uint16_t *y,
                            uint16_t *strength, uint8_t *point_count, uint8_t max_points)
{
    (void)touch;
    (void)strength;
    uint8_t count = *point_count < max_points ? *point_count : max_points;
    for (uint8_t i = 0; i < count; i++) {
        x[i] = map_touch_axis(x[i], TOUCH_X_LEFT_SCALED, TOUCH_X_RIGHT_SCALED, LCD_HRES - 1);
        y[i] = map_touch_axis(y[i], TOUCH_Y_TOP_SCALED, TOUCH_Y_BOTTOM_SCALED, LCD_VRES - 1);
    }
}

esp_err_t display_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    uint32_t duty = (percent * 255U) / 100U;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty), TAG,
                        "set backlight duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

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

    ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
    ledc_channel_config_t backlight_channel = {
        .gpio_num = PIN_LCD_BK,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 255,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));

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

    spi_bus_config_t touch_buscfg = {
        .sclk_io_num = PIN_TOUCH_SCLK,
        .mosi_io_num = PIN_TOUCH_MOSI,
        .miso_io_num = PIN_TOUCH_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TOUCH_HOST, &touch_buscfg, SPI_DMA_DISABLED));

    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_spi_config_t touch_io_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(PIN_TOUCH_CS);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)TOUCH_HOST,
        &touch_io_cfg,
        &touch_io));

    esp_lcd_touch_handle_t touch = NULL;
    esp_lcd_touch_config_t touch_cfg = {
        .x_max = LCD_HRES,
        .y_max = LCD_VRES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .process_coordinates = calibrate_touch,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(touch_io, &touch_cfg, &touch));

    const lvgl_port_touch_cfg_t lvgl_touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    lv_indev_t *touch_indev = lvgl_port_add_touch(&lvgl_touch_cfg);
    if (!touch_indev) {
        ESP_LOGE(TAG, "lvgl_port_add_touch failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Display and touch OK (%dx%d)", LCD_HRES, LCD_VRES);
    return ESP_OK;
}
