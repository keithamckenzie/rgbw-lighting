# AGENTS.md -- RGBW Lighting Agent Guide

## Scope
Quick rules for agents working in this repo. Keep these aligned with CLAUDE.md.

## Hardware best practices (addressable LEDs)
- Add a 500-1000 uF capacitor across + and - at the strip input.
- Add a 300-500 ohm resistor in series with the data line, close to the first pixel.
- Connect ground first, then +V, then data; disconnect in reverse.
- Keep the MCU-to-first-pixel data run short.
- Inject power at multiple points for long runs; aim for <=1 meter from any pixel to a power feed.
- NeoPixel guidance expects a data-high around 70% of pixel Vdd; 3.3V data is marginal with 5V pixels, so use a proper 5V level shifter.

## WS2815B
- Preserve the BIN backup data line when doing custom wiring (DIN/BIN two-pixels-upstream scheme).
- If DIN fails, the IC switches to BIN so downstream LEDs keep working (breakpoint-continuous transmission).

## ADC accuracy (ESP32)
- Use ADC calibration (for example `adc_cali_raw_to_voltage`) when amplitude accuracy matters.
- Add a small bypass capacitor (for example 100 nF) near the ADC input if noise is an issue.

## BLE footprint
- Prefer NimBLE-Arduino when flash/RAM are tight; reported testing shows ~50% less flash and ~100 KB less RAM vs Bluedroid.

## Pin safety (ESP32)
- **Never use GPIO 6-11** (SPI flash).
- **GPIO 12 is dangerous for MOSFET gates** — if pulled HIGH at boot, the ESP32 sets flash to 1.8V and enters a boot loop. Use GPIOs 4, 13, 14, 16-19, 21-23, 25-27, 32-33 instead.
- ADC2 pins are unavailable when WiFi is active. Use ADC1 (GPIO 32-39) for audio.

## Brownout protection
- Power the ESP32 from a separate regulator, not the same 5V rail as LED strips.
- Add 1000 uF bulk cap on the strip power input.
- Use software soft-start (ramp brightness over 50-200 ms).

## Persistent storage (ESP32)
- Use `Preferences.h` (NVS) for small settings; LittleFS for files. SPIFFS is deprecated.
- Throttle frequent NVS writes (dirty flag + periodic flush).

## Build environment
- PlatformIO is installed in `.venv/` via uv. Run `source .venv/bin/activate` before `pio` commands, or use `.venv/bin/pio` directly.
- If `.venv/` is missing: `uv venv .venv && uv pip install platformio --python .venv/bin/python`

## Docs hygiene
- When changing hardware guidance, update both `CLAUDE.md` and `AGENTS.md`.
