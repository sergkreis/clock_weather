#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temp_c;
    float feels_like_c;
    int   humidity_pct;
    int   pressure_hpa;

    float wind_speed_ms;
    int   wind_deg;

    char  description[64];
    char  icon[8];

    int64_t dt;
    int64_t sunrise;
    int64_t sunset;
    int     timezone_sec;
} weather_info_t;

esp_err_t weather_fetch_current(const char *api_key,
                                const char *city,
                                const char *country,
                                const char *lang,
                                weather_info_t *out,
                                int timeout_ms);

#ifdef __cplusplus
}
#endif
