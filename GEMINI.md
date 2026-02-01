# GEMINI.md -- RGBW Lighting Project Guide

## Project Overview

PlatformIO monorepo for RGBW LED lighting controllers targeting ESP32 and Arduino (AVR) platforms. The system supports addressable LED strips (SK6812 RGBW and WS2815B RGB), PWM-driven MOSFET channels, I2S microphones for sound-reactive lighting, RCA audio input, WiFi/BLE wireless control, and physical switches.

## Repository Structure

```
rgbw-lighting/
  apps/                         # Standalone PlatformIO projects
    example-rgbw/               # Template app (copy to create new apps)
      platformio.ini
      src/main.cpp
  shared/lib/                   # Shared libraries (linked via lib_extra_dirs)
    RGBWCommon/                 # Core RGBW/HSV types, color math, utilities
    LEDStrip/                   # Addressable strip driver (SK6812 RGBW, WS2815B RGB)
    LEDPWM/                     # PWM channel driver (MOSFET-controlled RGBW)
    Connectivity/               # WiFi and BLE managers (ESP32 only)
  .gitignore
  README.md
```

### Creating a New App

1. Copy `apps/example-rgbw/` to `apps/your-app-name/`
2. Edit `platformio.ini` for your board and dependencies
3. Write your code in `src/main.cpp`

### Library Dependency Graph

```
RGBWCommon          (no dependencies -- foundation library)
  <- LEDStrip       (depends on RGBWCommon)
  <- LEDPWM         (depends on RGBWCommon)
Connectivity        (ESP32 only, no internal lib dependencies)
```

## Build System

**Framework:** Arduino (via PlatformIO)
**Shared library path:** `lib_extra_dirs = ../../shared/lib` in each app's `platformio.ini`

### Prerequisites

PlatformIO CLI is installed in a project-local virtual environment managed by [uv](https://docs.astral.sh/uv/):

```bash
# First-time setup (requires uv)
uv venv .venv
uv pip install platformio --python .venv/bin/python

# Activate the venv (makes `pio` available)
source .venv/bin/activate
```

Or invoke without activating: `.venv/bin/pio run`

### Target Platforms

| Environment    | Platform      | Board      | C++ Standard |
|---------------|---------------|------------|-------------|
| `esp32`       | espressif32   | esp32dev   | gnu++17     |
| `arduino-uno` | atmelavr      | uno        | gnu++11     |

### Build Commands

```bash
cd apps/<app-name>
pio run                          # Build all environments
pio run -e esp32                 # Build ESP32 only
pio run -t upload -e esp32       # Upload to ESP32
pio device monitor               # Serial monitor (115200 baud)
pio test -e native               # Run unit tests on host
```

### Recommended Build Flags

```ini
[env]
framework = arduino
lib_extra_dirs = ../../shared/lib
monitor_speed = 115200
build_flags =
    -Wall
    -Wextra
    -Werror=return-type

[env:esp32]
platform = espressif32
board = esp32dev
build_flags =
    ${env.build_flags}
    -std=gnu++17
    -DCORE_DEBUG_LEVEL=3          ; 0=none, 3=info, 5=verbose
build_unflags = -std=gnu++11

[env:esp32-release]
platform = espressif32
board = esp32dev
build_type = release
build_flags =
    ${env.build_flags}
    -std=gnu++17
    -DCORE_DEBUG_LEVEL=0
    -DNDEBUG
    -Os
build_unflags = -std=gnu++11

[env:arduino-uno]
platform = atmelavr
board = uno
lib_deps =
    makuna/NeoPixelBus@^2.8.0
```

## Language and Coding Standards

### C++ Standard

- **ESP32:** C++17 (`-std=gnu++17`). Use `constexpr`, `std::optional`, structured bindings, `if constexpr` freely.
- **Arduino AVR:** C++11 (compiler default). No C++14/17 features.
- **Shared libraries** (in `shared/lib/`): Must compile as **C++11** for cross-platform compatibility. Guard C++17 features with `#if __cplusplus >= 201703L`.

### Naming Conventions (established by existing code)

| Element             | Convention          | Example                    |
|--------------------|--------------------|-----------------------------|
| Files              | `snake_case.h/cpp` | `led_strip.h`               |
| Classes            | `PascalCase`       | `LEDStrip`, `BLEManager`    |
| Functions/methods  | `camelCase`        | `setPixel()`, `hsvToRGBW()` |
| Private members    | `_camelCase`       | `_brightness`, `_server`     |
| Constants/defines  | `UPPER_SNAKE_CASE` | `LED_PIN`, `NUM_LEDS`       |
| Namespaces         | `snake_case`       | `namespace pins {}`          |
| Enums              | `PascalCase` type, `PascalCase` values | `LedError::InvalidIndex` |

Prefer `constexpr` over `#define` for typed constants where compiler supports it:
```cpp
constexpr uint8_t kLedPin = 5;       // ESP32-specific code
constexpr uint16_t kNumLeds = 30;
```
Use `#define` for values needed by both ESP32 and AVR or for conditional compilation.

### Header Files

- Use `#pragma once` (already established).
- Platform-specific code: wrap entire file body in `#ifdef ESP32` / `#endif`.
- Include order: Arduino/system headers, then library headers, then local headers.

### Strings

- **Never use Arduino `String` class** in production code. It causes heap fragmentation on constrained devices.
- Use `char[]` buffers with `snprintf()` for string formatting.
- For log messages, use ESP_LOG macros (ESP32) or a cross-platform wrapper.

### No Exceptions

C++ exceptions are disabled on ESP32-IDF (`-fno-exceptions`) and unavailable on AVR. Use return codes:

```cpp
enum class LedError : uint8_t {
    Ok = 0,
    InvalidIndex,
    BufferFull,
    HardwareFailure,
    Timeout,
};
```

## Memory Management

### Stack vs Heap

- Pre-allocate all buffers in `setup()` or at construction time. Never allocate in tight loops.
- FreeRTOS task stack size: 4096 bytes minimum for LED tasks, 8192+ for WiFi/BLE tasks.
- ESP32 has ~320 KB SRAM. Arduino Uno has 2 KB. Size buffers accordingly.
- Use `static` or global arrays for fixed-size buffers. Avoid `new`/`malloc` after initialization.

### Buffer Patterns

```cpp
// Good: static buffer, known at compile time
static RGBW pixelBuffer[MAX_LEDS];

// Good: allocated once in constructor
_pixels = new RGBW[numLeds];  // only in constructor/setup

// Bad: allocation in loop
void update() {
    auto* temp = new RGBW[count];  // NEVER do this
}
```

## Platform Abstraction

Use `#ifdef ESP32` for platform-specific code. Shared libraries already follow this pattern:

```cpp
void LEDPWM::_applyColor() {
    RGBW scaled = scaleBrightness(_currentColor, _brightness);
#ifdef ESP32
    // Scale 8-bit channel values to match configured resolution
    uint32_t maxDuty = (1 << _resolution) - 1;
    ledcWrite(_pins.r, (uint32_t)scaled.r * maxDuty / 255);  // LEDC peripheral
#else
    analogWrite(_pins.r, scaled.r); // AVR analogWrite (8-bit)
#endif
}
```

## RGBW LED Control

### Color Model

- **RGBW struct:** 4 bytes (r, g, b, w), each 0-255.
- **HSV struct:** hue (0-360), saturation (0-255), value (0-255).
- `hsvToRGBW()` extracts the white component from the minimum of RGB channels. Use this for smooth color transitions with efficient white LED usage.
- `lerpRGBW()` for interpolation between colors. `scaleBrightness()` for dimming.

### Addressable Strips (SK6812 RGBW and WS2815B RGB)

The `LEDStrip` library uses **NeoPixelBus** (by Makuna) for addressable LED output. Strip type is a compile-time template parameter:

```cpp
#include "led_strip.h"

// SK6812 RGBW
LEDStrip<StripType::SK6812_RGBW> rgbwStrip(5, 30);

// WS2815B RGB
LEDStrip<StripType::WS2815B_RGB> rgbStrip(5, 60);
```

#### Protocol Comparison

| Feature                | SK6812 RGBW               | WS2815B RGB               |
|------------------------|---------------------------|---------------------------|
| **Channels**           | 4 (R, G, B, W)           | 3 (R, G, B)              |
| **Bits per pixel**     | 32                        | 24                        |
| **Byte order**         | GRB+W                     | GRB                       |
| **NeoPixelBus Feature/Method** | `NeoGrbwFeature` / `Sk6812Method` | `NeoGrbFeature` / `Ws2812xMethod` |
| **Operating voltage**  | 5V                        | 12V                       |
| **Current (full white)** | ~80 mA @ 5V (20 mA/ch) | ~18 mA @ 12V (~6 mA/ch)  |
| **Power per LED**      | ~400 mW                   | ~216 mW                   |
| **Backup data line**   | No                        | Yes (BIN)                 |
| **White channel**      | Dedicated white LED die   | RGB-mixed (no white die)  |

#### White Channel Handling

- **SK6812 RGBW:** Has a dedicated white LED die. `hsvToRGBW()` extracts the white component from the minimum of RGB for efficient usage.
- **WS2815B RGB:** No white die. The `LEDStrip::show()` method automatically folds `RGBW.w` back into the RGB channels with clamping (`r = min(r + w, 255)`).

#### Level Shifting

Both strip types use 5V-logic data lines.

**ESP32 level shifting options (ranked):**
1. **74HCT125 quad buffer** (best): Accepts 3.3V input, outputs 5V. Add 33-ohm series resistor at output.
2. **Sacrificial LED at 3.3V:** Wire one LED powered at 3.3V as the first pixel.

**WS2815B power domain note:** The 12V supply powers the LEDs only. The data line is 5V logic. Never connect 12V to the level shifter.

### PWM Channels (MOSFET-driven)

- Use the `LEDPWM` library for discrete R, G, B, W channels via MOSFETs.
- **MOSFET selection:** Use logic-level N-channel MOSFETs (e.g., IRLZ44N, IRLB8721).
- **Flyback protection:** Add a diode across inductive loads.

**ESP32 LEDC Resolution/Frequency:**
Recommended: **12-bit at ~19.5 kHz**.

### Gamma Correction

Human vision perceives brightness logarithmically. Apply gamma correction for perceptually uniform dimming. See `docs/led-control.md` for the gamma 2.2 lookup table used on PWM channels. NeoPixelBus provides `NeoGamma<NeoGammaTableMethod>` built-in (currently using `NeoGammaNullMethod` for no gamma; swap when ready).

### Power Management

- **SK6812 RGBW (5V):** 30-LED strip = 2.4 A max.
- **WS2815B RGB (12V):** 30-LED strip = 0.54 A max.
- Always calculate worst-case current draw and size power supplies with 20% headroom.

## Audio Input and Sound-Reactive Lighting

### I2S Microphones (INMP441, SPH0645)

- Connect via I2S interface.
- Sample at 44100 Hz for full audio spectrum.
- I2S provides 24-bit samples in DMA buffers without CPU blocking.

### RCA Audio Input (Line-Level)

- **Use ADC1 pins only** (GPIO 32-39).
- Bias signal to Vcc/2 (1.65V).
- Use **I2S ADC Mode** for continuous DMA sampling (avoids CPU blocking of `analogRead`).

### FFT Processing

- **Use ESP-DSP:** Espressif's official DSP library uses Xtensa SIMD instructions.
- Performance: <2ms per FFT vs 10-15ms for generic C implementations.
- Map frequency bands to RGBW channels.

## Standard Lighting Protocols

To allow control from PC software (xLights, Vixen, Jinx!), we implement standard E1.31 and DDP receivers.

### E1.31 (sACN)
- Standard DMX-over-Ethernet.
- Good for moderate pixel counts (< 4 Universes / ~680 pixels).
- Use Multicast for simple setup, Unicast for lower network load.

### DDP (Distributed Display Protocol)
- Lightweight protocol, more efficient than E1.31.
- Preferred for high pixel counts on ESP32 over WiFi.
- "WLED-compatible" software control.

## WiFi and BLE

### WiFi
- **WebServer:** Use **ESPAsyncWebServer**. Synchronous servers block LED animations.
- Use event-driven connection management.

### BLE
- **Stack:** Use **NimBLE-Arduino**.
    - ~50% less RAM/Flash usage than default Bluedroid.
    - Critical for fitting WiFi + LED + Audio + BLE on one chip.
- **Service:**
  - Color characteristic (0xFF01): WRITE, 4 bytes (R, G, B, W)
  - Brightness characteristic (0xFF02): READ/WRITE, 1 byte (0-255)

### WiFi + BLE Coexistence
- Both share the ESP32 radio.
- **Critical:** On ESP32, the RMT peripheral handles LED timing in hardware without disabling interrupts. On AVR, `show()` disables interrupts during transmission, which can disrupt wireless connectivity.

## FreeRTOS (ESP32)

### Task Guidelines

| Task                  | Priority | Core | Stack Size |
|----------------------|----------|------|------------|
| LED animation/update | 3        | 1    | 4096       |
| Audio FFT processing | 4        | 1    | 8192       |
| WiFi/BLE handling    | 1        | 0    | 4096       |
| Web server           | 2        | 0    | 8192       |
| Sensor polling       | 2        | 1    | 4096       |

## Pin Safety Rules

- **GPIO 6-11:** Connected to SPI flash. Never use.
- **GPIO 36, 39:** Input-only. Use for ADC/analog input.
- **ADC2 pins:** Unavailable when WiFi is active. Use ADC1 (GPIO 32-39).
- **GPIO 0, 2, 15:** Affect boot mode.

## Library Stack Recommendations

| Component | Recommended Library | Why? |
|-----------|---------------------|------|
| **LED Driver** | **NeoPixelBus** | Non-blocking DMA/RMT, native RGBW types, template-based strip selection. |
| **BLE** | **NimBLE-Arduino** | Saves ~100KB RAM, 50% Flash vs default stack. |
| **Web Server** | **ESPAsyncWebServer** | Non-blocking, smooth animations while serving pages. |
| **FFT** | **ESP-DSP** | Hardware accelerated (SIMD), 5x-10x faster. |
| **Protocol** | **AsyncUDP** | Required for high-performance E1.31/DDP. |

## Git Conventions

- Sensitive files (`secrets.h`, `credentials.h`) are gitignored.
- PlatformIO build artifacts (`.pio/`, `build/`) are gitignored.