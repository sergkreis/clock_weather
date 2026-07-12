#pragma once

#include "lvgl.h"
#include "radar_client.h"

lv_obj_t *radar_ui_create(lv_event_cb_t switch_callback);
void radar_ui_set_status(const char *status);
void radar_ui_update(const radar_snapshot_t *snapshot);
bool radar_ui_get_selected(radar_aircraft_t *aircraft);
void radar_ui_set_selected_details(const radar_aircraft_t *aircraft, bool resolved);
