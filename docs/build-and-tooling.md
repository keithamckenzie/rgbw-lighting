# Build System and Tooling

PlatformIO build configuration, target platforms, build flags, testing setup, and development workflow.

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

| Environment    | Platform      | Board       | C++ Standard | MCU |
|---------------|---------------|-------------|-------------|-----|
| `esp32`       | espressif32   | esp32dev    | gnu++17     | ESP32 (Xtensa LX6, dual-core, 240 MHz) |
| `esp32-release` | espressif32 | esp32dev    | gnu++17     | Same, optimized for size (-Os) |
| `esp8266`     | espressif8266 | nodemcuv2   | gnu++11     | ESP8266 (Tensilica L106, single-core, 80 MHz) |
| `arduino-uno` | atmelavr      | uno         | gnu++11     | ATmega328P (8-bit, 16 MHz) |
| `native`      | native        | —           | c++17       | Host machine (for unit tests) |

### Recommended platformio.ini

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

[env:native]
platform = native
build_flags = -std=c++17 -DUNIT_TEST -DNATIVE_BUILD
test_framework = unity
lib_extra_dirs = ../../shared/lib
lib_ignore = LEDStrip, LEDPWM, Connectivity  ; hardware-dependent
```

### Build Flags Explained

| Flag | Purpose |
|------|---------|
| `-Wall` | Enable all standard warnings |
| `-Wextra` | Enable extra warnings (unused params, etc.) |
| `-Werror=return-type` | Error on missing return statements (catches serious bugs) |
| `-std=gnu++17` | C++17 with GNU extensions (project requirement; overrides platform default of gnu++11) |
| `build_unflags = -std=gnu++11` | Remove ESP32 platform's default C++11 flag |
| `-DCORE_DEBUG_LEVEL=3` | Set ESP_LOG verbosity — Arduino-ESP32 mapping: 0=none, 1=error, 2=warn, 3=info, 4=debug, 5=verbose |
| `-DNDEBUG` | Disable `assert()` in release builds |
| `-Os` | Optimize for size (release builds) |
| `-DUNIT_TEST` | Defined during host-side testing |
| `-DNATIVE_BUILD` | Defined when building for host (not embedded) |

### Additional Useful Build Flags

```ini
# Partition table for OTA + LittleFS
board_build.partitions = partitions.csv

# Filesystem type
board_build.filesystem = littlefs

# Flash mode (QIO is fastest, DIO is most compatible)
board_build.flash_mode = dio

# Flash size (default 4MB, check your module)
board_upload.flash_size = 4MB

# Upload speed (default 460800, increase for faster uploads)
upload_speed = 921600

# Monitor filters for ESP32 crash decoding
monitor_filters = esp32_exception_decoder
```

## Essential Commands

All commands run from `apps/<app-name>/` directory.

| Task                    | Command                          |
| ----------------------- | -------------------------------- |
| **Build all envs**      | `pio run`                        |
| **Build ESP32 only**    | `pio run -e esp32`               |
| **Build AVR only**      | `pio run -e arduino-uno`         |
| **Upload to ESP32**     | `pio run -t upload -e esp32`     |
| **Serial monitor**      | `pio device monitor`             |
| **Run unit tests**      | `pio test -e native`             |
| **Build filesystem**    | `pio run -t buildfs -e esp32`    |
| **Upload filesystem**   | `pio run -t uploadfs -e esp32`   |
| **Activate venv**       | `source .venv/bin/activate`      |
| **Clean build**         | `pio run -t clean -e esp32`      |
| **List connected boards** | `pio device list`              |
| **Update platforms**    | `pio pkg update`                 |

### Serial Monitor Options

```bash
# Basic monitor (115200 baud, default)
pio device monitor

# With ESP32 exception decoder (decodes crash backtraces)
pio device monitor --filter esp32_exception_decoder

# Specific port
pio device monitor --port /dev/cu.usbserial-0001

# Custom baud rate
pio device monitor --baud 115200

# Log to file
pio device monitor --log-dest monitor.log
```

### Upload and Monitor in One Step

```bash
pio run -t upload -e esp32 && pio device monitor
```

## Testing

### Unit Test Framework

PlatformIO uses the Unity test framework for native (host-side) testing. Test pure logic (color math, FFT processing, filters) on the host. Hardware-dependent code should be behind HAL interfaces.

### Test File Structure

```
apps/<app-name>/
  test/
    test_rgbw/
      test_color_conversion.cpp
    test_audio/
      test_fft.cpp
      test_dc_blocker.cpp
```

### Writing Tests

```cpp
// test/test_rgbw/test_color_conversion.cpp
#include <unity.h>
#include "rgbw.h"

void test_hsvToRGBW_red() {
    HSV hsv = {0, 255, 255};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_EQUAL_UINT8(255, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
}

void test_hsvToRGBW_white() {
    HSV hsv = {0, 0, 255};
    RGBW result = hsvToRGBW(hsv);
    // With white extraction, pure white should use the W channel
    TEST_ASSERT_EQUAL_UINT8(255, result.w);
}

void test_scaleBrightness_zero() {
    RGBW color = {255, 128, 64, 32};
    RGBW result = scaleBrightness(color, 0);
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_lerpRGBW_midpoint() {
    RGBW a = {0, 0, 0, 0};
    RGBW b = {200, 100, 50, 250};
    RGBW mid = lerpRGBW(a, b, 128);  // ~50%
    TEST_ASSERT_UINT8_WITHIN(2, 100, mid.r);
    TEST_ASSERT_UINT8_WITHIN(2, 50, mid.g);
}

// PlatformIO Unity entry point
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_hsvToRGBW_red);
    RUN_TEST(test_hsvToRGBW_white);
    RUN_TEST(test_scaleBrightness_zero);
    RUN_TEST(test_lerpRGBW_midpoint);
    return UNITY_END();
}
```

### Running Tests

```bash
# Run all tests in native environment
pio test -e native

# Run specific test suite
pio test -e native --filter test_rgbw

# Verbose output
pio test -e native -v
```

### What to Test on Host vs Hardware

| Test On Host (`native`) | Test On Hardware |
|------------------------|-----------------|
| Color conversion (HSV -> RGBW) | LED strip output (visual) |
| Brightness scaling math | PWM signal accuracy |
| Interpolation (lerp) | WiFi/BLE connectivity |
| FFT frequency analysis | I2S microphone sampling |
| Beat detection algorithm | ADC readings |
| JSON config parsing | NVS read/write |
| Animation state machines | Real-time frame rates |
| Protocol encoding | Level shifter signal integrity |

### Mocking Hardware Dependencies

The `native` environment uses `lib_ignore` to skip hardware-dependent libraries. For testing code that references hardware types:

```cpp
#ifdef NATIVE_BUILD
// Mock types for host compilation
struct MockNeoPixelBus {
    void SetPixelColor(uint16_t, uint32_t) {}
    void Show() {}
    void SetLuminance(uint8_t) {}
};
#endif
```

## Filesystem (LittleFS)

### Building and Uploading

Place web UI files, JSON configs, certificates in `data/` at the app root:

```
apps/my-app/
  data/
    index.html
    style.css
    config.json
  src/
    main.cpp
  platformio.ini
```

```bash
pio run -t buildfs -e esp32      # Build filesystem image from data/
pio run -t uploadfs -e esp32     # Upload to device
```

Required `platformio.ini` settings:
```ini
board_build.filesystem = littlefs
board_build.partitions = partitions.csv   ; if custom partition table
```

## Continuous Integration Tips

For CI/CD pipelines (GitHub Actions, etc.):

```bash
# Install PlatformIO
pip install platformio

# Build all environments
pio run

# Run tests
pio test -e native

# Check for compilation warnings (treat as errors)
# Add to platformio.ini: build_flags = -Werror
pio run -e esp32

# Size report
pio run -e esp32 -t size
```

### Build Size Monitoring

Track firmware size to avoid exceeding flash partition limits:

```bash
# Show RAM/Flash usage after build
pio run -e esp32 -v 2>&1 | grep -E "(RAM|Flash)"

# Detailed size report
pio run -e esp32 -t size
```

Typical ESP32 firmware sizes:
- Minimal (WiFi + LEDs): ~800 KB - 1 MB
- With BLE: +200-400 KB (Bluedroid) or +100-200 KB (NimBLE)
- With OTA: Needs dual app partitions (see partition table in esp32-internals.md)
