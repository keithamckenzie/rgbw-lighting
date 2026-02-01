# Coding Standards

C++ language standards, naming conventions, memory management, platform abstraction, and logging for the RGBW lighting project.

## C++ Standard

- **ESP32:** C++17 (`-std=gnu++17`). Use `constexpr`, `std::optional`, structured bindings, `if constexpr` freely.
- **Arduino AVR:** C++11 (compiler default). No C++14/17 features.
- **Shared libraries** (in `shared/lib/`): Must compile as **C++11** for cross-platform compatibility. Guard C++17 features with `#if __cplusplus >= 201703L`.

### C++17 Features Available on ESP32

| Feature | Example | Notes |
|---------|---------|-------|
| `constexpr if` | `if constexpr (sizeof(T) > 4)` | Compile-time branching, eliminates `#ifdef` in templates |
| `std::optional` | `std::optional<RGBW> color;` | Safe nullable values without pointers |
| Structured bindings | `auto [r, g, b, w] = color;` | Destructure structs and arrays |
| `std::string_view` | `void log(std::string_view msg)` | Non-owning string references (avoid on AVR) |
| Fold expressions | `(args + ...)` | Variadic template expansion |
| Nested namespaces | `namespace a::b::c {}` | Cleaner namespace declarations |
| Class template argument deduction | `std::array vals = {1, 2, 3};` | Compiler deduces template args |
| `inline` variables | `inline constexpr int kVal = 5;` | Header-safe global constants |

### C++11 Limitations on AVR

AVR's C++11 lacks:
- No `std::optional`, `std::variant`, `std::any`
- No `constexpr if`
- No structured bindings
- Limited STL (no `<algorithm>`, `<string>`, `<vector>` in practice due to memory)
- No `std::function` (too much overhead for 2 KB RAM)

Use plain C constructs, function pointers, and simple templates for shared library code.

## Naming Conventions (established by existing code)

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

### Enum Best Practices

Use scoped enums (`enum class`) with explicit underlying type for embedded:
```cpp
// Good: scoped, sized, serializable
enum class LedError : uint8_t {
    Ok = 0,
    InvalidIndex,
    BufferFull,
    HardwareFailure,
    Timeout,
};

// Good: scoped enum for modes
enum class AnimationMode : uint8_t {
    Solid = 0,
    Rainbow,
    Breathe,
    Chase,
    SoundReactive,
};
```

Benefits:
- Explicit size control (important for NVS storage and BLE characteristics)
- No implicit conversion to `int` (catches bugs)
- Namespace isolation (no name collisions)

## Header Files

- Use `#pragma once` (already established).
- Platform-specific code: wrap entire file body in `#ifdef ESP32` / `#endif`.
- Include order: Arduino/system headers, then library headers, then local headers.

### Include Order Example

```cpp
// 1. Arduino/system headers
#include <Arduino.h>
#include <stdint.h>

// 2. Platform-specific system headers
#ifdef ESP32
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#endif

// 3. External library headers
#include <NeoPixelBus.h>

// 4. Project library headers
#include "rgbw.h"

// 5. Local headers
#include "pins.h"
#include "config.h"
```

## Strings

- **Never use Arduino `String` class** in production code. It causes heap fragmentation on constrained devices.
- Use `char[]` buffers with `snprintf()` for string formatting.
- For log messages, use ESP_LOG macros (ESP32) or a cross-platform wrapper.

### Why Arduino String is Dangerous

The `String` class uses dynamic heap allocation. On ESP32 (320 KB DRAM) and especially AVR (2 KB RAM):
- Each concatenation allocates a new buffer and frees the old one
- Repeated operations fragment the heap into unusable small blocks
- Eventually `malloc()` fails even with "free" memory available
- Symptoms: random crashes, watchdog resets, WiFi disconnects

### Safe String Patterns

```cpp
// Good: stack-allocated, fixed size
char buf[64];
snprintf(buf, sizeof(buf), "LED %d: R=%u G=%u B=%u", idx, c.r, c.g, c.b);

// Good: compile-time string in flash (ESP32)
static const char* TAG = "LEDStrip";
ESP_LOGI(TAG, "Pixel %d set to (%u,%u,%u,%u)", idx, c.r, c.g, c.b, c.w);

// Good: for JSON config responses
char json[256];
snprintf(json, sizeof(json),
    "{\"brightness\":%u,\"mode\":\"%s\"}",
    brightness, modeName);

// Bad: heap fragmentation
String msg = "LED " + String(idx) + ": " + String(c.r);  // NEVER
```

## No Exceptions

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

### Error Handling Patterns

```cpp
// Return error codes from functions
LedError readSensorValue(uint8_t channel, uint16_t* value) {
    if (channel >= MAX_CHANNELS) return LedError::InvalidIndex;
    if (!value) return LedError::InvalidIndex;
    *value = analogRead(channel);
    return LedError::Ok;
}

// Check at call site
uint16_t sensorVal;
if (readSensorValue(0, &sensorVal) != LedError::Ok) {
    ESP_LOGE(TAG, "Failed to read sensor on channel 0");
}
```

On ESP32 with C++17, `std::optional` is another option for functions that may fail:
```cpp
#if __cplusplus >= 201703L
std::optional<RGBW> parseColor(const uint8_t* data, size_t len) {
    if (len < 4) return std::nullopt;
    return RGBW(data[0], data[1], data[2], data[3]);
}
#endif
```

## Memory Management

### Stack vs Heap

- Pre-allocate all buffers in `setup()` or at construction time. Never allocate in tight loops.
- FreeRTOS task stack size: 4096 bytes minimum for LED tasks, 8192+ for WiFi/BLE tasks.
- ESP32 has ~520 KB SRAM total (approximately 320 KB DRAM + 200 KB IRAM per the Technical Reference Manual; exact usable amounts depend on IDF configuration). Arduino Uno has 2 KB. Size buffers accordingly.
- Use `static` or global arrays for fixed-size buffers. Avoid `new`/`malloc` after initialization.

### Memory Budget Reference

| Platform | Total SRAM | Typical Available | Notes |
|----------|-----------|-------------------|-------|
| ESP32 | 520 KB | ~200-300 KB | WiFi/BLE reserves ~80-150 KB |
| ESP32-S3 | 512 KB | ~200-300 KB | Similar to ESP32 |
| ESP32-C3 | 400 KB | ~200-250 KB | RISC-V, single core |

SRAM totals are per datasheet; usable heap is less due to IDF, stack, and static allocations.
| Arduino Uno | 2 KB | ~1.5 KB | After stack and globals |
| Arduino Mega | 8 KB | ~6 KB | |

### LED Buffer Sizing

Each pixel requires:
- RGBW (SK6812): 4 bytes per pixel in application buffer + NeoPixelBus internal buffer
- RGB (WS2815B): 4 bytes in app buffer (RGBW model) + 3 bytes in NeoPixelBus internal buffer

| Strip Length | App Buffer | NeoPixelBus Buffer (RGBW) | NeoPixelBus Buffer (RGB) | Total (RGBW) |
|-------------|-----------|----------------------|---------------------|-------------|
| 30 LEDs | 120 B | 120 B | 90 B | 240 B |
| 60 LEDs | 240 B | 240 B | 180 B | 480 B |
| 150 LEDs | 600 B | 600 B | 450 B | 1200 B |
| 300 LEDs | 1200 B | 1200 B | 900 B | 2400 B |
| 600 LEDs | 2400 B | 2400 B | 1800 B | 4800 B |

On AVR, 150 RGBW LEDs (1200 B total) consumes 60% of available RAM. Keep strips short or use ESP32.

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

### Monitoring Free Memory

```cpp
// ESP32: check heap
ESP_LOGI(TAG, "Free heap: %u bytes", esp_get_free_heap_size());
ESP_LOGI(TAG, "Min free heap ever: %u bytes", esp_get_minimum_free_heap_size());

// FreeRTOS: check task stack high-water mark
UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack HWM: %u words (%u bytes)", hwm, hwm * 4);

// AVR: approximate free RAM
#ifndef ESP32
extern int __heap_start, *__brkval;
int freeRam() {
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
#endif
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

### Platform Detection Macros

| Macro | When Defined | Use For |
|-------|-------------|---------|
| `ESP32` | ESP32, ESP32-S2, S3, C3 | All ESP32 variants |
| `ESP_PLATFORM` | ESP-IDF builds | IDF-specific APIs |
| `ARDUINO` | Arduino framework | Arduino API calls |
| `__AVR__` | AVR targets | AVR-specific code |
| `UNIT_TEST` | Native test builds | Mocking hardware |
| `NATIVE_BUILD` | Host compilation | Skip hardware init |

### HAL Pattern for Testability

For code that needs to run on host (unit tests) and hardware:

```cpp
// hal.h - Hardware Abstraction Layer
class IHAL {
public:
    virtual uint32_t millis() = 0;
    virtual void pinMode(uint8_t pin, uint8_t mode) = 0;
    virtual void digitalWrite(uint8_t pin, uint8_t val) = 0;
    virtual ~IHAL() = default;
};

// hal_arduino.h - Real hardware
class ArduinoHAL : public IHAL {
public:
    uint32_t millis() override { return ::millis(); }
    void pinMode(uint8_t pin, uint8_t mode) override { ::pinMode(pin, mode); }
    void digitalWrite(uint8_t pin, uint8_t val) override { ::digitalWrite(pin, val); }
};

// hal_mock.h - For unit tests
class MockHAL : public IHAL {
public:
    uint32_t _millis = 0;
    uint32_t millis() override { return _millis; }
    void pinMode(uint8_t, uint8_t) override {}
    void digitalWrite(uint8_t, uint8_t) override {}
};
```

## Logging

### ESP32

Use ESP_LOG macros with one `TAG` per file:

```cpp
#include <esp_log.h>
static const char* TAG = "LEDStrip";

ESP_LOGE(TAG, "error message");    // Always shown
ESP_LOGW(TAG, "warning message");  // Warnings
ESP_LOGI(TAG, "info message");     // Normal operation
ESP_LOGD(TAG, "debug message");    // Debug builds
ESP_LOGV(TAG, "verbose message");  // Verbose builds
```

Control verbosity via `CORE_DEBUG_LEVEL` build flag:

| Level | Value | Shows |
|-------|-------|-------|
| None | 0 | Nothing (release builds) |
| Error | 1 | `ESP_LOGE` only |
| Warn | 2 | `ESP_LOGE` + `ESP_LOGW` |
| Info | 3 | + `ESP_LOGI` (default for dev) |
| Debug | 4 | + `ESP_LOGD` |
| Verbose | 5 | Everything |

### Cross-Platform Logging Wrapper

For shared libraries that run on both ESP32 and AVR, use a macro wrapper:

```cpp
// log_wrapper.h
#pragma once

#ifdef ESP32
    #include <esp_log.h>
    #define LOG_E(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
    #define LOG_W(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
    #define LOG_I(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
    #define LOG_D(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#elif defined(NATIVE_BUILD)
    #include <cstdio>
    #define LOG_E(tag, fmt, ...) fprintf(stderr, "[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_W(tag, fmt, ...) fprintf(stderr, "[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_I(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_D(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
    // AVR: Serial-based logging
    #define LOG_E(tag, fmt, ...) Serial.printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_W(tag, fmt, ...) Serial.printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_I(tag, fmt, ...) Serial.printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_D(tag, fmt, ...)  // No-op on AVR to save flash
#endif
```

Note: AVR `Serial.printf` is available on some cores but not all. For maximum portability on AVR, use `Serial.print()` with manual formatting or a lightweight printf library.
