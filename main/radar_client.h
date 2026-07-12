#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define RADAR_MAX_AIRCRAFT 20

typedef struct {
    char hex[8];
    char callsign[12];
    char type[8];
    char registration[12];
    char aircraft_name[32];
    char manufacturer[20];
    char airline[28];
    char origin[24];
    char destination[24];
    float latitude;
    float longitude;
    float altitude_ft;
    float speed_kt;
    float track_deg;
    float distance_km;
    float bearing_deg;
    bool on_ground;
} radar_aircraft_t;

typedef struct {
    radar_aircraft_t aircraft[RADAR_MAX_AIRCRAFT];
    size_t count;
    size_t total_reported;
    int64_t source_timestamp_ms;
} radar_snapshot_t;

esp_err_t radar_fetch_nearby(double center_lat,
                             double center_lon,
                             float radius_km,
                             radar_snapshot_t *out,
                             int timeout_ms);
esp_err_t radar_enrich_aircraft(radar_aircraft_t *aircraft, int timeout_ms);
