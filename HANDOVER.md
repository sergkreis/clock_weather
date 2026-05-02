# Clock Weather - Handover

Последнее обновление: 2026-05-01

## Быстрый контекст

Clock Weather - ESP-IDF приложение погодных часов для ESP32 Cheap Yellow Display (`ESP32-2432S028R`). Устройство подключается к Wi-Fi, синхронизирует время через SNTP, получает погоду из OpenWeather и показывает compact LVGL dashboard.

Глобальный индекс проектов:

```text
C:\Users\Sergej\Documents\Codex\PROJECTS.md
```

## Пути и репозиторий

Локальный путь:

```text
C:\Users\Sergej\clock_weather
```

GitHub:

```text
https://github.com/sergkreis/clock_weather.git
```

Ветка:

```text
main
```

## Технологии

```text
ESP-IDF 5.5.x
LVGL 9
espressif/esp_lvgl_port
OpenWeather API
Target: esp32
Hardware: ESP32-2432S028R / Cheap Yellow Display
```

VS Code settings ранее указывали на:

```text
C:\esp\v5.5.3\esp-idf
```

## Основные файлы

```text
README.md                     - русская инструкция и заметки проекта
main/main.c                   - app entrypoint, LVGL UI, clock/weather tasks
main/display.c                - ST7789/LVGL display initialization
main/weather_client.c         - OpenWeather HTTP client and JSON parser
main/wifi_manager.c           - Wi-Fi station setup and reconnect handling
main/app_config.h             - city, country, language, refresh interval, timezone
main/wifi_secrets.example.h   - безопасный template для секретов
main/wifi_secrets.h           - реальные локальные секреты, ignored by git
dependencies.lock             - ESP-IDF component lock file
sdkconfig.defaults            - portable default sdkconfig values
```

## Текущее состояние

```text
App source committed and pushed to GitHub.
README должен оставаться на русском.
Реальные Wi-Fi/OpenWeather секреты лежат в main/wifi_secrets.h и не должны попадать в git.
main/wifi_secrets.example.h - безопасный template в GitHub.
```

UI:

```text
compact clock/weather dashboard with LVGL
city, Wi-Fi state, date/time with small seconds
drawn LVGL weather icon
current temperature, description, feels-like, humidity, wind, last update time
day/night themes from OpenWeather sunrise/sunset data
time labels: large HH, blinking colon, large MM, smaller SS without second colon
center block: icon-left / temperature-right
dense layout for 240x320 screen
```

## Проверка и команды

Обычный workflow:

```text
edit -> build/test locally -> commit -> push to GitHub
```

Build/flash:

```powershell
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

На прошлом handover `idf.py build` не запускался, потому что отсутствовало ESP-IDF Python virtual environment:

```text
C:\Users\Sergej\.espressif\python_env\idf5.5_py3.13_env
```

Перед доверием build results восстановить/install ESP-IDF Python environment и запустить:

```powershell
idf.py build
```

## Исправления, уже сделанные ранее

```text
Real Wi-Fi/OpenWeather secrets excluded from git.
Added safe wifi_secrets.example.h.
Added Russian README.
Cleaned broken mojibake comments/strings from source files.
Improved UI layout and status text.
Added day/night theme.
Added drawn LVGL weather icons.
Added city/Wi-Fi header.
Added Updated HH:MM status.
Added degree Celsius labels and wind in km/h.
Improved URL encoding for OpenWeather query parameters.
Ignored local .vscode/settings.json, build/, managed_components/, sdkconfig, and main/wifi_secrets.h.
```

## Открытые задачи

```text
1. Restore ESP-IDF Python environment and run idf.py build.
2. Flash to the CYD board and verify screen orientation, colors, and touchless UI fit.
3. If display colors are inverted, test esp_lcd_panel_invert_color(panel, true) in main/display.c.
4. If image is shifted, tune esp_lcd_panel_set_gap(panel, x, y) in main/display.c.
5. Optionally add weather icons or sunrise/sunset display after basic hardware verification.
```

## Запрещено

```text
Не коммитить main/wifi_secrets.h.
Не публиковать Wi-Fi credentials или OpenWeather API key.
Не коммитить build/, managed_components/, sdkconfig, локальные .vscode/settings.json.
Не переводить README.md на английский; README проекта должен быть на русском.
```

## Как продолжать в новом чате

```text
Open C:\Users\Sergej\Documents\Codex\PROJECTS.md and continue Clock Weather.
Then open this HANDOVER.md.
Before code changes, check git status and verify ESP-IDF environment.
```
