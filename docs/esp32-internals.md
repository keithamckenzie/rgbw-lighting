# ESP32 Internals

FreeRTOS, WiFi/BLE connectivity, persistent storage (NVS/LittleFS), I2C, OTA updates, and mDNS.

## FreeRTOS

### Task Guidelines

Suggested starting points -- adjust stack sizes based on actual usage (use `uxTaskGetStackHighWaterMark()` to measure):

| Task                  | Priority | Core | Stack Size (starting point) |
|----------------------|----------|------|-----------------------------|
| LED animation/update | 3        | 1    | 4096                        |
| Audio FFT processing | 4        | 1    | 8192                        |
| WiFi/BLE handling    | 1        | 0    | 4096                        |
| Web server           | 2        | 0    | 8192                        |
| Sensor polling       | 2        | 1    | 4096                        |

- Use `xTaskCreatePinnedToCore()` to assign tasks to specific cores.
- Use `vTaskDelayUntil()` for periodic tasks (maintains consistent period), not `vTaskDelay()` or `delay()`.

### Task Notifications vs Queues

**Task notifications** are built into each task (no separate object needed), significantly faster than binary semaphores ([FreeRTOS documentation](https://www.freertos.org/RTOS-task-notifications.html) notes they require no separate kernel object — the overhead is a small per-task field already in the task control block). **Queues** support multiple producers, buffer multiple messages, and handle structured data.

| Data Flow | Mechanism | Rationale |
|-----------|-----------|-----------|
| BLE callback -> LED task (color) | Task notification (`eSetValueWithOverwrite`) | RGBW fits in 32 bits (`r<<24 \| g<<16 \| b<<8 \| w`). Latest value wins. |
| Audio ISR -> FFT task (buffer ready) | Task notification (`eIncrement` or `eSetBits`) | Lightweight ISR-safe wake-up |
| FFT task -> LED task (spectrum) | Queue of pointers | Structured data, needs buffering |
| WebSocket -> LED task (commands) | Queue | Multiple clients may send simultaneously |
| Button ISR -> main loop | Task notification (`eSetBits`) | Set a bit per button, ISR-safe via `xTaskNotifyFromISR` |

### Stack Overflow Detection

ESP-IDF provides two methods (configured in `sdkconfig`):

1. **Method 1:** Checks stack pointer at context switch. Fast but can miss corruption between switches.
2. **Method 2:** Fills stack with `0xA5A5A5A5` at creation, checks a known pattern at the end of the stack at each context switch. More reliable, small overhead. **Use this for development.**

```cpp
// Measure actual stack usage periodically
UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack HWM: %u bytes free", hwm * sizeof(StackType_t));
// Size stacks to maintain 200-512 bytes headroom beyond high-water mark
```

### Heap Fragmentation Monitoring

```cpp
size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
float fragmentation = 100.0f * (1.0f - (float)largestBlock / (float)freeHeap);
ESP_LOGI(TAG, "Heap: %u free, %u largest, %.1f%% fragmented",
         freeHeap, largestBlock, fragmentation);
```

### Queues and Synchronization

- Use `xQueueSend()` / `xQueueReceive()` to pass data between tasks (e.g., audio samples -> FFT -> LED update).
- Use mutexes (`xSemaphoreCreateMutex()`) to protect shared resources (I2C bus, LED strip buffer). Mutexes provide priority inheritance; binary semaphores do not.
- In ISRs, use only `*FromISR()` variants of FreeRTOS calls.

### ISR Rules

- Mark with `IRAM_ATTR` (function must be in IRAM, not flash).
- No heap allocation, no Serial.print, no ESP_LOG. Avoid floating point (hardware FPU context is not saved by default in ISRs).
- Keep execution time as short as possible (rule of thumb: single-digit microseconds).
- Use `xQueueSendFromISR()` to pass data out; call `portYIELD_FROM_ISR()` if a higher-priority task was woken.

### Watchdog

```cpp
#include <esp_task_wdt.h>
esp_task_wdt_init(10, true);     // 10s timeout, panic on trigger
esp_task_wdt_add(nullptr);       // Register current task
esp_task_wdt_reset();            // Feed in task loop
```

Unsubscribe before long-blocking operations (OTA, large flash writes).

**Common pitfall:** The idle task on Core 0 feeds the task watchdog. If you run a continuous task on Core 0 at priority >= 1 without ever yielding, the watchdog triggers. Always include a `vTaskDelay` or blocking call in every task loop.

### Task Teardown and Error Handling

Stopping a FreeRTOS task that uses I2S or other blocking drivers requires care. A task blocked inside `i2s_read()` will not respond to a simple flag change.

**Teardown sequence:**

1. **Stop the peripheral first.** Call `i2s_stop()` / `i2s_driver_uninstall()` before killing the task. This unblocks any pending `i2s_read()` call (it returns with an error or zero bytes).
2. **Signal the task to exit.** Set a `volatile bool running = false` flag that the task checks each iteration.
3. **Wait for the task to acknowledge.** Use a binary semaphore (`xSemaphoreGive` from the task just before it returns, `xSemaphoreTake` with a timeout from the caller). Do not call `vTaskDelete()` from outside without confirmation — the task may be mid-write to shared state.
4. **Clean up resources** (free buffers, delete queue, delete semaphore) only after the task has exited.

```cpp
// Task side
void audioTaskFunc(void* param) {
    Impl* impl = (Impl*)param;
    while (impl->running) {
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_NUM_0, buf, bufSize, &bytesRead, pdMS_TO_TICKS(200));
        if (err != ESP_OK || bytesRead == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));  // Backoff on failure — avoids pegging the core
            continue;
        }
        // ... process audio ...
    }
    xSemaphoreGive(impl->taskExitSem);  // Signal clean exit
    vTaskDelete(NULL);
}

// Teardown side
void stopAudioTask(Impl* impl) {
    impl->running = false;
    i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    // Wait for task to exit (1 second timeout)
    xSemaphoreTake(impl->taskExitSem, pdMS_TO_TICKS(1000));
    // Now safe to free buffers, delete queue, etc.
}
```

**Key points:**
- **Backoff on read failures.** If `i2s_read()` fails or returns 0 bytes, add a short `vTaskDelay(pdMS_TO_TICKS(10))` before retrying. Without this, the task will spin at maximum priority and starve other tasks on the same core.
- **Timeout on `i2s_read()`.** Use a bounded timeout (e.g., 200 ms) instead of `portMAX_DELAY`. This ensures the task re-checks the `running` flag periodically even if I2S stalls.
- **Never call `vTaskDelete(taskHandle)` from outside** on a task that holds resources. The task may be in the middle of writing to a queue or holding a mutex.

### Common FreeRTOS Pitfalls

- **Priority inversion:** Use `xSemaphoreCreateMutex()` (not binary semaphore) for priority inheritance.
- **Core 0 watchdog starvation:** Any task on Core 0 that never yields will starve the idle task and trigger watchdog.
- **`delay()` vs `delayMicroseconds()`:** `delay()` maps to `vTaskDelay()` (yields to scheduler). `delayMicroseconds()` is a busy-wait that blocks the entire core.
- **WiFi stack size:** User tasks on Core 0 that call WiFi APIs need 8192+ byte stacks.

## WiFi and BLE

### WiFi

- Use event-driven connection management (WiFi event callbacks), not blocking loops.
- Implement exponential backoff for reconnection (5s -> 10s -> 20s -> 40s -> 60s cap).
- WiFi protocol stack runs on **Core 0** by default (configurable via `CONFIG_ESP32_WIFI_TASK_CORE_ID`). Pin application tasks to **Core 1**.
- Set `WiFi.setAutoReconnect(true)` for basic reconnection handling.

### mDNS

Use `ESPmDNS.h` (bundled with arduino-esp32, no extra dependency) to make the device discoverable as `hostname.local`:

```cpp
#include <ESPmDNS.h>

// Call AFTER WiFi connects (in event callback or after waitForConnectResult)
MDNS.begin("rgbw-controller");           // -> rgbw-controller.local
MDNS.addService("http", "tcp", 80);      // advertise web server
```

- Call `WiFi.setHostname("rgbw-controller")` **before** `WiFi.begin()` to also set the DHCP hostname.
- On WiFi reconnect, call `MDNS.end()` then `MDNS.begin()` again.
- Generally works on iOS, macOS, Windows 10+, and Linux (Avahi). mDNS support on Android varies by version and manufacturer; some devices may need a third-party app for `.local` resolution.

### BLE

- The Connectivity library defines a GATT service for RGBW control:
  - Service UUID: `0000FF00-0000-1000-8000-00805F9B34FB`
  - Color characteristic (0xFF01): WRITE, 4 bytes (R, G, B, W)
  - Brightness characteristic (0xFF02): READ/WRITE, 1 byte (0-255)
- Release classic BT memory with `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)` to free heap used by the classic BT controller (amount varies by IDF version and configuration).
- BLE does NOT auto-restart advertising on disconnect. Explicitly call `pServer->startAdvertising()` in the `onDisconnect` callback or main loop.

### NimBLE vs Bluedroid Memory Comparison

Approximate memory comparison (values reported by the NimBLE-Arduino library; actual savings vary by IDF version, configuration, and features used):

| Resource | Bluedroid BLE | NimBLE-Arduino | Savings |
|----------|--------------|----------------|---------|
| Flash | ~350 KB | ~170 KB | ~180 KB (~50%) |
| RAM (heap) | ~150-200 KB | ~50-70 KB | ~100-130 KB |

The NimBLE migration alone frees enough RAM to make a full system (WiFi + BLE + Audio + LEDs) viable on ESP32. See [project-roadmap.md](project-roadmap.md#nimble-arduino-migration-notes) for migration details.

### WiFi + BLE Coexistence

- Both share the ESP32 radio. Coexistence is handled by the IDF coexistence module.
- On AVR, `show()` disables interrupts during transmission, which can disrupt other time-critical tasks and interrupts (serial, timers, etc.). On ESP32, the RMT peripheral handles timing in hardware without disabling interrupts. For long strips, keep update frequency reasonable to avoid starving other tasks.

## 433 MHz RF Remote Control

433 MHz RF provides simple, low-latency wireless control without WiFi/BLE overhead. A remote sends fixed codes; the ESP32 receives and maps them to actions.

### Hardware

**Remote:** Sonoff RM433R2 (or similar 433 MHz EV1527-based remote). Each button transmits a unique fixed code. The code is set at the factory and printed on a sticker or discoverable via the learning process below.

**Receiver:** RXB6 433 MHz superheterodyne receiver module. Superheterodyne receivers have significantly better sensitivity and range than the cheaper regenerative (XY-MK-5V) modules.

| RXB6 Pin | Connection |
|----------|------------|
| VCC | 3.3V or 5V (5V gives better range) |
| GND | ESP32 GND |
| DATA | ESP32 GPIO (any safe input pin, e.g., GPIO 27) |
| ANT | 17.3 cm straight wire (quarter-wave antenna for 433 MHz) |

**Antenna:** A 17.3 cm straight wire soldered to the ANT pad provides a basic quarter-wave antenna. For better range, use a coiled helical antenna or an SMA connector with a proper 433 MHz antenna. Without an antenna, range is limited to ~1-2 meters.

### Software (rc-switch Library)

The `rc-switch` library (`sui77/rc-switch`) handles decoding of common 433 MHz protocols (EV1527, PT2262, etc.).

**PlatformIO dependency** (already in `apps/example-rgbw/platformio.ini`):
```ini
lib_deps =
    sui77/rc-switch@^2.6.4
```

### Learning Button Codes

Run this sketch to discover the codes transmitted by each remote button:

```cpp
#include <RCSwitch.h>

#define RF_RX_PIN 27

RCSwitch rf = RCSwitch();

void setup() {
    Serial.begin(115200);
    rf.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
}

void loop() {
    if (rf.available()) {
        Serial.printf("Code: %lu  Bits: %u  Protocol: %u\n",
                       rf.getReceivedValue(),
                       rf.getReceivedBitlength(),
                       rf.getReceivedProtocol());
        rf.resetAvailable();
    }
}
```

Press each remote button and note the code values. These are stable per-button (fixed code, not rolling).

### Mapping Codes to Actions

```cpp
#include <RCSwitch.h>

#define RF_RX_PIN 27

// Codes discovered from the learning sketch
static const uint32_t RF_CODE_ON     = 1234567;  // Replace with actual codes
static const uint32_t RF_CODE_OFF    = 1234568;
static const uint32_t RF_CODE_MODE   = 1234569;
static const uint32_t RF_CODE_BRIGHT = 1234570;

RCSwitch rf = RCSwitch();
uint32_t lastRfTime = 0;
const uint32_t RF_DEBOUNCE_MS = 250;  // Ignore repeated codes within 250 ms

void setup() {
    rf.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
}

void handleRF() {
    if (!rf.available()) return;

    uint32_t now = millis();
    uint32_t code = rf.getReceivedValue();
    rf.resetAvailable();

    if (code == 0 || (now - lastRfTime) < RF_DEBOUNCE_MS) return;
    lastRfTime = now;

    switch (code) {
        case RF_CODE_ON:     powerOn();         break;
        case RF_CODE_OFF:    powerOff();        break;
        case RF_CODE_MODE:   cycleMode();       break;
        case RF_CODE_BRIGHT: cycleBrightness(); break;
    }
}
```

### Notes

- **Latency:** 433 MHz RF adds variable transmission latency (estimated tens of milliseconds; depends on protocol, encoding, and environment). For sound-reactive modes, consider [beat quantization](audio-reactive.md#beat-quantization) to absorb timing jitter.
- **Range:** Depends heavily on antenna, supply voltage, obstacles, and interference. With RXB6 at 5V and a 17.3 cm quarter-wave antenna, indoor range through walls is typically several tens of meters; line-of-sight range is longer. Test in your actual environment.
- **Interference:** Other 433 MHz devices (garage doors, weather stations, smart plugs) may trigger spurious codes. The debounce and code-matching logic filters these out, but avoid using generic codes that might collide.
- **ISR usage:** `rc-switch` uses a pin interrupt internally. The GPIO pin must support interrupts (`digitalPinToInterrupt()` must return a valid value).
- **Security:** EV1527 uses OTP fixed codes (20-bit address + 4-bit data), not rolling or encrypted codes. Receivers "learn" a remote's fixed code during a pairing/learning step, but the code itself is static and can be replayed. Acceptable for lighting control but not for security-sensitive applications.

### Credentials and Security

- Store WiFi credentials in `secrets.h` (gitignored) or ESP32 NVS (Non-Volatile Storage).
- Never commit credentials. The `.gitignore` already excludes `secrets.h` and `credentials.h`.
- Use WPA2 minimum for WiFi. Use HTTPS for any web configuration endpoints.

### OTA Updates

- Use ArduinoOTA or ESP-IDF OTA partition scheme.
- Always verify firmware signatures before applying updates.
- Disable watchdog timer during OTA operations, re-enable after.

## Persistent Storage

### NVS (Preferences)

Use `Preferences.h` for small typed settings (WiFi credentials, LED presets, brightness, mode). NVS is a key-value store in a dedicated flash partition with automatic wear leveling.

```cpp
#include <Preferences.h>

Preferences prefs;
prefs.begin("led_preset", false);    // namespace (max 15 chars), read-write
prefs.putUChar("brightness", 200);
prefs.putString("ssid", ssid);       // key max 15 chars
uint8_t b = prefs.getUChar("brightness", 255);  // default if not found
prefs.end();
```

**Limits:** Key and namespace names max 15 characters. String/blob values max ~4000 bytes (depends on NVS partition page size; see ESP-IDF NVS documentation). NVS partition size depends on the partition table; common defaults are 16-24 KB, sufficient for hundreds of key-value pairs.

**Wear leveling:** NVS uses log-structured storage; writes append rather than overwrite in-place. For settings that change frequently (e.g., brightness slider), throttle writes with a dirty flag and periodic flush (every 30s or on shutdown) rather than writing on every change.

### NVS Write Throttling Pattern

```cpp
static bool nvsDirty = false;
static uint32_t lastNvsWrite = 0;
const uint32_t NVS_FLUSH_INTERVAL_MS = 30000;  // 30 seconds

void setBrightness(uint8_t value) {
    _brightness = value;
    nvsDirty = true;
}

void flushNvsIfNeeded() {
    if (nvsDirty && (millis() - lastNvsWrite > NVS_FLUSH_INTERVAL_MS)) {
        Preferences prefs;
        prefs.begin("led_preset", false);
        prefs.putUChar("brightness", _brightness);
        prefs.end();
        nvsDirty = false;
        lastNvsWrite = millis();
    }
}
```

### LittleFS (File Storage)

Use LittleFS for larger files: web UI (HTML/CSS/JS), JSON config, certificates. **SPIFFS is available but not actively developed -- prefer LittleFS for new projects.**

PlatformIO setup in `platformio.ini`:

```ini
board_build.filesystem = littlefs
board_build.partitions = partitions.csv   ; custom partition table
```

Place files in `data/` at the app root. Build and upload:

```bash
pio run -t buildfs -e esp32      # Build filesystem image from data/
pio run -t uploadfs -e esp32     # Upload to device
```

Code usage:

```cpp
#include <LittleFS.h>

if (!LittleFS.begin(true)) { return; }  // true = format on first failure
File f = LittleFS.open("/config.json", "r");
```

### Partition Tables

OTA requires dual app slots. Create `partitions.csv` in the app root for a 4 MB flash layout:

```csv
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x5000
otadata,   data, ota,     0xE000,   0x2000
app0,      app,  ota_0,   0x10000,  0x1A0000
app1,      app,  ota_1,   0x1B0000, 0x1A0000
littlefs,  data, spiffs,  0x360000, 0xA0000
```

| Partition | Size | Purpose |
|-----------|------|---------|
| nvs | 20 KB (varies by partition table) | Key-value settings |
| otadata | 8 KB | OTA boot selection |
| app0 | 1664 KB | Primary firmware |
| app1 | 1664 KB | OTA update slot |
| littlefs | 640 KB | Web UI, config files |

In Arduino-ESP32, the LittleFS partition reuses the `spiffs` subtype (0x82) -- this is a framework convention; the actual filesystem is determined by `board_build.filesystem`. ESP-IDF users should check their framework's partition type requirements. App partitions must be 64 KB aligned.

## I2C Best Practices

### Bus Configuration

- Initialize I2C once in `setup()`, not in each device driver.
- ESP32 allows arbitrary SDA/SCL pins via `Wire.begin(SDA_PIN, SCL_PIN)`.
- Default speed after `Wire.begin()` is **100 kHz** (standard mode). Call `Wire.setClock(400000)` for 400 kHz fast mode. Drop back to 100 kHz if NACK errors occur with long wires (>30 cm).

### Pull-Up Resistors

- ESP32 internal pull-ups (tens of kohm) are too weak for reliable I2C, especially at 400 kHz.
- **Always use external pull-ups:** 4.7 kohm for 100 kHz, 2.2 kohm for 400 kHz.
- One set of pull-ups per bus, not per device. Pull up to 3.3V.

### Thread Safety

Protect I2C bus with a mutex when accessed from multiple FreeRTOS tasks:

```cpp
static SemaphoreHandle_t i2cMutex = xSemaphoreCreateMutex();

bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;  // Timeout
    }
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);  // Repeated start
    Wire.requestFrom(addr, (uint8_t)len);
    for (size_t i = 0; i < len && Wire.available(); i++) {
        data[i] = Wire.read();
    }
    xSemaphoreGive(i2cMutex);
    return true;
}
```

### Error Recovery

If repeated NACK/timeout errors occur, perform bus recovery by toggling SCL 9 times to release a stuck slave device, then re-initialize with `Wire.begin()`.

```cpp
void i2cBusRecovery(uint8_t sdaPin, uint8_t sclPin) {
    Wire.end();
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, OUTPUT);

    // Toggle SCL 9 times to release stuck slave
    for (int i = 0; i < 9; i++) {
        digitalWrite(sclPin, LOW);
        delayMicroseconds(5);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(5);
    }

    // Generate STOP condition
    pinMode(sdaPin, OUTPUT);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(sdaPin, HIGH);
    delayMicroseconds(5);

    // Re-initialize
    Wire.begin(sdaPin, sclPin);
}
```

## RAM Budget (Full System)

Estimated RAM usage with all subsystems active:

| Component | RAM Usage |
|-----------|-----------|
| NimBLE (1 connection) | ~50-70 KB |
| WiFi (STA mode) | ~50-70 KB |
| AsyncWebServer + 2 WS clients | ~10-15 KB |
| LED pixel buffer (120 LEDs) | ~480 B |
| FFT buffers (1024-pt, float) | ~12 KB |
| I2S DMA buffers (4 x 1024 x 2B) | ~8 KB |
| FreeRTOS tasks (3 app tasks) | ~16 KB |
| **Total estimated** | **~160-190 KB** |
| **Available DRAM** | **~320 KB** |
| **Remaining** | **~130-160 KB** |

With Bluedroid instead of NimBLE, BLE alone uses ~150-200 KB, leaving only ~30-60 KB free. The NimBLE migration is essential for the full system.
