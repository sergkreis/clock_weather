#include "sntp_time.h"

#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "sntp";

void sntp_time_init(const char *tz)
{
    if (tz && tz[0]) {
        setenv("TZ", tz, 1);
        tzset();
        ESP_LOGI(TAG, "TZ set to %s", tz);
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

esp_err_t sntp_time_wait(int tries, int delay_ms)
{
    if (tries <= 0) tries = 15;
    if (delay_ms <= 0) delay_ms = 1000;

    for (int i = 1; i <= tries; i++) {
        if (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
            ESP_LOGI(TAG, "time synced");
            return ESP_OK;
        }
        ESP_LOGI(TAG, "waiting for time sync... (%d/%d)", i, tries);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    ESP_LOGW(TAG, "time sync timeout");
    return ESP_ERR_TIMEOUT;
}