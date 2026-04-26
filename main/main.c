#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "app_config.h"
#include "display.h"
#include "sntp_time.h"
#include "weather_client.h"
#include "wifi_manager.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_48);

static const char *TAG = "main";

static lv_obj_t *label_time;
static lv_obj_t *label_date;
static lv_obj_t *label_temp;
static lv_obj_t *label_desc;
static lv_obj_t *label_feels;
static lv_obj_t *label_hum;
static lv_obj_t *label_wind;
static lv_obj_t *label_status;

static bool time_is_valid(void)
{
    time_t now = 0;
    struct tm ti = {0};
    time(&now);
    localtime_r(&now, &ti);
    return ti.tm_year >= (2020 - 1900);
}

static lv_obj_t *create_separator(lv_obj_t *parent, lv_align_t align, int y_ofs)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, 204, 1);
    lv_obj_align(line, align, 0, y_ofs);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x1f2933), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    return line;
}

static lv_obj_t *create_dash_col(lv_obj_t *parent,
                                 const char *title,
                                 lv_obj_t **value_label,
                                 lv_align_t align,
                                 int x_ofs)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, 76, 48);
    lv_obj_align(col, align, x_ofs, -28);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);

    lv_obj_t *title_label = lv_label_create(col);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x7d8793), 0);

    *value_label = lv_label_create(col);
    lv_label_set_text(*value_label, "--");
    lv_obj_align(*value_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(*value_label, lv_color_hex(0xf8fafc), 0);

    return col;
}

static void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x05070a), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xf8fafc), 0);

    label_date = lv_label_create(scr);
    lv_label_set_text(label_date, "---");
    lv_obj_align(label_date, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_date, lv_color_hex(0x7d8793), 0);

    label_time = lv_label_create(scr);
    lv_label_set_text(label_time, "--:--");
    lv_obj_align(label_time, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xf8fafc), 0);

    create_separator(scr, LV_ALIGN_TOP_MID, 92);

    label_temp = lv_label_create(scr);
    lv_label_set_text(label_temp, "--.- C");
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, -18);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_temp, lv_color_hex(0xffc857), 0);

    label_desc = lv_label_create(scr);
    lv_label_set_long_mode(label_desc, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label_desc, 210);
    lv_label_set_text(label_desc, "Updating weather");
    lv_obj_align_to(label_desc, label_temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_text_align(label_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_desc, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_desc, lv_color_hex(0xcbd5e1), 0);

    create_separator(scr, LV_ALIGN_BOTTOM_MID, -86);

    create_dash_col(scr, "Feels", &label_feels, LV_ALIGN_BOTTOM_LEFT, 5);
    create_dash_col(scr, "Humidity", &label_hum, LV_ALIGN_BOTTOM_MID, 0);
    create_dash_col(scr, "Wind", &label_wind, LV_ALIGN_BOTTOM_RIGHT, -5);

    label_status = lv_label_create(scr);
    lv_label_set_long_mode(label_status, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label_status, 220);
    lv_label_set_text(label_status, "Booting");
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_text_align(label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x475569), 0);
}

static void ui_set_status(const char *txt)
{
    if (label_status) {
        lv_label_set_text(label_status, txt);
    }
}

static void ui_set_time_now(void)
{
    if (!label_time || !label_date) {
        return;
    }

    if (!time_is_valid()) {
        lv_label_set_text(label_time, "--:--");
        lv_label_set_text(label_date, "Waiting for time");
        return;
    }

    time_t now = 0;
    struct tm ti = {0};
    char buf[24];

    time(&now);
    localtime_r(&now, &ti);

    strftime(buf, sizeof(buf), "%H:%M", &ti);
    lv_label_set_text(label_time, buf);

    strftime(buf, sizeof(buf), "%a %d %b", &ti);
    lv_label_set_text(label_date, buf);
}

static void ui_set_weather(const weather_info_t *w)
{
    if (!label_temp || !w) {
        return;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f C", w->temp_c);
    lv_label_set_text(label_temp, buf);

    char desc[64];
    snprintf(desc, sizeof(desc), "%s", w->description[0] ? w->description : "No description");
    if (desc[0] >= 'a' && desc[0] <= 'z') {
        desc[0] -= ('a' - 'A');
    }
    lv_label_set_text(label_desc, desc);

    snprintf(buf, sizeof(buf), "%.1f C", w->feels_like_c);
    lv_label_set_text(label_feels, buf);

    snprintf(buf, sizeof(buf), "%d%%", w->humidity_pct);
    lv_label_set_text(label_hum, buf);

    snprintf(buf, sizeof(buf), "%.1f m/s", w->wind_speed_ms);
    lv_label_set_text(label_wind, buf);
}

static void task_clock(void *arg)
{
    (void)arg;

    while (1) {
        lvgl_port_lock(portMAX_DELAY);
        ui_set_time_now();
        lvgl_port_unlock();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void task_weather(void *arg)
{
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        if (!wifi_manager_is_connected()) {
            lvgl_port_lock(portMAX_DELAY);
            ui_set_status("Waiting for Wi-Fi");
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        weather_info_t w;
        memset(&w, 0, sizeof(w));

        lvgl_port_lock(portMAX_DELAY);
        ui_set_status("Fetching weather");
        lvgl_port_unlock();

        esp_err_t ret = weather_fetch_current(
            OWM_API_KEY,
            OWM_CITY,
            OWM_COUNTRY,
            OWM_LANG,
            &w,
            12000);

        lvgl_port_lock(portMAX_DELAY);
        if (ret == ESP_OK) {
            ui_set_weather(&w);
            ui_set_status("Weather updated");
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS((int)WEATHER_UPDATE_MINUTES * 60 * 1000));
        } else {
            ui_set_status("Weather update failed");
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(30000));
        }
    }
}

void app_main(void)
{
    wifi_manager_cfg_t cfg = {
        .ssid = WIFI_SSID,
        .pass = WIFI_PASS,
        .use_wpa3_sae = false,
    };
    ESP_ERROR_CHECK(wifi_manager_start(&cfg));

    esp_err_t ret = wifi_manager_wait_connected(pdMS_TO_TICKS(30000));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi timeout, continuing without initial connection: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Wi-Fi connected");
    }

    sntp_time_init(APP_TIMEZONE);
    (void)sntp_time_wait(20, 1000);

    ESP_ERROR_CHECK(display_init());

    lvgl_port_lock(portMAX_DELAY);
    ui_create();
    ui_set_time_now();
    ui_set_status("Running");
    lvgl_port_unlock();

    xTaskCreate(task_clock, "clock", 4096, NULL, 5, NULL);
    xTaskCreate(task_weather, "weather", 6144, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
