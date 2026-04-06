#pragma once

// =============================================================================
// DJ Booth Configuration
// =============================================================================
// Build-time defaults for the 26x32 WS2812B panel. Values can be overridden
// through PlatformIO build flags when a future slice needs different wiring.

// -----------------------------------------------------------------------------
// Panel Geometry
// -----------------------------------------------------------------------------

#ifndef PANEL_WIDTH
#define PANEL_WIDTH 26
#endif

#ifndef PANEL_HEIGHT
#define PANEL_HEIGHT 32
#endif

#define NUM_PIXELS (PANEL_WIDTH * PANEL_HEIGHT)

#if PANEL_WIDTH < 1 || PANEL_HEIGHT < 1
#error "PANEL_WIDTH and PANEL_HEIGHT must be at least 1"
#endif

// -----------------------------------------------------------------------------
// Strip Type
// -----------------------------------------------------------------------------
// WS2812B is electrically a 3-channel GRB strip. The shared LEDStrip library
// currently exposes WS2815B_RGB, which uses the same NeoPixelBus RGB method.

#define STRIP_TYPE_SK6812     0
#define STRIP_TYPE_WS2812_RGB 1

#ifndef STRIP_TYPE
#define STRIP_TYPE STRIP_TYPE_WS2812_RGB
#endif

// -----------------------------------------------------------------------------
// Matrix Wiring Pattern
// -----------------------------------------------------------------------------

#define WIRING_SERPENTINE_H  0
#define WIRING_PROGRESSIVE_H 1
#define WIRING_SERPENTINE_V  2

#ifndef WIRING_PATTERN
#define WIRING_PATTERN WIRING_SERPENTINE_H
#endif

// -----------------------------------------------------------------------------
// Pin Assignments
// -----------------------------------------------------------------------------

#if defined(ESP32) || defined(NATIVE_BUILD)
    #ifndef PIN_LED_DATA
    #define PIN_LED_DATA 4
    #endif

    #ifndef PIN_SWITCH
    #define PIN_SWITCH 13
    #endif

    #ifndef PIN_BRIGHTNESS
    #define PIN_BRIGHTNESS 32
    #endif

    #ifndef PIN_I2S_SCK
    #define PIN_I2S_SCK 26
    #endif

    #ifndef PIN_I2S_WS
    #define PIN_I2S_WS 25
    #endif

    #ifndef PIN_I2S_SD
    #define PIN_I2S_SD 33
    #endif

    #ifndef PIN_I2S_MCLK
    #define PIN_I2S_MCLK 27
    #endif
#endif

// -----------------------------------------------------------------------------
// Audio Configuration
// -----------------------------------------------------------------------------

#ifndef AUDIO_ENABLED
    #ifdef ESP32
        #define AUDIO_ENABLED 1
    #else
        #define AUDIO_ENABLED 0
    #endif
#endif

#define AUDIO_INPUT_MODE_MIC 0
#define AUDIO_INPUT_MODE_ADC 1

#ifndef AUDIO_INPUT_MODE
#define AUDIO_INPUT_MODE AUDIO_INPUT_MODE_MIC
#endif

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 44100
#endif

#ifndef AUDIO_FFT_SIZE
#define AUDIO_FFT_SIZE 1024
#endif

// Beat detection tuning
#define BEAT_THRESHOLD     1.5f
#define BEAT_COOLDOWN_MS   200
#define BPM_EMA_ALPHA      0.15f

// -----------------------------------------------------------------------------
// Runtime Behavior
// -----------------------------------------------------------------------------

#ifndef DEFAULT_POWER_ON
#define DEFAULT_POWER_ON 1
#endif

#ifndef DEFAULT_BRIGHTNESS
#define DEFAULT_BRIGHTNESS 160
#endif

#ifndef TARGET_FPS
#define TARGET_FPS 30
#endif

#define FRAME_MS (1000U / TARGET_FPS)

#ifndef CONTROL_QUEUE_LENGTH
#define CONTROL_QUEUE_LENGTH 16
#endif
