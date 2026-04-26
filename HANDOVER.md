# Clock Weather Handover

## Project

ESP-IDF weather clock for ESP32 Cheap Yellow Display (`ESP32-2432S028R`).

Local path:

```text
C:\Users\Sergej\clock_weather
```

GitHub:

```text
https://github.com/sergkreis/clock_weather.git
```

Current branch:

```text
main
```

## Current State

- App source is committed and pushed to GitHub.
- UI renders a compact clock/weather dashboard with LVGL.
- UI includes city, Wi-Fi state, date/time with small seconds, drawn LVGL weather icon, current
  temperature, description, feels-like, humidity, wind, and last update time.
- Center block uses an icon-left / temperature-right layout with no heavy
  separator line through the weather area. The layout is intentionally denser
  to better use the 240x320 screen.
- Time is built from separate labels: large `HH`, blinking colon, large `MM`,
  and smaller `SS` without a second colon.
- UI switches between day and night themes using OpenWeather sunrise/sunset data.
- Weather is fetched from OpenWeather by city/country.
- Time sync uses SNTP and German POSIX timezone.
- Real local secrets are kept in `main/wifi_secrets.h` and ignored by git.
- `main/wifi_secrets.example.h` is the safe template committed to GitHub.
- README is in Russian by project convention.

## Stack

```text
ESP-IDF 5.5.x
LVGL 9
espressif/esp_lvgl_port
OpenWeather API
Target: esp32
```

## Important Files

```text
README.md                     - Russian setup and project notes
main/main.c                   - app entrypoint, LVGL UI, clock/weather tasks
main/display.c                - ST7789/LVGL display initialization
main/weather_client.c         - OpenWeather HTTP client and JSON parser
main/wifi_manager.c           - Wi-Fi station setup and reconnect handling
main/app_config.h             - city, country, language, refresh interval, timezone
main/wifi_secrets.example.h   - safe secrets template
main/wifi_secrets.h           - real local secrets, ignored by git
dependencies.lock             - ESP-IDF component lock file
sdkconfig.defaults            - portable default sdkconfig values
```

## Git / Deployment Workflow

Normal workflow:

```text
edit -> build/test locally -> commit -> push to GitHub
```

Build/flash workflow:

```powershell
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

## Local Environment Notes

VS Code settings previously pointed to:

```text
C:\esp\v5.5.3\esp-idf
```

At the last handover, `idf.py build` could not run because the ESP-IDF Python
virtual environment was missing:

```text
C:\Users\Sergej\.espressif\python_env\idf5.5_py3.13_env
```

Before relying on build results, restore/install the ESP-IDF Python environment
and run `idf.py build`.

## Review Notes

Issues fixed during initial GitHub cleanup:

- Real Wi-Fi/OpenWeather secrets were excluded from git.
- Added safe `wifi_secrets.example.h`.
- Added Russian README.
- Cleaned broken mojibake comments/strings from source files.
- Improved UI layout and status text.
- Added day/night theme, drawn LVGL weather icons, city/Wi-Fi header,
  `Updated HH:MM` status, degree Celsius labels, and wind in km/h.
- Improved URL encoding for OpenWeather query parameters.
- Ignored local `.vscode/settings.json`, `build/`, `managed_components/`,
  `sdkconfig`, and `main/wifi_secrets.h`.

## Next Useful Steps

- Restore ESP-IDF Python environment and run `idf.py build`.
- Flash to the CYD board and verify screen orientation, colors, and touchless UI fit.
- If display colors are inverted, test `esp_lcd_panel_invert_color(panel, true)` in `main/display.c`.
- If image is shifted, tune `esp_lcd_panel_set_gap(panel, x, y)` in `main/display.c`.
- Optionally add weather icons or sunrise/sunset display after basic hardware verification.
