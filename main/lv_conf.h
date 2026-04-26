#pragma once

// Make LVGL include this project-local config.
#define LV_CONF_INCLUDE_SIMPLE 1

// Keep LVGL logs off for the device build.
#define LV_USE_LOG 0

// 16-bit RGB565 color.
#define LV_COLOR_DEPTH 16

// Use LVGL's built-in allocator.
#define LV_MEM_CUSTOM 0

// Fonts enabled in sdkconfig.defaults.
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1

// Default font.
#define LV_FONT_DEFAULT &lv_font_montserrat_14
