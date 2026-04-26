#include <stdbool.h>
#include <stdint.h>
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

static lv_obj_t *label_city;
static lv_obj_t *label_wifi;
static lv_obj_t *label_time;
static lv_obj_t *label_date;
static lv_obj_t *icon_sun;
static lv_obj_t *icon_moon_mask;
static lv_obj_t *icon_cloud_a;
static lv_obj_t *icon_cloud_b;
static lv_obj_t *icon_cloud_c;
static lv_obj_t *icon_cloud_base;
static lv_obj_t *icon_rain_1;
static lv_obj_t *icon_rain_2;
static lv_obj_t *icon_rain_3;
static lv_obj_t *icon_snow_1;
static lv_obj_t *icon_snow_2;
static lv_obj_t *icon_snow_3;
static lv_obj_t *label_temp;
static lv_obj_t *label_desc;
static lv_obj_t *label_feels;
static lv_obj_t *label_hum;
static lv_obj_t *label_wind;
static lv_obj_t *label_status;

typedef struct {
    uint32_t bg;
    uint32_t text;
    uint32_t muted;
    uint32_t dim;
    uint32_t accent;
    uint32_t icon;
} ui_theme_t;

typedef enum {
    WEATHER_ICON_WAIT,
    WEATHER_ICON_SUN,
    WEATHER_ICON_MOON,
    WEATHER_ICON_CLOUD,
    WEATHER_ICON_RAIN,
    WEATHER_ICON_STORM,
    WEATHER_ICON_SNOW,
    WEATHER_ICON_FOG,
} weather_icon_kind_t;

static const ui_theme_t THEME_NIGHT = {
    .bg = 0x05070a,
    .text = 0xf8fafc,
    .muted = 0xcbd5e1,
    .dim = 0x7d8793,
    .accent = 0xffc857,
    .icon = 0x60a5fa,
};

static const ui_theme_t THEME_DAY = {
    .bg = 0xf6f8fb,
    .text = 0x141a22,
    .muted = 0x3f4b59,
    .dim = 0x6b7480,
    .accent = 0xd97706,
    .icon = 0x0e74b8,
};

static bool s_day_theme = false;

static bool time_is_valid(void)
{
    time_t now = 0;
    struct tm ti = {0};
    time(&now);
    localtime_r(&now, &ti);
    return ti.tm_year >= (2020 - 1900);
}

static void set_label_color(lv_obj_t *label, lv_color_t color)
{
    if (label) {
        lv_obj_set_style_text_color(label, color, 0);
    }
}

static void set_obj_bg(lv_obj_t *obj, lv_color_t color)
{
    if (obj) {
        lv_obj_set_style_bg_color(obj, color, 0);
    }
}

static void set_line_color(lv_obj_t *obj, lv_color_t color)
{
    if (obj) {
        lv_obj_set_style_line_color(obj, color, 0);
    }
}

static void icon_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (!obj) {
        return;
    }

    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_set_weather_icon(weather_icon_kind_t kind)
{
    bool show_sun = kind == WEATHER_ICON_SUN || kind == WEATHER_ICON_MOON || kind == WEATHER_ICON_WAIT;
    bool show_moon_mask = kind == WEATHER_ICON_MOON;
    bool show_cloud = kind == WEATHER_ICON_CLOUD || kind == WEATHER_ICON_RAIN ||
                      kind == WEATHER_ICON_STORM || kind == WEATHER_ICON_SNOW ||
                      kind == WEATHER_ICON_FOG || kind == WEATHER_ICON_WAIT;
    bool show_rain = kind == WEATHER_ICON_RAIN || kind == WEATHER_ICON_STORM;
    bool show_snow = kind == WEATHER_ICON_SNOW || kind == WEATHER_ICON_FOG;

    icon_set_hidden(icon_sun, !show_sun);
    icon_set_hidden(icon_moon_mask, !show_moon_mask);
    icon_set_hidden(icon_cloud_a, !show_cloud);
    icon_set_hidden(icon_cloud_b, !show_cloud);
    icon_set_hidden(icon_cloud_c, !show_cloud);
    icon_set_hidden(icon_cloud_base, !show_cloud);
    icon_set_hidden(icon_rain_1, !show_rain);
    icon_set_hidden(icon_rain_2, !show_rain);
    icon_set_hidden(icon_rain_3, !show_rain);
    icon_set_hidden(icon_snow_1, !show_snow);
    icon_set_hidden(icon_snow_2, !show_snow);
    icon_set_hidden(icon_snow_3, !show_snow);
}

static void ui_apply_theme(bool day)
{
    const ui_theme_t *theme = day ? &THEME_DAY : &THEME_NIGHT;
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(theme->text), 0);

    set_label_color(label_city, lv_color_hex(theme->dim));
    set_label_color(label_wifi, lv_color_hex(wifi_manager_is_connected() ? theme->icon : theme->dim));
    set_label_color(label_date, lv_color_hex(theme->dim));
    set_label_color(label_time, lv_color_hex(theme->text));
    set_label_color(label_temp, lv_color_hex(theme->accent));
    set_label_color(label_desc, lv_color_hex(theme->muted));
    set_label_color(label_feels, lv_color_hex(theme->text));
    set_label_color(label_hum, lv_color_hex(theme->text));
    set_label_color(label_wind, lv_color_hex(theme->text));
    set_label_color(label_status, lv_color_hex(theme->dim));

    set_obj_bg(icon_sun, lv_color_hex(theme->icon));
    set_obj_bg(icon_moon_mask, lv_color_hex(theme->bg));
    set_obj_bg(icon_cloud_a, lv_color_hex(theme->muted));
    set_obj_bg(icon_cloud_b, lv_color_hex(theme->muted));
    set_obj_bg(icon_cloud_c, lv_color_hex(theme->muted));
    set_obj_bg(icon_cloud_base, lv_color_hex(theme->muted));
    set_obj_bg(icon_snow_1, lv_color_hex(theme->icon));
    set_obj_bg(icon_snow_2, lv_color_hex(theme->icon));
    set_obj_bg(icon_snow_3, lv_color_hex(theme->icon));
    set_line_color(icon_rain_1, lv_color_hex(theme->icon));
    set_line_color(icon_rain_2, lv_color_hex(theme->icon));
    set_line_color(icon_rain_3, lv_color_hex(theme->icon));

    s_day_theme = day;
}

static lv_obj_t *create_icon_circle(lv_obj_t *parent, int size, int x, int y)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, size, size);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *create_icon_rect(lv_obj_t *parent, int w, int h, int x, int y, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *create_icon_line(lv_obj_t *parent, const lv_point_precise_t *points)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, 2);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

static void ui_create_weather_icon(lv_obj_t *parent)
{
    static const lv_point_precise_t rain_1[] = {{24, 45}, {18, 58}};
    static const lv_point_precise_t rain_2[] = {{39, 45}, {33, 58}};
    static const lv_point_precise_t rain_3[] = {{54, 45}, {48, 58}};

    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 96, 78);
    lv_obj_align(box, LV_ALIGN_CENTER, -54, -10);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);

    icon_sun = create_icon_circle(box, 44, 24, 6);
    icon_moon_mask = create_icon_circle(box, 36, 44, 2);

    icon_cloud_a = create_icon_circle(box, 36, 11, 29);
    icon_cloud_b = create_icon_circle(box, 44, 30, 18);
    icon_cloud_c = create_icon_circle(box, 30, 58, 34);
    icon_cloud_base = create_icon_rect(box, 66, 25, 17, 45, 13);

    icon_rain_1 = create_icon_line(box, rain_1);
    icon_rain_2 = create_icon_line(box, rain_2);
    icon_rain_3 = create_icon_line(box, rain_3);

    icon_snow_1 = create_icon_circle(box, 5, 24, 68);
    icon_snow_2 = create_icon_circle(box, 5, 44, 70);
    icon_snow_3 = create_icon_circle(box, 5, 64, 68);

    ui_set_weather_icon(WEATHER_ICON_WAIT);
}

static lv_obj_t *create_dash_col(lv_obj_t *parent,
                                 const char *title,
                                 lv_obj_t **value_label,
                                 lv_align_t align,
                                 int x_ofs)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, 76, 48);
    lv_obj_align(col, align, x_ofs, -32);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);

    lv_obj_t *title_label = lv_label_create(col);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(THEME_NIGHT.dim), 0);

    *value_label = lv_label_create(col);
    lv_label_set_text(*value_label, "--");
    lv_obj_align(*value_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(*value_label, lv_color_hex(THEME_NIGHT.text), 0);

    return col;
}

static void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();

    label_city = lv_label_create(scr);
    lv_label_set_long_mode(label_city, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label_city, 118);
    lv_label_set_text_fmt(label_city, "%s, %s", OWM_CITY, OWM_COUNTRY);
    lv_obj_align(label_city, LV_ALIGN_TOP_LEFT, 10, 8);
    lv_obj_set_style_text_font(label_city, &lv_font_montserrat_12, 0);

    label_wifi = lv_label_create(scr);
    lv_label_set_text(label_wifi, "Wi-Fi --");
    lv_obj_align(label_wifi, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_12, 0);

    label_date = lv_label_create(scr);
    lv_label_set_text(label_date, "---");
    lv_obj_align(label_date, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_12, 0);

    label_time = lv_label_create(scr);
    lv_label_set_text(label_time, "--:--:--");
    lv_obj_align(label_time, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, 0);

    ui_create_weather_icon(scr);

    label_temp = lv_label_create(scr);
    lv_label_set_text(label_temp, "--.- C");
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 47, -15);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_28, 0);

    label_desc = lv_label_create(scr);
    lv_label_set_long_mode(label_desc, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label_desc, 210);
    lv_label_set_text(label_desc, "Updating weather");
    lv_obj_align(label_desc, LV_ALIGN_CENTER, 0, 42);
    lv_obj_set_style_text_align(label_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_desc, &lv_font_montserrat_16, 0);

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

    ui_apply_theme(false);
}

static void ui_set_status(const char *txt)
{
    if (label_status) {
        lv_label_set_text(label_status, txt);
    }
}

static void ui_set_wifi_status(void)
{
    if (!label_wifi) {
        return;
    }

    bool connected = wifi_manager_is_connected();
    lv_label_set_text(label_wifi, connected ? "Wi-Fi OK" : "Wi-Fi --");
    set_label_color(label_wifi, lv_color_hex(connected ? (s_day_theme ? THEME_DAY.icon : THEME_NIGHT.icon)
                                                       : (s_day_theme ? THEME_DAY.dim : THEME_NIGHT.dim)));
}

static void ui_set_updated_now(void)
{
    if (!label_status) {
        return;
    }

    if (!time_is_valid()) {
        lv_label_set_text(label_status, "Weather updated");
        return;
    }

    time_t now = 0;
    struct tm ti = {0};
    char buf[32];

    time(&now);
    localtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "Updated %H:%M", &ti);
    lv_label_set_text(label_status, buf);
}

static void ui_set_time_now(void)
{
    if (!label_time || !label_date) {
        return;
    }

    ui_set_wifi_status();

    if (!time_is_valid()) {
        lv_label_set_text(label_time, "--:--:--");
        lv_label_set_text(label_date, "Waiting for time");
        return;
    }

    time_t now = 0;
    struct tm ti = {0};
    char buf[24];

    time(&now);
    localtime_r(&now, &ti);

    strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    lv_label_set_text(label_time, buf);

    strftime(buf, sizeof(buf), "%a %d %b", &ti);
    lv_label_set_text(label_date, buf);
}

static bool weather_is_day(const weather_info_t *w)
{
    int64_t now = w->dt;
    if (now == 0 && time_is_valid()) {
        time_t local_now = 0;
        time(&local_now);
        now = (int64_t)local_now;
    }

    return w->sunrise > 0 && w->sunset > 0 && now >= w->sunrise && now < w->sunset;
}

static weather_icon_kind_t weather_icon_kind(const weather_info_t *w)
{
    if (strncmp(w->icon, "01", 2) == 0) {
        return weather_is_day(w) ? WEATHER_ICON_SUN : WEATHER_ICON_MOON;
    }
    if (strncmp(w->icon, "02", 2) == 0 || strncmp(w->icon, "03", 2) == 0 ||
        strncmp(w->icon, "04", 2) == 0) {
        return WEATHER_ICON_CLOUD;
    }
    if (strncmp(w->icon, "09", 2) == 0 || strncmp(w->icon, "10", 2) == 0) {
        return WEATHER_ICON_RAIN;
    }
    if (strncmp(w->icon, "11", 2) == 0) {
        return WEATHER_ICON_STORM;
    }
    if (strncmp(w->icon, "13", 2) == 0) {
        return WEATHER_ICON_SNOW;
    }
    if (strncmp(w->icon, "50", 2) == 0) {
        return WEATHER_ICON_FOG;
    }
    return WEATHER_ICON_WAIT;
}

static void ui_set_weather(const weather_info_t *w)
{
    if (!label_temp || !w) {
        return;
    }

    bool is_day = weather_is_day(w);
    ui_apply_theme(is_day);
    ui_set_wifi_status();
    ui_set_weather_icon(weather_icon_kind(w));

    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f °C", w->temp_c);
    lv_label_set_text(label_temp, buf);

    char desc[64];
    snprintf(desc, sizeof(desc), "%s", w->description[0] ? w->description : "No description");
    if (desc[0] >= 'a' && desc[0] <= 'z') {
        desc[0] -= ('a' - 'A');
    }
    lv_label_set_text(label_desc, desc);

    snprintf(buf, sizeof(buf), "%.1f °C", w->feels_like_c);
    lv_label_set_text(label_feels, buf);

    snprintf(buf, sizeof(buf), "%d%%", w->humidity_pct);
    lv_label_set_text(label_hum, buf);

    snprintf(buf, sizeof(buf), "%.0f km/h", w->wind_speed_ms * 3.6f);
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
            ui_set_status("Offline, keeping last weather");
            ui_set_wifi_status();
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        weather_info_t w;
        memset(&w, 0, sizeof(w));

        lvgl_port_lock(portMAX_DELAY);
        ui_set_status("Fetching weather");
        ui_set_wifi_status();
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
            ui_set_updated_now();
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS((int)WEATHER_UPDATE_MINUTES * 60 * 1000));
        } else {
            ui_set_status("Weather error, retrying");
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
