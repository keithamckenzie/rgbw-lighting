# Project Roadmap

Library upgrade paths, planned shared libraries, and known improvement notes for the RGBW lighting project.

## Library Upgrade Path

The current libraries use Adafruit NeoPixel and the default ESP32 BLE stack. For production firmware, consider these upgrades:

| Current | Upgrade | Benefit |
|---------|---------|---------|
| Adafruit NeoPixel | **NeoPixelBus** | Native `RgbColor`/`RgbwColor` types, selectable RMT/I2S/DMA drivers, built-in gamma correction, explicit SK6812/WS2815 method classes |
| ESP32 BLE stack | **NimBLE-Arduino** | Reported testing shows ~50% less flash and ~100 KB less RAM vs Bluedroid; aims for API compatibility |
| WebServer (sync) | **ESPAsyncWebServer** + WebSocket | Non-blocking, low-latency real-time color control |
| analogRead polling | **I2S ADC DMA mode** | Continuous audio sampling without CPU blocking |
| arduinoFFT | **ESP-DSP** (ESP32 only) | Xtensa SIMD-accelerated FFT; keep arduinoFFT for AVR/host tests |

### NeoPixelBus Migration Notes

NeoPixelBus uses template-based strip type selection instead of runtime flags:

```cpp
// Adafruit NeoPixel (current)
Adafruit_NeoPixel strip(NUM_LEDS, PIN, NEO_GRBW + NEO_KHZ800);
strip.setPixelColor(i, r, g, b, w);

// NeoPixelBus (upgrade) - SK6812 RGBW with RMT
#include <NeoPixelBus.h>
NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method> strip(NUM_LEDS, PIN);
strip.SetPixelColor(i, RgbwColor(r, g, b, w));

// NeoPixelBus - WS2815 RGB with RMT
NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod> strip(NUM_LEDS, PIN);
strip.SetPixelColor(i, RgbColor(r, g, b));
```

Key differences:
- Strip type is a compile-time template parameter, not runtime
- Built-in `RgbwColor` and `RgbColor` types with color math methods
- Built-in gamma correction via `NeoGamma<NeoGammaTableMethod>`
- Multiple driver backends: RMT (default, recommended), I2S, SPI, bit-bang
- When RMT channels are exhausted, switch specific strips to I2S or SPI method
- Do NOT mix Adafruit NeoPixel and NeoPixelBus in the same project (RMT channel conflicts)

### NimBLE-Arduino Migration Notes

NimBLE aims for API compatibility with the default ESP32 BLE stack. Key migration steps:

```cpp
// Current (Bluedroid)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// NimBLE (drop-in replacement for most code)
#include <NimBLEDevice.h>
// BLEServer, BLECharacteristic, etc. are aliased automatically
```

Changes needed:
- Replace `#include <BLE*.h>` with `#include <NimBLE*.h>` (or use compatibility header)
- `BLE2902` descriptor is handled automatically by NimBLE (no manual creation needed)
- Some callback signatures differ slightly — check NimBLE docs
- Memory savings: ~50% less flash, ~100 KB less RAM vs Bluedroid (reported by the library)
- Add to `platformio.ini`: `lib_deps = h2zero/NimBLE-Arduino@^1.4.0`

### ESPAsyncWebServer Migration Notes

```cpp
// Current (synchronous WebServer)
#include <WebServer.h>
WebServer server(80);
server.on("/color", HTTP_POST, handleColor);

// Async (upgrade)
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

server.on("/color", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Handle REST endpoint
    request->send(200, "application/json", "{\"ok\":true}");
});

// WebSocket for real-time color control
ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
              AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        // Parse 4-byte RGBW color
        if (len >= 4) {
            RGBW color = {data[0], data[1], data[2], data[3]};
            // Apply color immediately
        }
    }
});
server.addHandler(&ws);
```

Benefits:
- Non-blocking request handling (no blocking the main loop)
- WebSocket support for low-latency real-time color updates
- Better performance under concurrent connections
- Add to `platformio.ini`: `lib_deps = me-no-dev/ESPAsyncWebServer@^1.2.3` (or the maintained fork `mathieucarbou/ESPAsyncWebServer`)

## Planned Shared Libraries

| Library | Purpose | Platform | Status |
|---------|---------|----------|--------|
| `AudioInput` | I2S mic + RCA ADC, FFT, beat detection, band analysis | ESP32 | Planned |
| `InputManager` | Debounced buttons, rotary encoders, switch matrices | All | Planned |
| `PowerManager` | Current limiting, power budget enforcement | All | Planned |

### AudioInput Library Design

Planned API:
```cpp
AudioInput audio;
audio.begin(AudioInput::I2S_MIC, PIN_BCK, PIN_WS, PIN_DIN);
// or
audio.begin(AudioInput::ADC_RCA, PIN_ADC);

// In task loop
AudioSpectrum spectrum = audio.getSpectrum();
float bassEnergy = spectrum.bandEnergy(20, 250);
float midEnergy = spectrum.bandEnergy(250, 2000);
float highEnergy = spectrum.bandEnergy(2000, 16000);
bool beatDetected = audio.isBeat();
```

### InputManager Library Design

Planned API:
```cpp
InputManager input;
input.addButton(PIN_MODE, InputManager::PULLUP, [](ButtonEvent e) {
    if (e == ButtonEvent::ShortPress) cycleMode();
    if (e == ButtonEvent::LongPress) togglePower();
});
input.addEncoder(PIN_A, PIN_B, [](int delta) {
    adjustBrightness(delta);
});
input.update();  // Call in loop or task
```

### PowerManager Library Design

Planned API:
```cpp
PowerManager power;
power.setStripBudget(0, 2400);  // Strip 0: max 2.4A
power.setStripBudget(1, 900);   // Strip 1: max 0.9A
power.setPWMBudget(5000);       // PWM channels: max 5A

// Before showing
RGBW* buffer = strip.getBuffer();
power.enforce(buffer, numLeds, stripIndex);  // Scales brightness to stay in budget
strip.show();
```

## Known Improvement Notes

These are documented improvements to apply as the codebase matures:

### LEDPWM Library

- **Resolution upgrade:** Increase default from 8-bit to 12-bit. 8-bit has visible stepping at low brightness; 12-bit at 19.5 kHz is the sweet spot for smooth dimming.
- **Gamma correction:** Add gamma correction in `_applyColor()`. Currently outputs linear PWM which doesn't match human brightness perception. See gamma correction details in [led-control.md](led-control.md#gamma-correction).
- **Hardware fade:** Consider adding `ledcFade()` for hardware-accelerated smooth transitions between colors. This offloads transition computation to the LEDC peripheral.

### RGBWCommon Library

- **Brightness scaling fix:** `scaleBrightness()` uses `>> 8` which means brightness=255 yields ~99.6% output (254.something). Fix: use `((channel * (brightness + 1)) >> 8)` for accurate full-scale output where brightness=255 produces the exact input value.
- **White extraction config:** Make white extraction configurable per LED type. SK6812 comes in warm white (~3000K), neutral white (~4000K), and cool white (~5000K) variants. The extraction ratio should account for the white LED's color temperature to maintain accurate color rendering.

### LEDStrip Library

- ~~**Static pointer issue:** The static `_neopixel` pointer prevents multiple strip instances.~~ **Fixed:** `_neopixel` is now a per-instance member. Multiple strips with different types (SK6812/WS2815B) are supported.

### Connectivity/WiFiManager

- **Blocking delay:** The `delay(100)` busy-wait in `connect()` blocks the main thread. Replace with `vTaskDelay(pdMS_TO_TICKS(100))` in FreeRTOS context for cooperative multitasking.
- **Async web server:** Migrate to ESPAsyncWebServer for real-time color control via WebSocket. The synchronous WebServer blocks during request handling.

### Connectivity/BLEManager

- **Notifications:** Add NOTIFY property and BLE2902 descriptors to characteristics. This enables the client to subscribe to brightness/color changes pushed from the device.
- **Singleton pattern:** Replace file-scope `_instance` singleton with static class member for cleaner architecture.

## Git Conventions

- Sensitive files (`secrets.h`, `credentials.h`) are gitignored.
- PlatformIO build artifacts (`.pio/`, `build/`) are gitignored.
- IDE files are gitignored except `.vscode/extensions.json` and `.vscode/settings.json` if shared.
