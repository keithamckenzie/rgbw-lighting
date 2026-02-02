#pragma once

// --- Panel dimensions ---
#ifndef PANEL_WIDTH
#define PANEL_WIDTH 36
#endif

#ifndef PANEL_HEIGHT
#define PANEL_HEIGHT 24
#endif

#define NUM_PIXELS (PANEL_WIDTH * PANEL_HEIGHT)

#if PANEL_WIDTH < 1 || PANEL_HEIGHT < 1
#error "PANEL_WIDTH and PANEL_HEIGHT must be at least 1"
#endif

// --- Strip type ---
// 0 = SK6812_RGBW (default), 1 = WS2815B_RGB
#ifndef STRIP_TYPE
#define STRIP_TYPE 0
#endif

// --- Wiring pattern ---
// 0 = serpentine horizontal (default), 1 = progressive horizontal, 2 = serpentine vertical
#ifndef WIRING_PATTERN
#define WIRING_PATTERN 0
#endif

#define WIRING_SERPENTINE_H  0
#define WIRING_PROGRESSIVE_H 1
#define WIRING_SERPENTINE_V  2

// --- Platform-conditional pin assignments ---
#ifdef ESP32
    #ifndef PIN_LED_DATA
    #define PIN_LED_DATA 4
    #endif
    #ifndef PIN_SWITCH
    #define PIN_SWITCH 13
    #endif
    #ifndef PIN_BRIGHTNESS
    #define PIN_BRIGHTNESS 32   // ADC1
    #endif
    // I2S audio pins
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
    #define PIN_I2S_MCLK 0   // GPIO 0 is hardware-fixed MCLK for I2S_NUM_0
    #endif
#elif defined(ESP8266)
    // ESP8266 UART1 always outputs on GPIO2/D4; pin is ignored by NeoPixelBus
    #ifndef PIN_LED_DATA
    #define PIN_LED_DATA 2
    #endif
    #ifndef PIN_SWITCH
    #define PIN_SWITCH 14       // D5
    #endif
    #ifndef PIN_BRIGHTNESS
    #define PIN_BRIGHTNESS A0
    #endif
    // No mic pin on ESP8266 (single ADC used for brightness)
#endif

// --- Audio enable ---
#ifndef AUDIO_ENABLED
    #ifdef ESP32
        #define AUDIO_ENABLED 1
    #else
        #define AUDIO_ENABLED 0
    #endif
#endif

// --- Frame rate ---
#if STRIP_TYPE == 0
    #define TARGET_FPS 25       // SK6812: 864 x 40us = 34.5ms data time
#else
    #define TARGET_FPS 30       // WS2815B: faster data rate
#endif

#define FRAME_MS (1000 / TARGET_FPS)

// --- Input tuning ---
#define LONG_PRESS_MS     800
#define ADC_READ_INTERVAL 50    // ms between brightness pot reads
#define ADC_SAMPLES       8     // moving average window

// --- Audio tuning (ESP32 only) ---
// 0 = I2S mic (ICS-43434, default), 1 = I2S ADC (PCM1808)
#ifndef AUDIO_INPUT_MODE
#define AUDIO_INPUT_MODE 0
#endif

#define BEAT_THRESHOLD     1.5f // energy vs average ratio
#define BEAT_COOLDOWN_MS   200  // minimum ms between beats
#define BPM_EMA_ALPHA      0.15f
