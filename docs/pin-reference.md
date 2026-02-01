# ESP32 Pin Reference

Complete GPIO reference for the RGBW lighting project, including pin safety rules, boot strapping behavior, and recommended assignments.

## Pin Categories Overview

```
ESP32 GPIO Map (34 GPIOs: 0-19, 21-23, 25-27, 32-39)
Note: GPIO 20, 24, 28-31 do not exist on ESP32

[FLASH - NEVER USE]  GPIO 6, 7, 8, 9, 10, 11
[INPUT ONLY]         GPIO 34, 35, 36 (VP), 37, 38, 39 (VN)
[ADC2 - WiFi blocks] GPIO 0, 2, 4, 12, 13, 14, 15, 25, 26, 27
[ADC1 - Always OK]   GPIO 32, 33, 34, 35, 36, 39
[BOOT STRAPPING]     GPIO 0, 2, 5, 12, 15
[SAFE FOR OUTPUT]    GPIO 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33
```

## Pin Safety Rules

### Never Use (GPIO 6-11)

These are connected to the integrated SPI flash memory. Using them **will crash the ESP32**:

| GPIO | Flash Function |
|------|---------------|
| 6 | SCK (SPI clock) |
| 7 | SDO / SD0 (data 0) |
| 8 | SDI / SD1 (data 1) |
| 9 | SHD / SD2 (data 2) |
| 10 | SWP / SD3 (data 3) |
| 11 | CSC (chip select) |

### Input-Only Pins (GPIO 34-39)

These GPIOs have **no output driver** and **no internal pull-up/pull-down** resistors:

| GPIO | Common Label | Best Use |
|------|-------------|----------|
| 34 | ADC1_CH6 | Analog input (audio, sensors) |
| 35 | ADC1_CH7 | Analog input |
| 36 | VP / SENSOR_VP | Analog input (pre-amp input on some boards) |
| 37 | — | ADC1_CH1 (often not exposed on dev boards) |
| 38 | — | ADC1_CH2 (often not exposed on dev boards) |
| 39 | VN / SENSOR_VN | Analog input |

Use for: ADC analog input, digital input with external pull-up/pull-down resistors.
Cannot use for: Any output (LEDs, MOSFETs, SPI, I2C, etc.)

### ADC2 Pins (WiFi Conflict)

**ADC2 is unavailable when WiFi is active.** Reading ADC2 while WiFi is connected returns errors or garbage data.

ADC2 pins: GPIO 0, 2, 4, 12, 13, 14, 15, 25, 26, 27

**For audio input, always use ADC1 (GPIO 32-39).**

ADC1 pins and channels:

| GPIO | ADC1 Channel |
|------|-------------|
| 32 | CH4 |
| 33 | CH5 |
| 34 | CH6 |
| 35 | CH7 |
| 36 | CH0 |
| 37 | CH1 |
| 38 | CH2 |
| 39 | CH3 |

## Boot Strapping Pins

These GPIOs are sampled at power-on reset to determine boot mode. External loads can prevent normal booting.

| GPIO | Function at Boot | Required State | Risk |
|------|-----------------|----------------|------|
| **0**  | Boot mode select | **HIGH** = normal boot; LOW = download mode | Button/load pulling LOW prevents app from running |
| **2**  | Boot mode (with GPIO0) | Any value for SPI boot; LOW for download mode | Generally safe for output use |
| **5**  | SDIO slave timing | Default pull-up; affects SDIO timing only | Low risk for LED projects (not required for boot) |
| **12** | Flash voltage select | **LOW** = 3.3V flash (correct); HIGH = 1.8V flash | **Dangerous: HIGH at boot prevents normal boot** |
| **15** | Boot log output | HIGH = enable UART boot messages | LOW suppresses boot log but otherwise safe |

### GPIO 12: The Most Dangerous Strapping Pin

If GPIO 12 is pulled HIGH during power-on, it selects 1.8V flash voltage. The ESP32 **fails to boot** (recoverable by reflashing or burning the efuse, but requires intervention).

**Safe usage of GPIO 12:**
- A MOSFET gate with a 10k pull-down resistor is safe (keeps LOW at boot)
- Any external pull-up will prevent boot
- **Best practice: avoid GPIO 12 entirely for MOSFET gates**

**If GPIO 12 must go HIGH during operation**, permanently fix flash voltage by burning the efuse (one-time, irreversible):
```bash
espefuse.py --port /dev/ttyUSB0 set_flash_voltage 3.3V
```
This burns the `XPD_SDIO_FORCE` and `XPD_SDIO_TIEH` efuses so GPIO 12 state is ignored at boot.

### GPIO 0: Boot Button Pin

Most dev boards connect GPIO 0 to a "BOOT" button (pulled LOW when pressed). If your circuit holds GPIO 0 LOW at power-on, the ESP32 enters download mode instead of running the application.

**Safe as output** after boot (e.g., LED indicator), but avoid heavy external loads that could pull it LOW during reset.

### GPIO 2: Built-in LED on Many Boards

Many ESP32 dev boards connect GPIO 2 to a blue LED. It must be LOW (or floating) during download mode boot. Safe for general output use, but be aware the onboard LED will flicker during boot and data writes if used for other purposes.

### GPIO 15: Boot Log Suppression

If GPIO 15 is LOW at boot, UART boot messages are suppressed. This is sometimes useful for clean serial output, but can make debugging boot issues harder. Generally safe for any use.

## Safe GPIOs for MOSFET Gates and LED Data

**Recommended:** 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33

| GPIO | Notes |
|------|-------|
| 4 | Safe, commonly used for LED data or MOSFET |
| 13 | Strapping-adjacent but safe for output |
| 14 | JTAG TMS pin — may output a brief signal during boot, acceptable for LEDs |
| 16 | **WROVER/PICO: Used for PSRAM** — check module |
| 17 | **WROVER/PICO: Used for PSRAM** — check module |
| 18 | SPI SCK default, safe if SPI not used |
| 19 | SPI MISO default, safe if SPI not used |
| 21 | I2C SDA default, safe if I2C not on this pin |
| 22 | I2C SCL default, safe if I2C not on this pin |
| 23 | SPI MOSI default, safe if SPI not used |
| 25 | DAC1 output capable |
| 26 | DAC2 output capable |
| 27 | General purpose |
| 32 | ADC1, also output capable |
| 33 | ADC1, also output capable |

### Module-Specific Restrictions

| Module | Unavailable GPIOs | Reason |
|--------|------------------|--------|
| ESP32-WROVER | 16, 17 | Connected to PSRAM |
| ESP32-PICO-D4 | 16, 17 | In-package flash |
| ESP32-PICO-V3 | 16, 17 | In-package flash/PSRAM |
| ESP32-S2/S3/C3 | Varies | Different GPIO maps entirely — check datasheet |

**Always check your specific module's datasheet** before assigning pins.

## Recommended Pin Assignments

**Document all pin assignments in a `pins.h` header per app with a connection table.** This prevents accidental conflicts and makes the hardware setup clear to anyone reading the code.

### Typical RGBW Lighting Setup

```cpp
// pins.h - Document ALL pin assignments per app
#pragma once

// === LED Strip Data ===
constexpr uint8_t PIN_LED_STRIP_1 = 4;    // SK6812 RGBW strip, 30 LEDs
constexpr uint8_t PIN_LED_STRIP_2 = 18;   // WS2815B RGB strip, 60 LEDs

// === PWM MOSFET Channels ===
constexpr uint8_t PIN_MOSFET_R = 25;
constexpr uint8_t PIN_MOSFET_G = 26;
constexpr uint8_t PIN_MOSFET_B = 27;
constexpr uint8_t PIN_MOSFET_W = 14;

// === Audio Input ===
constexpr uint8_t PIN_AUDIO_ADC = 32;     // ADC1_CH4, RCA line-in

// === I2S Microphone ===
constexpr uint8_t PIN_I2S_BCK  = 19;
constexpr uint8_t PIN_I2S_WS   = 23;      // aka LRCK
constexpr uint8_t PIN_I2S_DIN  = 33;      // Data in from mic

// === I2C Bus ===
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;

// === Physical Controls ===
constexpr uint8_t PIN_BUTTON_MODE = 13;   // Mode cycle button
constexpr uint8_t PIN_ENCODER_A  = 16;    // Rotary encoder (check module!)
constexpr uint8_t PIN_ENCODER_B  = 17;    // Rotary encoder (check module!)

// === Connection Table ===
// Pin | Function     | Direction | Pull   | Notes
// ----|-------------|-----------|--------|------
//  4  | LED Strip 1 | OUT       | —      | 74HCT125 level shifted
// 18  | LED Strip 2 | OUT       | —      | 74HCT125 level shifted
// 25  | MOSFET Red  | OUT       | 10k DN | IRLB8721 gate
// 26  | MOSFET Green| OUT       | 10k DN | IRLB8721 gate
// 27  | MOSFET Blue | OUT       | 10k DN | IRLB8721 gate
// 14  | MOSFET White| OUT       | 10k DN | IRLB8721 gate
// 32  | Audio ADC   | IN        | —      | Biased to 1.65V
// 21  | I2C SDA     | I/O       | 4.7k UP| External pull-up
// 22  | I2C SCL     | OUT       | 4.7k UP| External pull-up
// 13  | Mode Button | IN        | INT UP | INPUT_PULLUP
```

## ESP32 Variant GPIO Comparison

For reference when targeting other ESP32 variants:

| Feature | ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 |
|---------|-------|----------|----------|----------|
| Total GPIOs | 34 usable | 43 | 45 | 22 |
| ADC channels | 18 (ADC1: 8, ADC2: 10) | 20 | 20 | 6 |
| Touch pins | 10 | 14 | 14 | 0 |
| DAC outputs | 2 (GPIO 25, 26) | 2 | 0 | 0 |
| UART | 3 | 2 | 3 | 2 |
| I2C | 2 | 2 | 2 | 1 |
| SPI | 3 (1 for flash) | 3 (1 for flash) | 3 (1 for flash) | 3 (1 for flash) |
| CPU | Dual Xtensa LX6 | Single Xtensa LX7 | Dual Xtensa LX7 | Single RISC-V |
