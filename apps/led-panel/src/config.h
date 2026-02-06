#pragma once

// =============================================================================
// LED Panel Configuration
// =============================================================================
// This file contains all configurable settings for the LED panel firmware.
// Values can be overridden via build flags (-D flags in platformio.ini).

// -----------------------------------------------------------------------------
// Panel Dimensions
// -----------------------------------------------------------------------------

// Panel width in LEDs (horizontal direction)
// The number of LEDs in each row of your panel
#ifndef PANEL_WIDTH
#define PANEL_WIDTH 36
#endif

// Panel height in LEDs (vertical direction)
// The number of rows in your panel
#ifndef PANEL_HEIGHT
#define PANEL_HEIGHT 24
#endif

// Total pixel count (auto-calculated)
#define NUM_PIXELS (PANEL_WIDTH * PANEL_HEIGHT)

#if PANEL_WIDTH < 1 || PANEL_HEIGHT < 1
#error "PANEL_WIDTH and PANEL_HEIGHT must be at least 1"
#endif

// -----------------------------------------------------------------------------
// LED Strip Type
// -----------------------------------------------------------------------------
// Choose the type of addressable LED strip you're using:
//   0 = SK6812 RGBW (default) - 4-channel with dedicated white LED
//       5V power, 80mA per LED at full brightness
//   1 = WS2815B RGB - 3-channel with backup data line
//       12V power, 30mA per LED at full brightness
#ifndef STRIP_TYPE
#define STRIP_TYPE 0
#endif

// -----------------------------------------------------------------------------
// Wiring Pattern
// -----------------------------------------------------------------------------
// How the LED strip is physically wired on your panel:
//   0 = Serpentine Horizontal (default) - Rows alternate direction (zigzag)
//       First row left-to-right, second row right-to-left, etc.
//   1 = Progressive Horizontal - All rows run the same direction
//       Every row starts from the same side
//   2 = Serpentine Vertical - Columns alternate direction
//       First column top-to-bottom, second column bottom-to-top, etc.
#ifndef WIRING_PATTERN
#define WIRING_PATTERN 0
#endif

// Wiring pattern constants (do not modify)
#define WIRING_SERPENTINE_H  0
#define WIRING_PROGRESSIVE_H 1
#define WIRING_SERPENTINE_V  2

// -----------------------------------------------------------------------------
// Pin Assignments (Platform-Specific)
// -----------------------------------------------------------------------------

#ifdef ESP32
    // GPIO pin connected to LED strip data line
    // Recommended: Use GPIO 4 for best signal integrity
    #ifndef PIN_LED_DATA
    #define PIN_LED_DATA 4
    #endif

    // GPIO pin for main control button/switch
    // Supports short press and long press actions
    #ifndef PIN_SWITCH
    #define PIN_SWITCH 13
    #endif

    // ADC pin for brightness potentiometer
    // Must use ADC1 pins (GPIO 32-39) to work with WiFi enabled
    #ifndef PIN_BRIGHTNESS
    #define PIN_BRIGHTNESS 32
    #endif

    // I2S audio input pins (for microphone or ADC)
    // SCK (Bit Clock) - Clock signal for I2S
    #ifndef PIN_I2S_SCK
    #define PIN_I2S_SCK 26
    #endif

    // WS (Word Select / LRCLK) - Left/right channel select
    #ifndef PIN_I2S_WS
    #define PIN_I2S_WS 25
    #endif

    // SD (Serial Data) - Audio data input from microphone
    #ifndef PIN_I2S_SD
    #define PIN_I2S_SD 33
    #endif

    // MCLK (Master Clock) - Required by some I2S devices
    // GPIO 0 is hardware-fixed as MCLK output for I2S_NUM_0 on ESP32
    #ifndef PIN_I2S_MCLK
    #define PIN_I2S_MCLK 0
    #endif

#elif defined(ESP8266)
    // ESP8266 uses UART1 for LED output (always GPIO 2/D4)
    // The pin value is ignored by NeoPixelBus but set for clarity
    #ifndef PIN_LED_DATA
    #define PIN_LED_DATA 2
    #endif

    // GPIO pin for main control button/switch
    #ifndef PIN_SWITCH
    #define PIN_SWITCH 14       // D5
    #endif

    // ADC pin for brightness potentiometer
    // ESP8266 has only one ADC pin (A0)
    #ifndef PIN_BRIGHTNESS
    #define PIN_BRIGHTNESS A0
    #endif

    // Note: ESP8266 does not support I2S microphone input
    // Audio-reactive features are disabled on this platform
#endif

// -----------------------------------------------------------------------------
// Audio Configuration (ESP32 only)
// -----------------------------------------------------------------------------

// Enable or disable audio-reactive features
// Set to 0 to disable audio input entirely
#ifndef AUDIO_ENABLED
    #ifdef ESP32
        #define AUDIO_ENABLED 1
    #else
        #define AUDIO_ENABLED 0
    #endif
#endif

// Audio input mode - select your audio hardware:
//   0 = I2S Microphone (default) - Digital mics like INMP441, SPH0645, ICS-43432
//       Connects directly to I2S pins for high-quality audio input
//   1 = I2S ADC - External ADC like PCM1808 for line-level input
//       Use this for RCA or 3.5mm audio input via an I2S ADC module
#ifndef AUDIO_INPUT_MODE
#define AUDIO_INPUT_MODE 0
#endif

// Beat detection threshold - energy ratio required to detect a beat
// Higher values = less sensitive (fewer false positives)
// Lower values = more sensitive (may trigger on quiet sounds)
// Recommended range: 1.2 to 2.0
#define BEAT_THRESHOLD     1.5f

// Minimum time between detected beats (milliseconds)
// Prevents rapid-fire beat detection on sustained bass notes
#define BEAT_COOLDOWN_MS   200

// BPM tracking smoothing factor (exponential moving average alpha)
// Lower values = smoother but slower response to tempo changes
// Higher values = faster response but more jitter
// Recommended range: 0.1 to 0.25
#define BPM_EMA_ALPHA      0.15f

// -----------------------------------------------------------------------------
// Timing and Performance
// -----------------------------------------------------------------------------

// Target frame rate depends on LED strip type
// SK6812: Limited by 40us per LED data time (864 LEDs @ 25fps)
// WS2815B: Faster data rate allows higher frame rates
#if STRIP_TYPE == 0
    #define TARGET_FPS 25       // SK6812: 864 x 40us = 34.5ms data time
#else
    #define TARGET_FPS 30       // WS2815B: faster data rate
#endif

// Frame duration in milliseconds (auto-calculated)
#define FRAME_MS (1000 / TARGET_FPS)

// -----------------------------------------------------------------------------
// Input Tuning
// -----------------------------------------------------------------------------

// Long press duration in milliseconds
// How long a button must be held to trigger a long-press action
#define LONG_PRESS_MS     800

// ADC reading interval (milliseconds)
// How often to sample the brightness potentiometer
#define ADC_READ_INTERVAL 50

// ADC moving average window size
// Higher values = smoother readings but slower response
#define ADC_SAMPLES       8
