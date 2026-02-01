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

## Power safety
- Fuse the main PSU output (120-150% of expected load) and consider per-zone fuses or PTC resettable fuses.
- All project circuits are low-voltage DC. Keep mains AC confined to the PSU; never expose AC wiring alongside DC control electronics.
- When tying multiple PSU negatives together (common ground), verify isolated outputs and bond them externally.

## Brownout protection
- Power the ESP32 from a separate regulator, not the same 5V rail as LED strips.
- When powering from a 12V/24V LED supply, use a buck converter (>= 1A, 5V for dev boards or 3.3V for bare modules) with common ground.
- Add 1000 uF bulk cap on the strip power input and on the buck converter input.
- Use software soft-start (ramp brightness over 50-200 ms).

## 24V non-addressable strips (COB / SMD)
- No data line; PWM-only control via MOSFETs (use `LEDPWM` library, not `LEDStrip`).
- Cut only at marked cut points. Power injection needed less frequently at 24V than 5V.

## Multi-zone systems
- Home-run wiring: all runs back to central ESP32 + PSU location. Centralized MOSFET placement is simpler; distributed only for long runs (>10m) or high current.
- One MOSFET per zone per color channel. Common ground across all PSUs.
- Mixing 12V and 24V zones is fine; MOSFETs are voltage-agnostic on the drain side.
- Future-proof cable runs: pull extra conductors (4+ wires), label both ends, use conduit where possible.
- Line-voltage fixtures (pot lights): use smart switches, 0-10V dimmers (check sink vs source type), or triac dimmers (requires isolated zero-cross detector + optotriac stage). ESP32 cannot switch/dim mains AC with a GPIO alone.

## MOSFET gate resistor
- 100 ohm (default, per IRLB8721 app notes) or 220 ohm (more conservative, common in hobby circuits). Both valid for LED PWM at 20 kHz.

## 433 MHz RF remote control
- RXB6 superheterodyne receiver + rc-switch library (`sui77/rc-switch`).
- EV1527 OTP fixed codes — learnable by receiver, not rolling/encrypted. Acceptable for lighting, not for security.
- Quarter-wave antenna: 17.3 cm straight wire on the ANT pad.

## Audio input
- I2S mics: INMP441, SPH0645, ICS-43432. (ICS-43434 is EOL; ICS-43432 is the current replacement.)
- Analog mic (MAX4466/MAX9814): simpler wiring but lower quality than I2S; adequate for basic beat detection only.
- Line-in: RCA via ADC1 (GPIO 32-39) with bias circuit, or PCM1808 external I2S ADC for higher fidelity (99 dB dynamic range, dual supply: 5V analog + 3.3V digital). Do not connect speaker-level outputs to PCM1808.
- Beat detection uses energy ratio in bass band vs sliding window. BPM tracking and beat quantization are documented in `docs/audio-reactive.md`.

## Persistent storage (ESP32)
- Use `Preferences.h` (NVS) for small settings; LittleFS for files. SPIFFS is deprecated.
- Throttle frequent NVS writes (dirty flag + periodic flush).

## Build environment
- PlatformIO is installed in `.venv/` via uv. Run `source .venv/bin/activate` before `pio` commands, or use `.venv/bin/pio` directly.
- If `.venv/` is missing: `uv venv .venv && uv pip install platformio --python .venv/bin/python`

## Docs hygiene
- When changing hardware guidance, update both `CLAUDE.md` and `AGENTS.md`.
