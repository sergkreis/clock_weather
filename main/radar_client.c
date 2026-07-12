#include "radar_client.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "radar_api";

#define RADAR_HTTP_MAX_RESPONSE (64 * 1024)
#define DEG_TO_RAD (0.01745329251994329577)
#define RAD_TO_DEG (57.295779513082320876)
#define EARTH_RADIUS_KM (6371.0)

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} response_buffer_t;

static esp_err_t response_grow(response_buffer_t *buffer, size_t extra)
{
    size_t required = buffer->len + extra + 1;
    if (required <= buffer->capacity) {
        return ESP_OK;
    }

    size_t capacity = buffer->capacity ? buffer->capacity : 4096;
    while (capacity < required) {
        capacity *= 2;
    }
    if (capacity > RADAR_HTTP_MAX_RESPONSE) {
        return ESP_ERR_NO_MEM;
    }

    char *next = realloc(buffer->data, capacity);
    if (!next) {
        return ESP_ERR_NO_MEM;
    }
    buffer->data = next;
    buffer->capacity = capacity;
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    response_buffer_t *buffer = (response_buffer_t *)event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0 || !buffer) {
        return ESP_OK;
    }

    esp_err_t err = response_grow(buffer, (size_t)event->data_len);
    if (err != ESP_OK) {
        return err;
    }

    memcpy(buffer->data + buffer->len, event->data, (size_t)event->data_len);
    buffer->len += (size_t)event->data_len;
    buffer->data[buffer->len] = 0;
    return ESP_OK;
}

static double json_number(const cJSON *object, const char *name, double fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

static void copy_json_string(char *dest, size_t dest_size, const cJSON *object, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    const char *value = cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
    snprintf(dest, dest_size, "%s", value);

    size_t len = strlen(dest);
    while (len > 0 && dest[len - 1] == ' ') {
        dest[--len] = 0;
    }
}

static void calculate_position(double center_lat,
                               double center_lon,
                               double lat,
                               double lon,
                               float *distance_km,
                               float *bearing_deg)
{
    double lat1 = center_lat * DEG_TO_RAD;
    double lat2 = lat * DEG_TO_RAD;
    double delta_lat = (lat - center_lat) * DEG_TO_RAD;
    double delta_lon = (lon - center_lon) * DEG_TO_RAD;

    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1) * cos(lat2) * sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    double distance = EARTH_RADIUS_KM * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    double y = sin(delta_lon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(delta_lon);
    double bearing = atan2(y, x) * RAD_TO_DEG;
    if (bearing < 0.0) {
        bearing += 360.0;
    }

    *distance_km = (float)distance;
    *bearing_deg = (float)bearing;
}

static void insert_by_distance(radar_snapshot_t *snapshot, const radar_aircraft_t *aircraft)
{
    size_t index = snapshot->count;
    if (index < RADAR_MAX_AIRCRAFT) {
        snapshot->count++;
    } else if (aircraft->distance_km >= snapshot->aircraft[RADAR_MAX_AIRCRAFT - 1].distance_km) {
        return;
    } else {
        index = RADAR_MAX_AIRCRAFT - 1;
    }

    while (index > 0 && snapshot->aircraft[index - 1].distance_km > aircraft->distance_km) {
        if (index < RADAR_MAX_AIRCRAFT) {
            snapshot->aircraft[index] = snapshot->aircraft[index - 1];
        }
        index--;
    }
    snapshot->aircraft[index] = *aircraft;
}

static esp_err_t parse_snapshot(const char *json,
                                double center_lat,
                                double center_lon,
                                float radius_km,
                                radar_snapshot_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *aircraft_array = cJSON_GetObjectItemCaseSensitive(root, "ac");
    if (!cJSON_IsArray(aircraft_array)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    out->source_timestamp_ms = (int64_t)json_number(root, "now", 0);
    out->total_reported = (size_t)cJSON_GetArraySize(aircraft_array);

    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, aircraft_array) {
        const cJSON *lat_item = cJSON_GetObjectItemCaseSensitive(item, "lat");
        const cJSON *lon_item = cJSON_GetObjectItemCaseSensitive(item, "lon");
        if (!cJSON_IsNumber(lat_item) || !cJSON_IsNumber(lon_item)) {
            continue;
        }

        radar_aircraft_t aircraft = {0};
        aircraft.latitude = (float)lat_item->valuedouble;
        aircraft.longitude = (float)lon_item->valuedouble;
        calculate_position(center_lat, center_lon, aircraft.latitude, aircraft.longitude,
                           &aircraft.distance_km, &aircraft.bearing_deg);
        if (aircraft.distance_km > radius_km) {
            continue;
        }

        copy_json_string(aircraft.callsign, sizeof(aircraft.callsign), item, "flight");
        copy_json_string(aircraft.hex, sizeof(aircraft.hex), item, "hex");
        copy_json_string(aircraft.type, sizeof(aircraft.type), item, "t");
        copy_json_string(aircraft.registration, sizeof(aircraft.registration), item, "r");

        const cJSON *altitude = cJSON_GetObjectItemCaseSensitive(item, "alt_baro");
        aircraft.on_ground = cJSON_IsString(altitude) && strcmp(altitude->valuestring, "ground") == 0;
        aircraft.altitude_ft = cJSON_IsNumber(altitude) ? (float)altitude->valuedouble : 0.0f;
        aircraft.speed_kt = (float)json_number(item, "gs", 0.0);
        aircraft.track_deg = (float)json_number(item, "track", 0.0);

        if (!aircraft.callsign[0]) {
            copy_json_string(aircraft.callsign, sizeof(aircraft.callsign), item, "hex");
        }
        insert_by_distance(out, &aircraft);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t radar_fetch_nearby(double center_lat,
                             double center_lon,
                             float radius_km,
                             radar_snapshot_t *out,
                             int timeout_ms)
{
    if (!out || radius_km <= 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    int radius_nm = (int)ceilf(radius_km / 1.852f);
    char url[192];
    snprintf(url, sizeof(url),
             "https://api.adsb.lol/v2/lat/%.5f/lon/%.5f/dist/%d",
             center_lat, center_lon, radius_nm);

    response_buffer_t response = {0};
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = timeout_ms > 0 ? timeout_ms : 12000,
        .event_handler = http_event_handler,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
        .user_agent = "clock-weather-cyd/1.0",
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "request failed: %s", esp_err_to_name(err));
        free(response.data);
        return err;
    }
    if (status != 200 || !response.data || response.len == 0) {
        ESP_LOGW(TAG, "unexpected HTTP status: %d", status);
        free(response.data);
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = parse_snapshot(response.data, center_lat, center_lon, radius_km, out);
    free(response.data);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "aircraft: %u nearby, %u reported",
                 (unsigned)out->count, (unsigned)out->total_reported);
    }
    return err;
}

static void copy_item_string(char *dest, size_t dest_size, const cJSON *object, const char *name)
{
    const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
    snprintf(dest, dest_size, "%s",
             cJSON_IsString(item) && item->valuestring ? item->valuestring : "");
}

static void copy_airport_label(char *dest, size_t dest_size, const cJSON *airport)
{
    char code[8];
    char city[20];
    copy_item_string(code, sizeof(code), airport, "iata_code");
    if (!code[0]) copy_item_string(code, sizeof(code), airport, "iata");
    if (!code[0]) copy_item_string(code, sizeof(code), airport, "icao_code");
    if (!code[0]) copy_item_string(code, sizeof(code), airport, "icao");
    copy_item_string(city, sizeof(city), airport, "municipality");
    if (!city[0]) copy_item_string(city, sizeof(city), airport, "location");
    if (code[0] && city[0]) {
        snprintf(dest, dest_size, "%s %s", code, city);
    } else {
        snprintf(dest, dest_size, "%s", code[0] ? code : city);
    }
}

static cJSON *fetch_json(const char *url, int timeout_ms, int *http_status)
{
    response_buffer_t response = {0};
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = timeout_ms > 0 ? timeout_ms : 10000,
        .event_handler = http_event_handler,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
        .user_agent = "clock-weather-cyd/1.0",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return NULL;
    esp_err_t err = esp_http_client_perform(client);
    *http_status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || *http_status != 200 || !response.data) {
        free(response.data);
        return NULL;
    }
    cJSON *root = cJSON_Parse(response.data);
    free(response.data);
    return root;
}

static void parse_route(radar_aircraft_t *aircraft, const cJSON *root)
{
    const cJSON *payload = root ? cJSON_GetObjectItemCaseSensitive(root, "response") : NULL;
    const cJSON *route = payload ? cJSON_GetObjectItemCaseSensitive(payload, "flightroute") : NULL;
    const cJSON *airline = route ? cJSON_GetObjectItemCaseSensitive(route, "airline") : NULL;
    copy_item_string(aircraft->airline, sizeof(aircraft->airline), airline, "name");
    copy_airport_label(aircraft->origin, sizeof(aircraft->origin),
                       route ? cJSON_GetObjectItemCaseSensitive(route, "origin") : NULL);
    copy_airport_label(aircraft->destination, sizeof(aircraft->destination),
                       route ? cJSON_GetObjectItemCaseSensitive(route, "destination") : NULL);
}

static bool fetch_adsblol_route(radar_aircraft_t *aircraft, int timeout_ms)
{
    char body[128];
    snprintf(body, sizeof(body),
             "{\"planes\":[{\"callsign\":\"%s\",\"lat\":%.5f,\"lng\":%.5f}]}",
             aircraft->callsign, aircraft->latitude, aircraft->longitude);
    response_buffer_t response = {0};
    esp_http_client_config_t config = {
        .url = "https://adsb.im/api/0/routeset",
        .method = HTTP_METHOD_POST,
        .timeout_ms = timeout_ms > 0 ? timeout_ms : 10000,
        .event_handler = http_event_handler,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
        .user_agent = "clock-weather-cyd/1.0",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || status < 200 || status >= 300 || !response.data) {
        free(response.data);
        ESP_LOGI(TAG, "adsb.im route HTTP %d", status);
        return false;
    }
    cJSON *root = cJSON_Parse(response.data);
    free(response.data);
    if (!cJSON_IsArray(root) || cJSON_GetArraySize(root) == 0) {
        cJSON_Delete(root);
        return false;
    }
    const cJSON *result = cJSON_GetArrayItem(root, 0);
    const cJSON *airports = cJSON_GetObjectItemCaseSensitive(result, "_airports");
    int count = cJSON_IsArray(airports) ? cJSON_GetArraySize(airports) : 0;
    if (count >= 2) {
        copy_airport_label(aircraft->origin, sizeof(aircraft->origin), cJSON_GetArrayItem(airports, 0));
        copy_airport_label(aircraft->destination, sizeof(aircraft->destination),
                           cJSON_GetArrayItem(airports, count - 1));
    }
    cJSON_Delete(root);
    return aircraft->origin[0] && aircraft->destination[0];
}

esp_err_t radar_enrich_aircraft(radar_aircraft_t *aircraft, int timeout_ms)
{
    if (!aircraft || !aircraft->callsign[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    char url[192];
    int aircraft_status = 0;
    if (aircraft->hex[0] || aircraft->registration[0]) {
        vTaskDelay(pdMS_TO_TICKS(50));
        snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/aircraft/%s",
                 aircraft->hex[0] ? aircraft->hex : aircraft->registration);
        cJSON *root = fetch_json(url, timeout_ms, &aircraft_status);
        if (root) {
            const cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "response");
            const cJSON *details = payload ? cJSON_GetObjectItemCaseSensitive(payload, "aircraft") : NULL;
            copy_item_string(aircraft->aircraft_name, sizeof(aircraft->aircraft_name), details, "type");
            copy_item_string(aircraft->manufacturer, sizeof(aircraft->manufacturer), details, "manufacturer");
            cJSON_Delete(root);
        }
    }

    int route_status = 0;
    vTaskDelay(pdMS_TO_TICKS(50));
    snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", aircraft->callsign);
    cJSON *route_root = fetch_json(url, timeout_ms, &route_status);
    if (route_root) {
        parse_route(aircraft, route_root);
        cJSON_Delete(route_root);
    }
    if (!aircraft->origin[0] || !aircraft->destination[0]) {
        vTaskDelay(pdMS_TO_TICKS(50));
        (void)fetch_adsblol_route(aircraft, timeout_ms);
    }
    ESP_LOGI(TAG, "details: %s, aircraft HTTP %d, route HTTP %d, %s -> %s",
             aircraft->callsign, aircraft_status, route_status,
             aircraft->origin[0] ? aircraft->origin : "?",
             aircraft->destination[0] ? aircraft->destination : "?");
    return (aircraft->aircraft_name[0] || aircraft->origin[0]) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
