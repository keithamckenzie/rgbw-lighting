# rgbw-lighting

RGBW lighting apps for ESP32 and Arduino using PlatformIO.

## Repo structure

```
apps/                   # Each app is a standalone PlatformIO project
  example-rgbw/         # Template app — copy this to start a new project
  led-panel/            # 24x36 RGBW panel (ESP32 + ESP8266)
shared/lib/             # Shared libraries used across apps
  RGBWCommon/           # Color types (RGBW, HSV), conversions, utilities
  LEDStrip/             # Addressable strip driver (SK6812 RGBW, WS2815B RGB) via NeoPixelBus
  LEDPWM/              # PWM-based RGBW channel driver (MOSFET/discrete LEDs)
  Connectivity/         # WiFi and BLE managers (ESP32 only)
```

## Creating a new app

1. Copy `apps/example-rgbw/` to `apps/your-app-name/`
2. Edit `platformio.ini` for your board and dependencies
3. Write your code in `src/main.cpp`
4. Build: `cd apps/your-app-name && pio run`

Shared libraries are automatically available via `lib_extra_dirs` in each app's `platformio.ini`.

## Supported hardware

| Board | LED Type | Connectivity |
|-------|----------|-------------|
| ESP32 | SK6812 RGBW / WS2815B RGB strips, PWM channels | WiFi, BLE |
| ESP8266 (NodeMCU) | SK6812 RGBW / WS2815B RGB strips | WiFi |
| Arduino Uno/Mega | SK6812 RGBW / WS2815B RGB strips, PWM channels | — |

## Building

Requires [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html) or the VS Code extension.

```sh
cd apps/example-rgbw
pio run                    # build for all envs
pio run -e esp32           # build for ESP32 only
pio run -t upload -e esp32 # upload to ESP32
pio device monitor         # serial monitor
```
