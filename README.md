# Clock Weather

ESP-IDF weather clock for the ESP32 Cheap Yellow Display (ESP32-2432S028R).

The app connects to Wi-Fi, syncs time over SNTP, fetches current weather from
OpenWeather, and renders a compact dashboard with LVGL.

## Hardware

- ESP32-2432S028R / Cheap Yellow Display
- ST7789 LCD over SPI

## Setup

1. Install ESP-IDF 5.5.x.
2. Copy `main/wifi_secrets.example.h` to `main/wifi_secrets.h`.
3. Fill in Wi-Fi credentials and the OpenWeather API key.
4. Build and flash:

```powershell
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

## Configuration

Runtime settings live in `main/app_config.h`:

- city/country/language for OpenWeather
- weather refresh interval
- POSIX timezone string

Do not commit `main/wifi_secrets.h`; it contains local credentials.
