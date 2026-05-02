# Clock Weather - Handover

Последнее обновление: 2026-05-02

## Быстрый контекст

Clock Weather - ESP-IDF приложение погодных часов для ESP32 Cheap Yellow Display (`ESP32-2432S028R`). Устройство подключается к Wi-Fi, синхронизирует время через SNTP, получает погоду из OpenWeather и показывает компактный LVGL dashboard на экране 240x320.

Глобальный индекс проектов:

```text
C:\Users\Sergej\Documents\Codex\PROJECTS.md
```

## Пути и репозиторий

Локальный путь текущей рабочей копии:

```text
C:\Users\Sergej\Projects\embedded\clock-weather
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

VS Code/ESP-IDF обычно указывает на:

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
main/sntp_time.c              - SNTP/timezone setup
main/app_config.h             - city, country, language, refresh interval, timezone
main/wifi_secrets.example.h   - безопасный template для секретов
main/wifi_secrets.h           - реальные локальные секреты, ignored by git
dependencies.lock             - ESP-IDF component lock file
sdkconfig.defaults            - portable default sdkconfig values
preview/screen.html           - desktop preview экрана 240x320 для дизайн-итераций
```

## Текущее состояние

```text
Wi-Fi данные и OpenWeather API key заведены локально в main/wifi_secrets.h.
main/wifi_secrets.h ignored by git и не должен попадать в коммиты.
Локальный sdkconfig тоже ignored by git.
Сборка/прошивка выполнялась из VS Code ESP-IDF extension.
Устройство подключалось к Wi-Fi, получало IP, синхронизировало время, валидировало TLS certificate и получало погоду.
SNTP может не успеть за первые 20 секунд, но продолжает синхронизацию фоном; время потом появляется.
```

UI:

```text
compact clock/weather dashboard with LVGL
city, Wi-Fi state, date/time with seconds
drawn LVGL weather icon
current temperature, description, feels-like, humidity, wind, last update time
day/night themes from OpenWeather sunrise/sunset data
time labels: large HH, blinking colon, large MM, smaller SS without second colon
center block: icon-left / temperature-right
metrics row: Feel / Hum / Wind
status footer: Upd HH:MM
dense layout tuned from real CYD photo; seconds intentionally kept
```

## Что изменено 2026-05-02

```text
Created local main/wifi_secrets.h from template and filled Wi-Fi/OpenWeather values.
Kept main/wifi_secrets.h ignored; secrets are not staged.
Fixed LVGL default font risk by adding CONFIG_LV_FONT_MONTSERRAT_14=y to sdkconfig.defaults.
Made weather URL/query truncation explicit in weather_client.c.
Added xTaskCreate result checks in main.c.
Refined UI for real CYD display: denser time block, seconds kept, weather block raised, temperature shifted left/up, footer raised.
Adjusted day theme muted color so cloud icon is less black on the light screen.
Added preview/screen.html as a browser-based 240x320 design preview.
Tried LVGL snapshot-over-UART approach, but removed it because ESP32 heap largest block was too small for a full 240x320 RGB565 snapshot.
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

Desktop preview:

```text
Open preview/screen.html in the browser.
Default preview scenario mirrors the latest real CYD photo: Day / Cloud / 19:50:24 / Scattered clouds.
```

В этом Codex terminal `idf.py` не доступен без ESP-IDF Python environment:

```text
C:\Users\Sergej\.espressif\python_env\idf5.5_py3.13_env
```

Если нужен build из обычного терминала, восстановить/install ESP-IDF Python environment. Иначе продолжать сборку из VS Code ESP-IDF extension, где она уже запускалась.

## Известные ограничения

```text
Full on-device LVGL screenshot не работает на ESP32 без PSRAM: для 240x320 RGB565 нужно около 153600 bytes contiguous heap, а largest heap был около 73728.
Для дизайн-ревью использовать preview/screen.html и реальные фото CYD.
HTML preview не является точным LVGL renderer: реальный Montserrat на CYD толще и шире, поэтому финальные отступы проверять фото/железом.
```

## Открытые задачи

```text
1. Собрать и прошить последнюю версию из VS Code.
2. Сфотографировать CYD после новых layout правок.
3. По фото проверить: time block, seconds spacing, cloud color, right edge of temperature, footer position.
4. Если display colors are inverted, test esp_lcd_panel_invert_color(panel, true) in main/display.c.
5. If image is shifted, tune esp_lcd_panel_set_gap(panel, x, y) in main/display.c.
6. Optionally add sunrise/sunset display or richer weather icons after basic UI settles.
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
Use preview/screen.html for quick UI iterations, then validate on real CYD photo.
```
