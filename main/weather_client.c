#include "weather_client.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "owm";

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} http_buf_t;

static bool is_url_unreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

static void url_encode_component(const char *in, char *out, size_t out_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;

    for (size_t i = 0; in[i] != 0 && o + 1 < out_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (is_url_unreserved(c)) {
            out[o++] = (char)c;
        } else if (o + 3 < out_len) {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0f];
        } else {
            break;
        }
    }

    out[o] = 0;
}

static esp_err_t http_buf_grow(http_buf_t *hb, size_t need_more)
{
    size_t need = hb->len + need_more + 1;
    if (need <= hb->cap) {
        return ESP_OK;
    }

    size_t new_cap = hb->cap ? hb->cap : 1024;
    while (new_cap < need) {
        new_cap *= 2;
    }

    if (new_cap > 8192) {
        ESP_LOGE(TAG, "HTTP response too large, max buffer size 8192 exceeded");
        return ESP_ERR_NO_MEM;
    }

    char *p = realloc(hb->buf, new_cap);
    if (!p) {
        return ESP_ERR_NO_MEM;
    }

    hb->buf = p;
    hb->cap = new_cap;
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_buf_t *hb = (http_buf_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0 && hb) {
        esp_err_t err = http_buf_grow(hb, (size_t)evt->data_len);
        if (err != ESP_OK) {
            return err;
        }

        memcpy(hb->buf + hb->len, evt->data, (size_t)evt->data_len);
        hb->len += (size_t)evt->data_len;
        hb->buf[hb->len] = 0;
    }

    return ESP_OK;
}

static esp_err_t parse_weather_json(const char *json, weather_info_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *main = cJSON_GetObjectItem(root, "main");
    cJSON *wind = cJSON_GetObjectItem(root, "wind");
    cJSON *sys = cJSON_GetObjectItem(root, "sys");
    cJSON *weather = cJSON_GetObjectItem(root, "weather");

    if (!cJSON_IsObject(main) || !cJSON_IsArray(weather) || cJSON_GetArraySize(weather) < 1) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *w0 = cJSON_GetArrayItem(weather, 0);

    cJSON *temp = cJSON_GetObjectItem(main, "temp");
    cJSON *feel = cJSON_GetObjectItem(main, "feels_like");
    cJSON *hum = cJSON_GetObjectItem(main, "humidity");
    cJSON *pres = cJSON_GetObjectItem(main, "pressure");

    out->temp_c = cJSON_IsNumber(temp) ? (float)temp->valuedouble : 0.0f;
    out->feels_like_c = cJSON_IsNumber(feel) ? (float)feel->valuedouble : 0.0f;
    out->humidity_pct = cJSON_IsNumber(hum) ? hum->valueint : 0;
    out->pressure_hpa = cJSON_IsNumber(pres) ? pres->valueint : 0;

    if (cJSON_IsObject(wind)) {
        cJSON *spd = cJSON_GetObjectItem(wind, "speed");
        cJSON *deg = cJSON_GetObjectItem(wind, "deg");
        out->wind_speed_ms = cJSON_IsNumber(spd) ? (float)spd->valuedouble : 0.0f;
        out->wind_deg = cJSON_IsNumber(deg) ? deg->valueint : 0;
    }

    cJSON *desc = cJSON_GetObjectItem(w0, "description");
    cJSON *icon = cJSON_GetObjectItem(w0, "icon");

    snprintf(out->description, sizeof(out->description), "%s",
             cJSON_IsString(desc) && desc->valuestring ? desc->valuestring : "");
    snprintf(out->icon, sizeof(out->icon), "%s",
             cJSON_IsString(icon) && icon->valuestring ? icon->valuestring : "");

    cJSON *dt = cJSON_GetObjectItem(root, "dt");
    cJSON *tz = cJSON_GetObjectItem(root, "timezone");
    out->dt = cJSON_IsNumber(dt) ? (int64_t)dt->valuedouble : 0;
    out->timezone_sec = cJSON_IsNumber(tz) ? tz->valueint : 0;

    if (cJSON_IsObject(sys)) {
        cJSON *sr = cJSON_GetObjectItem(sys, "sunrise");
        cJSON *ss = cJSON_GetObjectItem(sys, "sunset");
        out->sunrise = cJSON_IsNumber(sr) ? (int64_t)sr->valuedouble : 0;
        out->sunset = cJSON_IsNumber(ss) ? (int64_t)ss->valuedouble : 0;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t weather_fetch_current(const char *api_key,
                                const char *city,
                                const char *country,
                                const char *lang,
                                weather_info_t *out,
                                int timeout_ms)
{
    if (!api_key || !api_key[0] || !city || !city[0] || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    const char *lang_use = (lang && lang[0]) ? lang : "en";

    char city_enc[128];
    char country_enc[16];
    char lang_enc[16];
    url_encode_component(city, city_enc, sizeof(city_enc));
    url_encode_component(country && country[0] ? country : "", country_enc, sizeof(country_enc));
    url_encode_component(lang_use, lang_enc, sizeof(lang_enc));

    char q[160];
    int written = 0;
    if (country_enc[0]) {
        written = snprintf(q, sizeof(q), "%s,%s", city_enc, country_enc);
    } else {
        written = snprintf(q, sizeof(q), "%s", city_enc);
    }
    if (written < 0 || (size_t)written >= sizeof(q)) {
        ESP_LOGW(TAG, "OpenWeather query is too long");
        return ESP_ERR_INVALID_SIZE;
    }

    char url[384];
    written = snprintf(url, sizeof(url),
                       "https://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric&lang=%s",
                       q, api_key, lang_enc);
    if (written < 0 || (size_t)written >= sizeof(url)) {
        ESP_LOGW(TAG, "OpenWeather URL is too long");
        return ESP_ERR_INVALID_SIZE;
    }

    http_buf_t hb = {0};
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = timeout_ms > 0 ? timeout_ms : 10000,
        .event_handler = http_event_handler,
        .user_data = &hb,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    int64_t content_len = esp_http_client_get_content_length(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP perform failed: %s", esp_err_to_name(err));
        free(hb.buf);
        return err;
    }

    if (status != 200) {
        ESP_LOGW(TAG, "HTTP status=%d content_len=%" PRId64, status, content_len);
        if (hb.buf && hb.len) {
            ESP_LOGW(TAG, "Body: %.*s", (int)hb.len, hb.buf);
        }
        free(hb.buf);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (!hb.buf || hb.len == 0) {
        free(hb.buf);
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = parse_weather_json(hb.buf, out);
    free(hb.buf);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "JSON parse failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}
