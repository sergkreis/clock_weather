#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *ssid;
    const char *pass;
    bool        use_wpa3_sae;
} wifi_manager_cfg_t;

esp_err_t wifi_manager_start(const wifi_manager_cfg_t *cfg);
esp_err_t wifi_manager_wait_connected(TickType_t ticks_to_wait);
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
