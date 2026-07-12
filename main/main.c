#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app_config.h"

#include "esp_err.h"
#include "esp_log.h"

#include "display.h"
#include "radar_client.h"
#include "radar_ui.h"
#include "sntp_time.h"
#include "weather_client.h"
#include "wifi_manager.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_48);

static const char *TAG = "main";

static lv_obj_t *weather_screen;
static lv_obj_t *radar_screen;
static bool showing_radar = false;
static SemaphoreHandle_t api_mutex;
static lv_obj_t *boot_screen;
static lv_obj_t *boot_status;
static lv_obj_t *boot_bar;

static lv_obj_t *label_city;
static lv_obj_t *label_wifi;
static lv_obj_t *label_date;
static lv_obj_t *label_hour;
static lv_obj_t *label_colon;
static lv_obj_t *label_minute;
static lv_obj_t *label_second;
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
static lv_obj_t *decor_header_line;
static lv_obj_t *decor_weather_panel;
static lv_obj_t *decor_metrics_panel;
static lv_obj_t *decor_metric_sep_a;
static lv_obj_t *decor_metric_sep_b;

typedef struct {
    uint32_t bg;
    uint32_t text;
    uint32_t muted;
    uint32_t dim;
    uint32_t accent;
    uint32_t icon;
    uint32_t cloud;
    uint32_t panel;
    uint32_t line;
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
    .cloud = 0xcbd5e1,
    .panel = 0x0b1118,
    .line = 0x263342,
};

static const ui_theme_t THEME_DAY = {
    .bg = 0xe7f0f5,
    .text = 0x111827,
    .muted = 0x374151,
    .dim = 0x708093,
    .accent = 0xb45309,
    .icon = 0x0369a1,
    .cloud = 0x6b7a8b,
    .panel = 0xf3f8fb,
    .line = 0xb8c7d4,
};

static bool s_day_theme = false;

static void screen_tap_handler(lv_event_t *event);

static void boot_ui_create(void)
{
    boot_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(boot_screen, lv_color_hex(0x05070a), 0);
    lv_obj_set_style_bg_opa(boot_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(boot_screen, 0, 0);
    lv_obj_set_style_pad_all(boot_screen, 0, 0);

    lv_obj_t *title = lv_label_create(boot_screen);
    lv_label_set_text(title, "CLOCK WEATHER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x55ef87), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -58);

    lv_obj_t *subtitle = lv_label_create(boot_screen);
    lv_label_set_text(subtitle, "CYD starting");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x8abf98), 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, -32);

    boot_bar = lv_bar_create(boot_screen);
    lv_obj_set_size(boot_bar, 190, 8);
    lv_obj_align(boot_bar, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_style_bg_color(boot_bar, lv_color_hex(0x173522), LV_PART_MAIN);
    lv_obj_set_style_bg_color(boot_bar, lv_color_hex(0x55ef87), LV_PART_INDICATOR);
    lv_obj_set_style_radius(boot_bar, 2, LV_PART_MAIN | LV_PART_INDICATOR);
    lv_bar_set_range(boot_bar, 0, 100);
    lv_bar_set_value(boot_bar, 10, LV_ANIM_OFF);

    boot_status = lv_label_create(boot_screen);
    lv_label_set_text(boot_status, "Display ready");
    lv_obj_set_style_text_font(boot_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(boot_status, lv_color_hex(0xd7ffe1), 0);
    lv_obj_align(boot_status, LV_ALIGN_CENTER, 0, 34);
    lv_screen_load(boot_screen);
}

static void boot_ui_step(const char *status, int progress)
{
    lvgl_port_lock(portMAX_DELAY);
    if (boot_status) lv_label_set_text(boot_status, status);
    if (boot_bar) lv_bar_set_value(boot_bar, progress, LV_ANIM_ON);
    lvgl_port_unlock();
}

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

static void set_border_color(lv_obj_t *obj, lv_color_t color)
{
    if (obj) {
        lv_obj_set_style_border_color(obj, color, 0);
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
    lv_obj_t *scr = weather_screen ? weather_screen : lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(theme->text), 0);

    set_label_color(label_city, lv_color_hex(theme->dim));
    set_label_color(label_wifi, lv_color_hex(wifi_manager_is_connected() ? theme->icon : theme->dim));
    set_label_color(label_date, lv_color_hex(theme->dim));
    set_label_color(label_hour, lv_color_hex(theme->text));
    set_label_color(label_colon, lv_color_hex(theme->text));
    set_label_color(label_minute, lv_color_hex(theme->text));
    set_label_color(label_second, lv_color_hex(theme->dim));
    set_label_color(label_temp, lv_color_hex(theme->accent));
    set_label_color(label_desc, lv_color_hex(theme->muted));
    set_label_color(label_feels, lv_color_hex(theme->text));
    set_label_color(label_hum, lv_color_hex(theme->text));
    set_label_color(label_wind, lv_color_hex(theme->text));
    set_label_color(label_status, lv_color_hex(theme->dim));

    set_obj_bg(icon_sun, lv_color_hex(theme->icon));
    set_obj_bg(icon_moon_mask, lv_color_hex(theme->bg));
    set_obj_bg(icon_cloud_a, lv_color_hex(theme->cloud));
    set_obj_bg(icon_cloud_b, lv_color_hex(theme->cloud));
    set_obj_bg(icon_cloud_c, lv_color_hex(theme->cloud));
    set_obj_bg(icon_cloud_base, lv_color_hex(theme->cloud));
    set_obj_bg(icon_snow_1, lv_color_hex(theme->icon));
    set_obj_bg(icon_snow_2, lv_color_hex(theme->icon));
    set_obj_bg(icon_snow_3, lv_color_hex(theme->icon));
    set_line_color(icon_rain_1, lv_color_hex(theme->icon));
    set_line_color(icon_rain_2, lv_color_hex(theme->icon));
    set_line_color(icon_rain_3, lv_color_hex(theme->icon));

    set_obj_bg(decor_header_line, lv_color_hex(theme->line));
    set_obj_bg(decor_weather_panel, lv_color_hex(theme->panel));
    set_border_color(decor_weather_panel, lv_color_hex(theme->line));
    set_obj_bg(decor_metrics_panel, lv_color_hex(theme->panel));
    set_border_color(decor_metrics_panel, lv_color_hex(theme->line));
    set_obj_bg(decor_metric_sep_a, lv_color_hex(theme->line));
    set_obj_bg(decor_metric_sep_b, lv_color_hex(theme->line));

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

static lv_obj_t *create_deco_panel(lv_obj_t *parent, int w, int h, int x, int y, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *create_deco_rule(lv_obj_t *parent, int w, int h, int x, int y)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_60, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static void ui_create_decor(lv_obj_t *parent)
{
    decor_header_line = create_deco_rule(parent, 220, 1, 10, 39);
    decor_weather_panel = create_deco_panel(parent, 222, 116, 9, 108, 8);
    decor_metrics_panel = create_deco_panel(parent, 220, 58, 10, 232, 7);
    decor_metric_sep_a = create_deco_rule(parent, 1, 34, 82, 244);
    decor_metric_sep_b = create_deco_rule(parent, 1, 34, 158, 244);
}

static void ui_create_weather_icon(lv_obj_t *parent)
{
    static const lv_point_precise_t rain_1[] = {{24, 45}, {18, 58}};
    static const lv_point_precise_t rain_2[] = {{39, 45}, {33, 58}};
    static const lv_point_precise_t rain_3[] = {{54, 45}, {48, 58}};

    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 70, 58);
    lv_obj_align(box, LV_ALIGN_CENTER, -76, -15);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);

    icon_sun = create_icon_circle(box, 32, 18, 5);
    icon_moon_mask = create_icon_circle(box, 26, 32, 2);

    icon_cloud_a = create_icon_circle(box, 25, 8, 22);
    icon_cloud_b = create_icon_circle(box, 31, 22, 14);
    icon_cloud_c = create_icon_circle(box, 21, 43, 26);
    icon_cloud_base = create_icon_rect(box, 47, 18, 12, 34, 9);

    icon_rain_1 = create_icon_line(box, rain_1);
    icon_rain_2 = create_icon_line(box, rain_2);
    icon_rain_3 = create_icon_line(box, rain_3);

    icon_snow_1 = create_icon_circle(box, 4, 17, 50);
    icon_snow_2 = create_icon_circle(box, 4, 32, 52);
    icon_snow_3 = create_icon_circle(box, 4, 47, 50);

    ui_set_weather_icon(WEATHER_ICON_WAIT);
}

static lv_obj_t *create_dash_col(lv_obj_t *parent,
                                 const char *title,
                                 lv_obj_t **value_label,
                                 lv_align_t align,
                                 int x_ofs)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, 76, 42);
    lv_obj_align(col, align, x_ofs, -39);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);

    lv_obj_t *title_label = lv_label_create(col);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(THEME_NIGHT.dim), 0);

    *value_label = lv_label_create(col);
    lv_label_set_text(*value_label, "--");
    lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(*value_label, 74);
    lv_obj_align(*value_label, LV_ALIGN_TOP_MID, 0, 21);
    lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(*value_label, lv_color_hex(THEME_NIGHT.text), 0);
    lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_CENTER, 0);

    return col;
}

static void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    weather_screen = scr;

    ui_create_decor(scr);

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

    label_hour = lv_label_create(scr);
    lv_label_set_text(label_hour, "--");
    lv_obj_align(label_hour, LV_ALIGN_TOP_LEFT, 29, 45);
    lv_obj_set_style_text_font(label_hour, &lv_font_montserrat_48, 0);

    label_colon = lv_label_create(scr);
    lv_label_set_text(label_colon, ":");
    lv_obj_align(label_colon, LV_ALIGN_TOP_LEFT, 90, 45);
    lv_obj_set_style_text_font(label_colon, &lv_font_montserrat_48, 0);

    label_minute = lv_label_create(scr);
    lv_label_set_text(label_minute, "--");
    lv_obj_align(label_minute, LV_ALIGN_TOP_LEFT, 108, 45);
    lv_obj_set_style_text_font(label_minute, &lv_font_montserrat_48, 0);

    label_second = lv_label_create(scr);
    lv_label_set_text(label_second, "--");
    lv_obj_align(label_second, LV_ALIGN_TOP_LEFT, 172, 61);
    lv_obj_set_style_text_font(label_second, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_opa(label_second, LV_OPA_70, 0);

    ui_create_weather_icon(scr);

    label_temp = lv_label_create(scr);
    lv_label_set_long_mode(label_temp, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label_temp, 144);
    lv_label_set_text(label_temp, "--\xC2\xB0");
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 37, -17);
    lv_obj_set_style_text_align(label_temp, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_48, 0);

    label_desc = lv_label_create(scr);
    lv_label_set_long_mode(label_desc, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label_desc, 210);
    lv_label_set_text(label_desc, "Updating weather");
    lv_obj_align(label_desc, LV_ALIGN_CENTER, 0, 38);
    lv_obj_set_style_text_align(label_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_desc, &lv_font_montserrat_16, 0);

    create_dash_col(scr, "Feel", &label_feels, LV_ALIGN_BOTTOM_LEFT, 10);
    create_dash_col(scr, "Hum", &label_hum, LV_ALIGN_BOTTOM_MID, 0);
    create_dash_col(scr, "Wind", &label_wind, LV_ALIGN_BOTTOM_RIGHT, -10);
    lv_obj_set_width(label_wind, 82);
    lv_obj_align(label_wind, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_style_text_font(label_wind, &lv_font_montserrat_14, 0);

    label_status = lv_label_create(scr);
    lv_label_set_long_mode(label_status, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label_status, 220);
    lv_label_set_text(label_status, "Booting");
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_align(label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);

    ui_apply_theme(false);
}

static void add_weather_switch_button(lv_obj_t *screen)
{
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 57, 24);
    lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -4, -3);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x123c24), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x2c8c4d), 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_add_event_cb(button, screen_tap_handler, LV_EVENT_PRESSED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "RADAR");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xd7ffe1), 0);
    lv_obj_center(label);
}

static void screen_tap_handler(lv_event_t *event)
{
    (void)event;
    if (!weather_screen || !radar_screen) {
        return;
    }

    showing_radar = !showing_radar;
    lv_screen_load_anim(showing_radar ? radar_screen : weather_screen,
                        LV_SCREEN_LOAD_ANIM_FADE_IN,
                        220,
                        0,
                        false);
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
        lv_label_set_text(label_status, "Updated");
        return;
    }

    time_t now = 0;
    struct tm ti = {0};
    char buf[32];

    time(&now);
    localtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "%H:%M", &ti);
    lv_label_set_text(label_status, buf);
}

static void ui_set_time_now(void)
{
    if (!label_hour || !label_colon || !label_minute || !label_second || !label_date) {
        return;
    }

    ui_set_wifi_status();

    if (!time_is_valid()) {
        lv_label_set_text(label_hour, "--");
        lv_label_set_text(label_colon, ":");
        lv_label_set_text(label_minute, "--");
        lv_label_set_text(label_second, "--");
        lv_label_set_text(label_date, "Waiting for time");
        return;
    }

    time_t now = 0;
    struct tm ti = {0};
    char buf[24];

    time(&now);
    localtime_r(&now, &ti);

    strftime(buf, sizeof(buf), "%H", &ti);
    lv_label_set_text(label_hour, buf);

    lv_label_set_text(label_colon, (ti.tm_sec % 2) == 0 ? ":" : " ");

    strftime(buf, sizeof(buf), "%M", &ti);
    lv_label_set_text(label_minute, buf);

    strftime(buf, sizeof(buf), "%S", &ti);
    lv_label_set_text(label_second, buf);

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
    ESP_ERROR_CHECK_WITHOUT_ABORT(display_set_brightness(is_day ? 100 : 35));
    ui_set_wifi_status();
    ui_set_weather_icon(weather_icon_kind(w));

    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", w->temp_c);
    lv_label_set_text(label_temp, buf);

    char desc[64];
    snprintf(desc, sizeof(desc), "%s", w->description[0] ? w->description : "No description");
    if (desc[0] >= 'a' && desc[0] <= 'z') {
        desc[0] -= ('a' - 'A');
    }
    lv_label_set_text(label_desc, desc);

    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", w->feels_like_c);
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
            ui_set_status("Offline");
            ui_set_wifi_status();
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        weather_info_t w;
        memset(&w, 0, sizeof(w));

        lvgl_port_lock(portMAX_DELAY);
        ui_set_status("Fetching...");
        ui_set_wifi_status();
        lvgl_port_unlock();

        xSemaphoreTake(api_mutex, portMAX_DELAY);
        esp_err_t ret = weather_fetch_current(
            OWM_API_KEY,
            OWM_CITY,
            OWM_COUNTRY,
            OWM_LANG,
            &w,
            12000);
        xSemaphoreGive(api_mutex);

        lvgl_port_lock(portMAX_DELAY);
        if (ret == ESP_OK) {
            ui_set_weather(&w);
            ui_set_updated_now();
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS((int)WEATHER_UPDATE_MINUTES * 60 * 1000));
        } else {
            ui_set_status("Weather error");
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(30000));
        }
    }
}

static void task_radar(void *arg)
{
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(8000));

    while (1) {
        if (!wifi_manager_is_connected()) {
            lvgl_port_lock(portMAX_DELAY);
            radar_ui_set_status("Waiting for Wi-Fi");
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        lvgl_port_lock(portMAX_DELAY);
        radar_ui_set_status("Updating live traffic");
        lvgl_port_unlock();

        radar_snapshot_t snapshot;
        xSemaphoreTake(api_mutex, portMAX_DELAY);
        esp_err_t ret = radar_fetch_nearby(
            RADAR_CENTER_LAT,
            RADAR_CENTER_LON,
            RADAR_RADIUS_KM,
            &snapshot,
            15000);
        xSemaphoreGive(api_mutex);

        lvgl_port_lock(portMAX_DELAY);
        if (ret == ESP_OK) {
            radar_ui_update(&snapshot);
        } else {
            radar_ui_set_status("Radar update failed");
        }
        lvgl_port_unlock();

        vTaskDelay(pdMS_TO_TICKS(ret == ESP_OK ? RADAR_UPDATE_SECONDS * 1000 : 15000));
    }
}

static void task_radar_details(void *arg)
{
    (void)arg;
    char last_callsign[12] = {0};
    uint32_t last_attempt = 0;

    while (1) {
        radar_aircraft_t selected = {0};
        lvgl_port_lock(portMAX_DELAY);
        bool have_selected = radar_ui_get_selected(&selected);
        lvgl_port_unlock();

        bool changed = have_selected && strcmp(last_callsign, selected.callsign) != 0;
        bool retry = have_selected && !selected.origin[0] &&
                     (lv_tick_get() - last_attempt) > 60000;
        if (changed || retry) {
            snprintf(last_callsign, sizeof(last_callsign), "%s", selected.callsign);
            last_attempt = lv_tick_get();
            lvgl_port_lock(portMAX_DELAY);
            radar_ui_set_selected_details(&selected, false);
            lvgl_port_unlock();

            xSemaphoreTake(api_mutex, portMAX_DELAY);
            (void)radar_enrich_aircraft(&selected, 10000);
            xSemaphoreGive(api_mutex);

            lvgl_port_lock(portMAX_DELAY);
            radar_ui_set_selected_details(&selected, true);
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void app_main(void)
{
    api_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(api_mutex ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(display_init());
    lvgl_port_lock(portMAX_DELAY);
    boot_ui_create();
    lvgl_port_unlock();
    boot_ui_step("Connecting Wi-Fi", 25);

    wifi_manager_cfg_t cfg = {
        .ssid = WIFI_SSID,
        .pass = WIFI_PASS,
        .use_wpa3_sae = false,
    };
    ESP_ERROR_CHECK(wifi_manager_start(&cfg));

    esp_err_t ret = wifi_manager_wait_connected(pdMS_TO_TICKS(30000));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi timeout, continuing without initial connection: %s", esp_err_to_name(ret));
        boot_ui_step("Wi-Fi offline", 45);
    } else {
        ESP_LOGI(TAG, "Wi-Fi connected");
        boot_ui_step("Wi-Fi connected", 50);
    }

    boot_ui_step("Synchronizing time", 65);
    sntp_time_init(APP_TIMEZONE);
    (void)sntp_time_wait(20, 1000);
    boot_ui_step(time_is_valid() ? "Time synchronized" : "Time pending", 78);
    boot_ui_step("Creating interface", 88);
    vTaskDelay(pdMS_TO_TICKS(250));

    lvgl_port_lock(portMAX_DELAY);
    ui_create();
    radar_screen = radar_ui_create(screen_tap_handler);
    add_weather_switch_button(weather_screen);
    showing_radar = true;
    lv_screen_load(radar_screen);
    ui_set_time_now();
    ui_set_status("Running");
    lvgl_port_unlock();

    BaseType_t task_ok = xTaskCreate(task_clock, "clock", 4096, NULL, 5, NULL);
    ESP_ERROR_CHECK(task_ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    task_ok = xTaskCreate(task_weather, "weather", 6144, NULL, 5, NULL);
    ESP_ERROR_CHECK(task_ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    task_ok = xTaskCreate(task_radar, "radar", 8192, NULL, 5, NULL);
    ESP_ERROR_CHECK(task_ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    task_ok = xTaskCreate(task_radar_details, "radar_details", 8192, NULL, 4, NULL);
    ESP_ERROR_CHECK(task_ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
