# Clock Weather Radar - Handover

Последнее обновление: 2026-07-12

## Контекст

Рабочее ESP-IDF-приложение для ESP32-2432S028R: погодные часы и сенсорный
ADS-B радар 240x320. Радар является стартовым экраном, погода открывается
кнопкой. Проект проверен сборкой и многократной прошивкой на реальном CYD.

## Пути

```text
Проект: C:\Users\Sergej\Projects\embedded\clock-weather
GitHub: https://github.com/sergkreis/clock_weather.git
Ветка: main
Индекс: C:\Users\Sergej\Documents\Codex\PROJECTS.md
ESP-IDF: C:\esp\v5.5.3\esp-idf
```

## Основные файлы

```text
main/main.c          - запуск, weather/radar/detail tasks, LVGL weather UI
main/display.c       - ST7789, XPT2046, калибровка сенсора, PWM-подсветка
main/radar_client.c  - ADSB.lol позиции, ADSBDB/adsb.im модель и маршруты
main/radar_ui.c      - радар, выбор, масштаб, фильтры и PATH-траектории
main/weather_client.c - OpenWeather client
main/app_config.h    - координаты, радиус, интервалы, timezone
sdkconfig.defaults   - переносимые настройки ESP-IDF
preview/screen.html  - браузерный макет погодного экрана
```

## Текущее поведение

- радар: 50 км вокруг Quedlinburg, обновление примерно каждые 10 секунд;
- выбранный самолёт: модель и маршрут загружаются отдельной задачей;
- `PATH`: до 24 точек на борт с шагом около 30 секунд, до 12 минут;
- общий mutex сериализует TLS-запросы погоды, радара и подробностей;
- последний успешный радар сохраняется при сетевой ошибке;
- сенсор откалиброван под конкретный CYD;
- загрузочный экран появляется до подключения Wi-Fi;
- ночная тема снижает яркость подсветки.

## Проверенная стабильность

```text
Исправлено падение LVGL на float-форматировании: использовать snprintf.
История PATH уменьшена для сохранения RAM под TLS.
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10.
CONFIG_MBEDTLS_ECP_RESTARTABLE выключен: в ESP-IDF 5.5.3 вызывал TLS LoadProhibited.
Сборка успешна, приложение занимает около 1.54 MB, свободно 27% app partition.
```

Внешние DNS/API иногда отвечают с ошибкой. Это допустимо, если следующий цикл
восстанавливается. Маршрут может отсутствовать у частных и служебных бортов.

## Сборка

```powershell
idf.py build
idf.py flash monitor
```

В VS Code используются ESP-IDF Build, Flash и Monitor. Последний обнаруженный
порт в локальных настройках был COM13, но после переподключения его нужно проверять.

## Безопасность

```text
Не коммитить main/wifi_secrets.h, sdkconfig, build/, managed_components/ и .vscode/settings.json.
README должен оставаться на русском.
Не отключать проверку TLS-сертификатов.
```

## Следующие шаги

1. Наблюдать устройство несколько часов с включённым `PATH`.
2. При необходимости уменьшить шум логов временных DNS-ошибок.
3. Возможное развитие: собственный RTL-SDR/readsb сервер и веб-карта.
