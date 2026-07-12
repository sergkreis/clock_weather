#pragma once

#include "wifi_secrets.h"

// OpenWeather
#define OWM_CITY        "Quedlinburg"
#define OWM_COUNTRY     "DE"
#define OWM_LANG        "en"

// Weather refresh interval.
#define WEATHER_UPDATE_MINUTES  20

// POSIX timezone for Germany.
#define APP_TIMEZONE    "CET-1CEST,M3.5.0,M10.5.0/3"

// ADS-B radar centered on Quedlinburg.
#define RADAR_CENTER_LAT        51.7870
#define RADAR_CENTER_LON        11.1410
#define RADAR_RADIUS_KM         50.0f
#define RADAR_UPDATE_SECONDS    10
