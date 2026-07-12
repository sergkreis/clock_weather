#include "radar_ui.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define RADAR_CENTER_X 120
#define RADAR_CENTER_Y 112
#define RADAR_RADIUS_PX 88
#define DEG_TO_RAD 0.01745329251994329577f
#define RADAR_LABEL_COUNT 5
#define MAX_TRACK_POINTS 24
#define TRACK_SAMPLE_INTERVAL 3
#define LOW_ALTITUDE_FT 10000.0f

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_16);

typedef enum {
    FILTER_ALL,
    FILTER_AIRBORNE,
    FILTER_LOW,
} radar_filter_t;

typedef struct {
    lv_obj_t *hitbox;
    lv_obj_t *marker;
    lv_obj_t *callsign;
} plane_widget_t;

typedef struct {
    float distance_km;
    float bearing_deg;
} trail_point_t;

typedef struct {
    char callsign[12];
    trail_point_t points[MAX_TRACK_POINTS];
    size_t count;
    uint8_t sample_counter;
    bool active;
    bool seen;
} aircraft_track_t;

static const float s_ranges[] = {10.0f, 25.0f, 50.0f};
static lv_obj_t *s_screen;
static lv_obj_t *s_count_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_selected_name;
static lv_obj_t *s_selected_type;
static lv_obj_t *s_selected_position;
static lv_obj_t *s_selected_motion;
static lv_obj_t *s_range_label;
static lv_obj_t *s_filter_label;
static lv_obj_t *s_path_label;
static lv_obj_t *s_track_lines[RADAR_MAX_AIRCRAFT];
static lv_point_precise_t s_track_line_points[RADAR_MAX_AIRCRAFT][MAX_TRACK_POINTS + 1];
static aircraft_track_t s_tracks[RADAR_MAX_AIRCRAFT];
static plane_widget_t s_planes[RADAR_MAX_AIRCRAFT];
static radar_snapshot_t s_snapshot;
static size_t s_selected;
static uint32_t s_snapshot_tick;
static uint8_t s_range_index = 2;
static radar_filter_t s_filter = FILTER_ALL;
static bool s_details_resolved;
static bool s_paths_visible;

static lv_color_t altitude_color(float altitude_ft)
{
    if (altitude_ft < 5000.0f) return lv_color_hex(0x55ef87);
    if (altitude_ft < 15000.0f) return lv_color_hex(0xffc857);
    return lv_color_hex(0x4cc9f0);
}

static bool aircraft_visible(const radar_aircraft_t *aircraft)
{
    if (aircraft->distance_km > s_ranges[s_range_index]) return false;
    if (s_filter == FILTER_AIRBORNE) return !aircraft->on_ground;
    if (s_filter == FILTER_LOW) return !aircraft->on_ground && aircraft->altitude_ft < LOW_ALTITUDE_FT;
    return true;
}

static lv_obj_t *create_text(lv_obj_t *parent, const char *text, const lv_font_t *font,
                             lv_color_t color, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static lv_obj_t *create_line(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, width, height);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x174b2b), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

static lv_obj_t *create_ring(lv_obj_t *parent, int diameter)
{
    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_set_size(ring, diameter, diameter);
    lv_obj_set_pos(ring, RADAR_CENTER_X - diameter / 2, RADAR_CENTER_Y - diameter / 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x227a42), 0);
    lv_obj_set_style_pad_all(ring, 0, 0);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return ring;
}

static lv_obj_t *create_button(lv_obj_t *parent, int x, int width, const char *text,
                               lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, 291);
    lv_obj_set_size(button, width, 25);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x123c24), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x2c8c4d), 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    if (callback) {
        lv_obj_add_event_cb(button, callback, LV_EVENT_PRESSED, NULL);
    }
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xd7ffe1), 0);
    lv_obj_center(label);
    return label;
}

static void projected_position(const radar_aircraft_t *aircraft, float *distance, float *bearing)
{
    float elapsed_s = (float)(lv_tick_get() - s_snapshot_tick) / 1000.0f;
    if (elapsed_s > 15.0f) elapsed_s = 15.0f;
    float old_rad = aircraft->bearing_deg * DEG_TO_RAD;
    float east = sinf(old_rad) * aircraft->distance_km;
    float north = cosf(old_rad) * aircraft->distance_km;
    float travelled = aircraft->speed_kt * 1.852f * elapsed_s / 3600.0f;
    float track = aircraft->track_deg * DEG_TO_RAD;
    east += sinf(track) * travelled;
    north += cosf(track) * travelled;
    *distance = sqrtf(east * east + north * north);
    *bearing = atan2f(east, north) / DEG_TO_RAD;
    if (*bearing < 0.0f) *bearing += 360.0f;
}

static void polar_to_screen(float distance, float bearing, int *x, int *y)
{
    float radius = distance / s_ranges[s_range_index] * RADAR_RADIUS_PX;
    float radians = bearing * DEG_TO_RAD;
    *x = RADAR_CENTER_X + (int)(sinf(radians) * radius);
    *y = RADAR_CENTER_Y - (int)(cosf(radians) * radius);
}

static const radar_aircraft_t *find_aircraft(const char *callsign)
{
    for (size_t i = 0; i < s_snapshot.count; i++) {
        if (strcmp(callsign, s_snapshot.aircraft[i].callsign) == 0) {
            return &s_snapshot.aircraft[i];
        }
    }
    return NULL;
}

static void update_tracks(const radar_snapshot_t *snapshot)
{
    for (size_t i = 0; i < RADAR_MAX_AIRCRAFT; i++) s_tracks[i].seen = false;

    for (size_t i = 0; i < snapshot->count; i++) {
        const radar_aircraft_t *aircraft = &snapshot->aircraft[i];
        aircraft_track_t *track = NULL;
        aircraft_track_t *empty = NULL;
        for (size_t j = 0; j < RADAR_MAX_AIRCRAFT; j++) {
            if (!s_tracks[j].active && !empty) empty = &s_tracks[j];
            if (s_tracks[j].active && strcmp(s_tracks[j].callsign, aircraft->callsign) == 0) {
                track = &s_tracks[j];
                break;
            }
        }
        if (!track) {
            track = empty;
            if (!track) continue;
            memset(track, 0, sizeof(*track));
            track->active = true;
            snprintf(track->callsign, sizeof(track->callsign), "%s", aircraft->callsign);
        }
        track->seen = true;
        if (track->count > 0 && ++track->sample_counter < TRACK_SAMPLE_INTERVAL) continue;
        track->sample_counter = 0;
        if (track->count == MAX_TRACK_POINTS) {
            memmove(&track->points[0], &track->points[1],
                    sizeof(track->points[0]) * (MAX_TRACK_POINTS - 1));
            track->count--;
        }
        track->points[track->count++] = (trail_point_t){
            .distance_km = aircraft->distance_km,
            .bearing_deg = aircraft->bearing_deg,
        };
    }

    for (size_t i = 0; i < RADAR_MAX_AIRCRAFT; i++) {
        if (s_tracks[i].active && !s_tracks[i].seen) memset(&s_tracks[i], 0, sizeof(s_tracks[i]));
    }
}

static void set_selected_details(void)
{
    if (s_snapshot.count == 0 || s_selected >= s_snapshot.count) {
        lv_label_set_text(s_selected_name, "No aircraft in filter");
        lv_label_set_text(s_selected_type, "");
        lv_label_set_text(s_selected_position, "Tap RANGE or FILTER");
        lv_label_set_text(s_selected_motion, "");
        return;
    }

    const radar_aircraft_t *aircraft = &s_snapshot.aircraft[s_selected];
    char details[64];
    char route[64];
    char motion[48];
    lv_label_set_text(s_selected_name, aircraft->callsign);
    if (aircraft->aircraft_name[0]) {
        snprintf(details, sizeof(details), "%s%s%s", aircraft->manufacturer,
                 aircraft->manufacturer[0] ? " " : "", aircraft->aircraft_name);
    } else {
        snprintf(details, sizeof(details), "%s%s%s", aircraft->type,
                 aircraft->registration[0] ? "  " : "", aircraft->registration);
    }
    lv_label_set_text(s_selected_type, details);
    if (aircraft->origin[0] && aircraft->destination[0]) {
        snprintf(route, sizeof(route), "%s > %s", aircraft->origin, aircraft->destination);
    } else if (!s_details_resolved) {
        snprintf(route, sizeof(route), "Loading route...");
    } else {
        snprintf(route, sizeof(route), "Route unavailable  %.1f km", aircraft->distance_km);
    }
    lv_label_set_text(s_selected_position, route);
    if (aircraft->on_ground) {
        snprintf(motion, sizeof(motion), "GROUND  %.0f km/h", aircraft->speed_kt * 1.852f);
    } else {
        snprintf(motion, sizeof(motion), "%.0f m  %.0f km/h",
                 aircraft->altitude_ft * 0.3048f, aircraft->speed_kt * 1.852f);
    }
    lv_label_set_text(s_selected_motion, motion);
}

static void render_radar(void)
{
    size_t visible_count = 0;
    size_t first_visible = RADAR_MAX_AIRCRAFT;
    bool selected_visible = false;
    for (size_t i = 0; i < RADAR_MAX_AIRCRAFT; i++) {
        lv_obj_add_flag(s_planes[i].hitbox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_planes[i].callsign, LV_OBJ_FLAG_HIDDEN);
    }

    for (size_t i = 0; i < s_snapshot.count; i++) {
        const radar_aircraft_t *aircraft = &s_snapshot.aircraft[i];
        if (!aircraft_visible(aircraft)) continue;
        if (first_visible == RADAR_MAX_AIRCRAFT) first_visible = i;
        if (i == s_selected) selected_visible = true;
        visible_count++;
        float distance;
        float bearing;
        int x;
        int y;
        projected_position(aircraft, &distance, &bearing);
        polar_to_screen(distance, bearing, &x, &y);

        lv_obj_set_pos(s_planes[i].hitbox, x - 12, y - 12);
        lv_obj_set_style_text_color(s_planes[i].marker,
                                    i == s_selected ? lv_color_hex(0xffffff)
                                                    : altitude_color(aircraft->altitude_ft), 0);
        lv_obj_set_style_transform_rotation(s_planes[i].marker,
                                            (int32_t)(aircraft->track_deg * 10.0f), 0);
        lv_obj_remove_flag(s_planes[i].hitbox, LV_OBJ_FLAG_HIDDEN);
        if (i < RADAR_LABEL_COUNT) {
            lv_label_set_text(s_planes[i].callsign, aircraft->callsign);
            lv_obj_set_pos(s_planes[i].callsign, x > 180 ? x - 54 : x + 7, y - 7);
            lv_obj_remove_flag(s_planes[i].callsign, LV_OBJ_FLAG_HIDDEN);
        }
    }

    char count[24];
    snprintf(count, sizeof(count), "%u/%u aircraft", (unsigned)visible_count,
             (unsigned)s_snapshot.count);
    lv_label_set_text(s_count_label, count);

    if (!selected_visible && first_visible < s_snapshot.count) {
        s_selected = first_visible;
    }

    for (size_t i = 0; i < RADAR_MAX_AIRCRAFT; i++) {
        lv_obj_add_flag(s_track_lines[i], LV_OBJ_FLAG_HIDDEN);
        if (!s_paths_visible || !s_tracks[i].active) continue;
        const radar_aircraft_t *aircraft = find_aircraft(s_tracks[i].callsign);
        if (!aircraft || !aircraft_visible(aircraft)) continue;
        uint32_t point_count = 0;
        for (size_t j = 0; j < s_tracks[i].count; j++) {
            if (s_tracks[i].points[j].distance_km > s_ranges[s_range_index]) continue;
            int x;
            int y;
            polar_to_screen(s_tracks[i].points[j].distance_km,
                            s_tracks[i].points[j].bearing_deg, &x, &y);
            s_track_line_points[i][point_count++] = (lv_point_precise_t){.x = x, .y = y};
        }
        float distance;
        float bearing;
        int x;
        int y;
        projected_position(aircraft, &distance, &bearing);
        if (distance <= s_ranges[s_range_index] && point_count < MAX_TRACK_POINTS + 1) {
            polar_to_screen(distance, bearing, &x, &y);
            s_track_line_points[i][point_count++] = (lv_point_precise_t){.x = x, .y = y};
        }
        if (point_count >= 2) {
            lv_line_set_points(s_track_lines[i], s_track_line_points[i], point_count);
            lv_obj_remove_flag(s_track_lines[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    set_selected_details();
}

static void plane_clicked(lv_event_t *event)
{
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (index >= s_snapshot.count) return;
    s_selected = index;
    s_details_resolved = false;
    render_radar();
}

static void range_clicked(lv_event_t *event)
{
    (void)event;
    s_range_index = (uint8_t)((s_range_index + 1) % 3);
    char text[12];
    snprintf(text, sizeof(text), "%.0f KM", s_ranges[s_range_index]);
    lv_label_set_text(s_range_label, text);
    render_radar();
}

static void filter_clicked(lv_event_t *event)
{
    (void)event;
    static const char *labels[] = {"ALL", "AIR", "LOW"};
    s_filter = (radar_filter_t)((s_filter + 1) % 3);
    lv_label_set_text(s_filter_label, labels[s_filter]);
    render_radar();
}

static void path_clicked(lv_event_t *event)
{
    (void)event;
    s_paths_visible = !s_paths_visible;
    lv_obj_set_style_text_color(s_path_label,
                                lv_color_hex(s_paths_visible ? 0xffffff : 0x8abf98), 0);
    render_radar();
}

static void animation_timer(lv_timer_t *timer)
{
    (void)timer;
    if (s_snapshot.count > 0) render_radar();
}

lv_obj_t *radar_ui_create(lv_event_cb_t switch_callback)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x020705), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    create_text(s_screen, "QLB RADAR", &lv_font_montserrat_16, lv_color_hex(0x55ef87), 8, 4);
    s_count_label = create_text(s_screen, "-- aircraft", &lv_font_montserrat_12,
                                lv_color_hex(0x8abf98), 150, 6);
    create_line(s_screen, RADAR_CENTER_X, RADAR_CENTER_Y - RADAR_RADIUS_PX, 1, RADAR_RADIUS_PX * 2);
    create_line(s_screen, RADAR_CENTER_X - RADAR_RADIUS_PX, RADAR_CENTER_Y, RADAR_RADIUS_PX * 2, 1);
    create_ring(s_screen, 36);
    create_ring(s_screen, 88);
    create_ring(s_screen, RADAR_RADIUS_PX * 2);
    create_text(s_screen, "N", &lv_font_montserrat_12, lv_color_hex(0x55ef87), 116, 17);
    create_text(s_screen, "S", &lv_font_montserrat_12, lv_color_hex(0x55ef87), 116, 197);
    create_text(s_screen, "W", &lv_font_montserrat_12, lv_color_hex(0x55ef87), 21, 106);
    create_text(s_screen, "E", &lv_font_montserrat_12, lv_color_hex(0x55ef87), 210, 106);

    lv_obj_t *center = lv_obj_create(s_screen);
    lv_obj_set_size(center, 7, 7);
    lv_obj_set_pos(center, RADAR_CENTER_X - 3, RADAR_CENTER_Y - 3);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center, lv_color_hex(0x55ef87), 0);
    lv_obj_set_style_border_width(center, 0, 0);
    lv_obj_remove_flag(center, LV_OBJ_FLAG_CLICKABLE);

    for (size_t i = 0; i < RADAR_MAX_AIRCRAFT; i++) {
        s_track_lines[i] = lv_line_create(s_screen);
        lv_obj_set_style_line_color(s_track_lines[i], lv_color_hex(0xffffff), 0);
        lv_obj_set_style_line_width(s_track_lines[i], 1, 0);
        lv_obj_set_style_line_opa(s_track_lines[i], LV_OPA_70, 0);
        lv_obj_set_style_line_rounded(s_track_lines[i], true, 0);
        lv_obj_remove_flag(s_track_lines[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_track_lines[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (size_t i = 0; i < RADAR_MAX_AIRCRAFT; i++) {
        s_planes[i].hitbox = lv_obj_create(s_screen);
        lv_obj_set_size(s_planes[i].hitbox, 24, 24);
        lv_obj_set_style_bg_opa(s_planes[i].hitbox, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_planes[i].hitbox, 0, 0);
        lv_obj_set_style_pad_all(s_planes[i].hitbox, 0, 0);
        lv_obj_remove_flag(s_planes[i].hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(s_planes[i].hitbox, plane_clicked, LV_EVENT_PRESSED, (void *)(uintptr_t)i);
        s_planes[i].marker = lv_label_create(s_planes[i].hitbox);
        lv_label_set_text(s_planes[i].marker, "^");
        lv_obj_set_style_text_font(s_planes[i].marker, &lv_font_montserrat_16, 0);
        lv_obj_set_style_transform_pivot_x(s_planes[i].marker, 5, 0);
        lv_obj_set_style_transform_pivot_y(s_planes[i].marker, 8, 0);
        lv_obj_center(s_planes[i].marker);
        s_planes[i].callsign = create_text(s_screen, "", &lv_font_montserrat_12,
                                           lv_color_hex(0xb7f7c8), 0, 0);
        lv_obj_add_flag(s_planes[i].hitbox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_planes[i].callsign, LV_OBJ_FLAG_HIDDEN);
    }

    create_line(s_screen, 8, 207, 224, 1);
    s_selected_name = create_text(s_screen, "Waiting for traffic", &lv_font_montserrat_16,
                                  lv_color_hex(0xf4fff7), 8, 211);
    s_selected_type = create_text(s_screen, "", &lv_font_montserrat_12,
                                  lv_color_hex(0x8abf98), 8, 230);
    s_selected_position = create_text(s_screen, "50 km around Quedlinburg", &lv_font_montserrat_12,
                                      lv_color_hex(0xb7f7c8), 8, 248);
    s_selected_motion = create_text(s_screen, "", &lv_font_montserrat_12,
                                    lv_color_hex(0xb7f7c8), 8, 266);
    s_status_label = create_text(s_screen, "Waiting for Wi-Fi", &lv_font_montserrat_12,
                                 lv_color_hex(0x4c805b), 8, 190);

    s_range_label = create_button(s_screen, 4, 61, "50 KM", range_clicked);
    s_filter_label = create_button(s_screen, 70, 54, "ALL", filter_clicked);
    s_path_label = create_button(s_screen, 129, 40, "PATH", path_clicked);
    lv_obj_set_style_text_color(s_path_label, lv_color_hex(0x8abf98), 0);
    create_button(s_screen, 174, 62, "WEATHER", switch_callback);
    lv_timer_create(animation_timer, 500, NULL);
    return s_screen;
}

void radar_ui_set_status(const char *status)
{
    if (s_status_label) lv_label_set_text(s_status_label, status ? status : "");
}

void radar_ui_update(const radar_snapshot_t *snapshot)
{
    if (!snapshot || !s_screen) return;

    char selected_callsign[sizeof(s_snapshot.aircraft[0].callsign)] = {0};
    radar_aircraft_t selected = {0};
    if (s_snapshot.count > 0 && s_selected < s_snapshot.count) {
        selected = s_snapshot.aircraft[s_selected];
        snprintf(selected_callsign, sizeof(selected_callsign), "%s", s_snapshot.aircraft[s_selected].callsign);
    }

    update_tracks(snapshot);
    s_snapshot = *snapshot;
    s_snapshot_tick = lv_tick_get();
    s_selected = 0;
    if (selected_callsign[0]) {
        for (size_t i = 0; i < s_snapshot.count; i++) {
            if (strcmp(selected_callsign, s_snapshot.aircraft[i].callsign) == 0) {
                s_selected = i;
                snprintf(s_snapshot.aircraft[i].aircraft_name,
                         sizeof(s_snapshot.aircraft[i].aircraft_name), "%s",
                         selected.aircraft_name);
                snprintf(s_snapshot.aircraft[i].manufacturer,
                         sizeof(s_snapshot.aircraft[i].manufacturer), "%s",
                         selected.manufacturer);
                snprintf(s_snapshot.aircraft[i].airline,
                         sizeof(s_snapshot.aircraft[i].airline), "%s", selected.airline);
                snprintf(s_snapshot.aircraft[i].origin,
                         sizeof(s_snapshot.aircraft[i].origin), "%s", selected.origin);
                snprintf(s_snapshot.aircraft[i].destination,
                         sizeof(s_snapshot.aircraft[i].destination), "%s", selected.destination);
                break;
            }
        }
    }
    render_radar();

    time_t now = 0;
    struct tm local = {0};
    char status[32];
    time(&now);
    localtime_r(&now, &local);
    strftime(status, sizeof(status), "Live %H:%M:%S", &local);
    lv_label_set_text(s_status_label, status);
}

bool radar_ui_get_selected(radar_aircraft_t *aircraft)
{
    if (!aircraft || s_snapshot.count == 0 || s_selected >= s_snapshot.count) return false;
    *aircraft = s_snapshot.aircraft[s_selected];
    return true;
}

void radar_ui_set_selected_details(const radar_aircraft_t *aircraft, bool resolved)
{
    if (!aircraft || s_snapshot.count == 0 || s_selected >= s_snapshot.count ||
        strcmp(aircraft->callsign, s_snapshot.aircraft[s_selected].callsign) != 0) {
        return;
    }
    radar_aircraft_t *selected = &s_snapshot.aircraft[s_selected];
    snprintf(selected->aircraft_name, sizeof(selected->aircraft_name), "%s", aircraft->aircraft_name);
    snprintf(selected->manufacturer, sizeof(selected->manufacturer), "%s", aircraft->manufacturer);
    snprintf(selected->airline, sizeof(selected->airline), "%s", aircraft->airline);
    snprintf(selected->origin, sizeof(selected->origin), "%s", aircraft->origin);
    snprintf(selected->destination, sizeof(selected->destination), "%s", aircraft->destination);
    s_details_resolved = resolved;
    render_radar();
}
