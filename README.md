# Clock Weather

Часы с погодой для ESP32 Cheap Yellow Display (`ESP32-2432S028R`).

Проект подключается к Wi-Fi, синхронизирует время через SNTP, получает текущую
погоду из OpenWeather и показывает компактный экран на LVGL: дата, время,
температура, описание погоды, ощущается как, влажность и ветер.

## Железо

- ESP32-2432S028R / Cheap Yellow Display
- LCD ST7789 по SPI

## Стек

- ESP-IDF 5.5.x
- LVGL 9
- `espressif/esp_lvgl_port`
- OpenWeather API

## Настройка

1. Установить ESP-IDF 5.5.x.
2. Скопировать `main/wifi_secrets.example.h` в `main/wifi_secrets.h`.
3. Заполнить Wi-Fi и OpenWeather API key в `main/wifi_secrets.h`.
4. Собрать и прошить:

```powershell
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

## Конфигурация

Основные настройки лежат в `main/app_config.h`:

- город, страна и язык для OpenWeather;
- интервал обновления погоды;
- POSIX timezone для локального времени.

## Секреты

`main/wifi_secrets.h` содержит локальные Wi-Fi данные и OpenWeather API key.
Этот файл исключён из git и не должен попадать в GitHub.

Для GitHub есть только безопасный шаблон:

```text
main/wifi_secrets.example.h
```

## Текущее состояние

Проект подготовлен как переносимый ESP-IDF репозиторий. В GitHub отправлены
исходники, README, devcontainer, lock-файл зависимостей и шаблон секретов.

Локальную сборку нужно прогнать после восстановления ESP-IDF Python окружения:

```text
C:\Users\Sergej\.espressif\python_env\idf5.5_py3.13_env
```
