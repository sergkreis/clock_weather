#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void sntp_time_init(const char *tz);
esp_err_t sntp_time_wait(int tries, int delay_ms);

#ifdef __cplusplus
}
#endif