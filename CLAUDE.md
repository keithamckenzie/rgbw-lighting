# CLAUDE.md -- RGBW Lighting Project Guide

**Critical behavioral rules first. Technical reference in `docs/` directory.**

## Project Overview

PlatformIO monorepo for RGBW LED lighting controllers targeting ESP32, ESP8266, and Arduino (AVR) platforms. The system supports addressable LED strips (SK6812 RGBW and WS2815B RGB), PWM-driven MOSFET channels, I2S microphones for sound-reactive lighting, RCA audio input, WiFi/BLE wireless control, and physical switches.

---

# SECTION 1: CRITICAL BEHAVIORAL RULES

---

### Git Safety Rules

| FORBIDDEN COMMAND              | Why It's Forbidden                        |
| ------------------------------ | ----------------------------------------- |
| `git checkout <ref> -- <file>` | Overwrites working file, destroys changes |
| `git restore <file>`           | Discards uncommitted changes              |
| `git reset --hard`             | Destroys all uncommitted work             |
| `git clean -fd`                | Deletes untracked files permanently       |

**ONLY use READ-ONLY git commands:**

- `git show <ref>:<path>` — View old version
- `git diff <ref>..HEAD -- <file>` — Compare versions
- `git log -- <file>` — View history
- Read tool — View current version

### Commit Safety Rules

**NEVER commit until user validates:**

1. Do NOT run `git add` or `git commit` without user confirmation.
2. Report FACTUALLY what changed (file:line, specific edits).
3. Explain HOW user can verify it works.
4. WAIT for user to say "commit this" or similar.

**Example:**

```
Changes made:
- shared/lib/LEDStrip/src/led_strip.cpp:42-45 — Added bounds check
- shared/lib/RGBWCommon/src/rgbw.h:12 — Made white extraction configurable

To verify:
1. Run `pio run` to confirm it builds
2. Upload to device and test LED output
3. Check edge cases

Ready to commit when you've verified these work.
```

### Commit Message Format Rules

**ZERO AI Attribution — This is NOT Optional.**

FORBIDDEN in commit messages:

```
Generated with Claude Code
Co-Authored-By: Claude <noreply@anthropic.com>
```

**REQUIRED format — DETAILED multi-paragraph commits:**

```
<type>: <brief summary>

<Detailed body with sections>
- Specific files changed and what changed
- Technical implementation details
- Organized into sections when multiple areas affected

Why:
- Rationale for the changes
- Benefits and improvements
- Context for future maintainers
```

**Types:** `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`

**Requirements:**

- Detailed body explaining WHAT changed (files, specific changes)
- "Why:" section explaining rationale and benefits
- Sections for different types of changes when applicable
- NO emoji, NO AI attribution, NO "Generated with" text
- NO one-line commits (use detailed multi-paragraph format)
- NO sensitive information (passwords, API keys, WiFi credentials, internal URLs)

### Language Safety Rules

**NEVER claim success before user verifies on hardware.**

"It compiles" does not mean "it works." Embedded code must be tested on the physical device.

FORBIDDEN phrases:

- "This is fixed/done/working"
- "Problem solved", "All set!"
- "Why This Fixes It:"

REQUIRED instead:

- Report what changed (file:line)
- Explain HOW to verify (build command, upload, expected serial output, expected LED behavior)
- Wait for user confirmation

### Error Handling Rules

**When ANY tool or command returns an error:**

1. STOP immediately.
2. Tell user: "Error occurred: [exact message]"
3. Explain what went wrong.
4. Fix before proceeding.
5. NEVER ignore and continue.

**Avoid ineffective repetition:**

- Before retrying, evaluate: "Did it work last time?"
- If the same approach failed, ask user for a different strategy.
- Do not retry the same failing command more than 2 times without changing approach.

### Clangd / IDE Diagnostic Rules

**NEVER dismiss diagnostics as "false positives" without investigation.**

When clangd or any IDE diagnostic appears during editing:

1. **Investigate first.** Read the actual warning. Determine if it's a real code issue or a tooling issue.
2. **Fix code issues immediately.** Common real issues that look like false positives:
   - `%lu` / `%u` format mismatches with `uint32_t` — use `PRIu32` from `<inttypes.h>` (portable across host and target).
   - Signed/unsigned comparison warnings — fix the types.
   - Unused variable/parameter warnings — fix or `(void)var`.
3. **Fix tooling issues at the source.** If clangd can't resolve Arduino/ESP-IDF symbols:
   - Check `.clangd` `Remove:` list for gcc/xtensa flags clang doesn't understand.
   - Check `compile_commands.json` exists and is current (`pio run` regenerates it).
   - For full cross-compiler support, editor must pass `--query-driver=**/xtensa-*-elf-*` to clangd (see `.clangd` file comments).
   - Do NOT just ignore and move on.
4. **Report what you found.** Tell the user: "Diagnostic X is caused by Y. Fixed by Z." or "Diagnostic X is a known clangd limitation with cross-compilation — here's the `.clangd` fix."

**FORBIDDEN:** Saying "these are false positives from clangd" and moving on without fixing anything.

**Portable printf for fixed-width types (ESP32 code):**

| Type | Format macro | Header |
|------|-------------|--------|
| `uint8_t` | `PRIu8` | `<inttypes.h>` |
| `uint16_t` | `PRIu16` | `<inttypes.h>` |
| `uint32_t` | `PRIu32` | `<inttypes.h>` |
| `int32_t` | `PRId32` | `<inttypes.h>` |
| `size_t` | `%zu` | (built-in) |

Example: `ESP_LOGI(TAG, "Rate: %" PRIu32 " Hz", sampleRate);`

### Build Verification Workflow

**Run build commands once, capture output, check exit code:**

```bash
pio run -e esp32 2>&1 > /tmp/pio.log; EC=$?; echo "Exit code: $EC"; head -80 /tmp/pio.log; echo "..."; tail -80 /tmp/pio.log
```

**Decision tree:**

| Exit Code | Meaning | What To Do |
|-----------|---------|------------|
| **0**     | Passed  | **STOP.** Move on. Do NOT run additional verification commands. |
| **Non-zero** | Failed | Search log for errors: `rg -i "error" /tmp/pio.log`. Fix them. |

The same pattern applies to tests:

```bash
pio test -e native 2>&1 > /tmp/test.log; EC=$?; echo "Exit code: $EC"; head -80 /tmp/test.log; echo "..."; tail -80 /tmp/test.log
```

**Do NOT** run `pio run` again after it already passed with exit code 0 just to "double check." Exit 0 means it passed.

### FORBIDDEN / REQUIRED Quick Reference

| Category         | FORBIDDEN                                            | REQUIRED                                             |
| ---------------- | ---------------------------------------------------- | ---------------------------------------------------- |
| **Commits**      | Commit without user validation                       | Report changes + wait for approval                   |
| **Commits**      | AI attribution / emoji / one-liners                  | Detailed multi-paragraph format (see above)          |
| **Commits**      | Sensitive info in commit messages                    | Redact passwords, keys, WiFi credentials             |
| **Language**     | Claim "done" / "fixed" / "working"                   | Report what changed + how to verify                  |
| **Git**          | `checkout` / `restore` / `reset --hard`              | `git show` / `git diff` (read-only)                  |
| **Builds**       | Re-running `pio run` after exit code 0               | Build once, check exit code, move on                 |
| **Builds**       | `cmd \| head` then `cmd \| tail` (runs cmd twice)   | Capture to file, check exit code, then inspect       |
| **Search**       | `grep` / `find` bash commands                        | Grep tool / Glob tool (or Task tool for exploration) |
| **File Paths**   | Guess or assume paths for commands                   | Verify with `ls` first, then run command             |
| **Docs**         | Multiple docs on same topic                          | ONE doc, update it                                   |
| **Upload**       | Upload to device without asking                      | ASK user first                                       |
| **Diagnostics**  | Dismiss as "false positives" without investigation   | Investigate, fix code or tooling, report findings    |

---

# SECTION 2: QUICK REFERENCE

---

### Essential Commands

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

**Pre-commit build check pattern:**

```bash
pio run -e esp32 2>&1 > /tmp/pio.log; EC=$?; echo "Exit code: $EC"; head -80 /tmp/pio.log; echo "..."; tail -80 /tmp/pio.log
```

- Exit code 0 = Passed. Move on. Do NOT run additional commands.
- Exit code non-zero = `rg -i "error" /tmp/pio.log` to find errors.

### Session Workflow

**Session start:** `source .venv/bin/activate` -> `git status` -> review working state

**Pre-commit:** User validates changes on hardware -> `pio run` -> exit 0 -> commit

**Upload:** `pio run -t upload -e esp32` -> `pio device monitor` (115200 baud) -> verify on device

---

# SECTION 3: TECHNICAL REFERENCE

Full technical documentation is in the `docs/` directory. This section provides essential structural context and links to detailed docs.

---

## Repository Structure

```
rgbw-lighting/
  apps/                         # Standalone PlatformIO projects
    example-rgbw/               # Template app (copy to create new apps)
      platformio.ini
      src/main.cpp
    led-panel/                  # 24x36 RGBW panel (ESP32 + ESP8266)
      platformio.ini
      src/
      test/
  shared/lib/                   # Shared libraries (linked via lib_extra_dirs)
    RGBWCommon/                 # Core RGBW/HSV types, color math, utilities
    LEDStrip/                   # Addressable strip driver (SK6812 RGBW, WS2815B RGB)
    LEDPWM/                     # PWM channel driver (MOSFET-controlled RGBW)
    Connectivity/               # WiFi and BLE managers (ESP32 only)
  docs/                         # Technical reference documentation
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

## Build System Summary

**Framework:** Arduino (via PlatformIO)
**Shared library path:** `lib_extra_dirs = ../../shared/lib` in each app's `platformio.ini`
**PlatformIO venv:** `source .venv/bin/activate` or `.venv/bin/pio run`

| Environment    | Platform      | Board       | C++ Standard |
|---------------|---------------|-------------|-------------|
| `esp32`       | espressif32   | esp32dev    | gnu++17     |
| `esp8266`     | espressif8266 | nodemcuv2   | gnu++11     |
| `arduino-uno` | atmelavr      | uno         | gnu++11     |

-> Full build configuration, flags, and test setup: **[docs/build-and-tooling.md](docs/build-and-tooling.md)**

## Coding Standards Summary

- **ESP32:** C++17. **AVR:** C++11. **Shared libraries:** C++11 compatible (guard C++17 with `#if __cplusplus >= 201703L`).
- **Naming:** Files `snake_case`, Classes `PascalCase`, methods `camelCase`, privates `_camelCase`, constants `UPPER_SNAKE_CASE`.
- **No Arduino `String` class** (heap fragmentation). Use `char[]` + `snprintf()`.
- **No exceptions.** Use `enum class` error codes.
- **Pre-allocate all buffers** in `setup()` or constructors. No runtime allocation in loops.
- **Logging:** ESP_LOG macros on ESP32, Serial on AVR. Use `CORE_DEBUG_LEVEL` build flag.

-> Full standards, patterns, memory management, platform abstraction: **[docs/coding-standards.md](docs/coding-standards.md)**

## LED Control Summary

- **Color model:** RGBW (4 bytes) and HSV (hue 0-360, sat/val 0-255). `hsvToRGBW()` extracts white from min(RGB).
- **Addressable driver:** NeoPixelBus via template-based `LEDStrip<StripType>` (e.g. `LEDStrip<StripType::SK6812_RGBW> strip(pin, numLeds);`).
- **SK6812 RGBW:** 4-channel, 5V, 32-bit, 80 mA/LED, dedicated white die. Reset >= 80 us.
- **WS2815B RGB:** 3-channel, 12V, 24-bit, 30 mA/LED, backup data line (BIN). Reset >= 280 us.
- **Both** use 800 kHz NRZ protocol. White channel folded into RGB automatically for WS2815B.
- **24V non-addressable (COB/SMD):** No data line; controlled purely by `LEDPWM` via MOSFETs.
- **PWM (MOSFET):** Use `LEDPWM` library. Default 12-bit at 19.5 kHz for smooth dimming.
- **Gamma correction:** Apply gamma 2.2 LUT for perceptually uniform brightness.

-> Full LED specs, timing, level shifting, signal conditioning, MOSFET wiring, gamma: **[docs/led-control.md](docs/led-control.md)**

## Power and Wiring Summary

- **Safety:** Fuse main PSU output and per-zone runs. AC/DC separation — mains confined to PSU only.
- **SK6812:** 80 mA/LED @ 5V. Power injection every 1-2m.
- **WS2815B:** 30 mA/LED @ 12V. Runs 5-10m without injection.
- **Wire gauge:** 18 AWG default. Never thinner than 22 AWG for power. Future-proof runs with extra conductors.
- **Brownout:** Separate 3.3V regulation for ESP32. Bulk caps. Software soft-start.
- **MOSFET:** Low-side N-channel (IRLB8721). 100R gate resistor, 10k pull-down.
- **Multi-zone:** Home-run wiring, centralized MOSFETs, one per zone per channel, common ground across PSUs. Check PSU isolation before bonding negatives.
- **Buck converter:** 24V/12V -> 5V (dev board) or 3.3V (bare module) for ESP32 power.
- **Line-voltage fixtures:** Not GPIO-direct; use smart switches, triac hardware (zero-cross + optotriac), 0-10V dimmers (check sink vs source), or DALI gateways.

-> Full power budget, safety/fusing, injection, connectors, wire gauge, brownout, multi-zone, buck converters, line-voltage fixtures, switches: **[docs/power-and-wiring.md](docs/power-and-wiring.md)**

## Audio and Sound-Reactive Summary

- **Inputs:** I2S microphone (INMP441/SPH0645/ICS-43432; ICS-43434 is EOL), RCA via ADC1 (GPIO 32-39), PCM1808 I2S ADC, I2S ADC DMA mode.
- **FFT:** Use ESP-DSP on ESP32 (10-17x faster than arduinoFFT). 1024-pt at 44100 Hz.
- **Band mapping:** Bass (20-250 Hz) -> R, Mids (250-2k Hz) -> G, Highs (2k-16k Hz) -> B.
- **Beat detection:** Energy ratio in bass band vs sliding window average.
- **BPM tracking:** Derive BPM from beat intervals, exponential moving average for stability, beat prediction.
- **Beat quantization:** Snap RF remote triggers to nearest beat boundary for musical synchronization.

-> Full audio setup, DMA config, ESP-DSP API, beat detection, band mapping: **[docs/audio-reactive.md](docs/audio-reactive.md)**

## ESP32 Internals Summary

- **FreeRTOS:** Pin WiFi/BLE to Core 0, app tasks to Core 1. Use `vTaskDelayUntil()` for periodic tasks.
- **WiFi:** Event-driven callbacks, exponential backoff reconnection.
- **BLE:** GATT service (UUID 0xFF00), color (0xFF01), brightness (0xFF02).
- **433 MHz RF:** RXB6 receiver + rc-switch library. Sonoff RM433R2 remote. EV1527 OTP fixed codes, learnable by receiver; not rolling/encrypted.
- **Storage:** NVS (`Preferences.h`) for settings, LittleFS for files/web UI.
- **I2C:** External pull-ups required (4.7k @ 100 kHz, 2.2k @ 400 kHz). Mutex for thread safety.
- **Partition table:** OTA requires dual app slots (see partitions.csv example in docs).

-> Full FreeRTOS patterns, WiFi/BLE/mDNS, 433 MHz RF, NVS/LittleFS, I2C, OTA, RAM budget: **[docs/esp32-internals.md](docs/esp32-internals.md)**

## Pin Reference Summary

- **GPIO 6-11:** SPI flash. Never use.
- **GPIO 34-39:** Input-only (no pull-ups). Use for ADC.
- **ADC2 (GPIO 0, 2, 4, 12-15, 25-27):** Unavailable when WiFi active. Use ADC1.
- **GPIO 12:** Most dangerous strapping pin -- HIGH at boot prevents normal boot.
- **Safe GPIOs:** 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 (16/17 unavailable on WROVER/PICO).

-> Full pin map, boot strapping details, safe GPIO table, module variants: **[docs/pin-reference.md](docs/pin-reference.md)**

## Project Roadmap Summary

- **Planned upgrades:** NimBLE-Arduino, ESPAsyncWebServer. ~~NeoPixelBus (done)~~. ~~ESP-DSP (done)~~. ~~I2S ADC DMA (done)~~.
- **Planned libraries:** ~~AudioInput (done)~~, InputManager, PowerManager.
- **Known fixes:** ~~LEDPWM 12-bit upgrade (done)~~, scaleBrightness fix, configurable white extraction, WiFiManager async migration.

-> Full upgrade details, migration guides, known improvements: **[docs/project-roadmap.md](docs/project-roadmap.md)**

## Documentation Index

| Document | Contents |
|----------|----------|
| [docs/led-control.md](docs/led-control.md) | Color model, SK6812/WS2815B specs and timing, 24V non-addressable strips, level shifting, signal conditioning, RMT channels, PWM/MOSFET, gamma correction |
| [docs/power-and-wiring.md](docs/power-and-wiring.md) | Power budget, injection patterns, wire gauge, connectors, brownout, MOSFET wiring, multi-zone architecture, buck converters, switches/buttons |
| [docs/audio-reactive.md](docs/audio-reactive.md) | I2S mics (INMP441/SPH0645/ICS-43432), RCA input, PCM1808 I2S ADC, ADC calibration, DMA sampling, ESP-DSP FFT, band mapping, beat detection, BPM tracking, beat quantization |
| [docs/esp32-internals.md](docs/esp32-internals.md) | FreeRTOS tasks/queues/ISR, WiFi/BLE/mDNS, 433 MHz RF remote, NVS/LittleFS/partitions, I2C, OTA, RAM budget |
| [docs/build-and-tooling.md](docs/build-and-tooling.md) | PlatformIO config, build flags, target platforms, testing with Unity, filesystem, CI tips |
| [docs/coding-standards.md](docs/coding-standards.md) | C++ standards, naming conventions, strings, error handling, memory management, platform abstraction, logging |
| [docs/pin-reference.md](docs/pin-reference.md) | GPIO safety, boot strapping pins, ADC channels, safe pin assignments, module variants |
| [docs/project-roadmap.md](docs/project-roadmap.md) | Library upgrades (NeoPixelBus, NimBLE, ESPAsync), planned libraries, known improvement notes, git conventions |
