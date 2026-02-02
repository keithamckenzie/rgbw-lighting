#include "input.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

// --- Button state ---
static bool s_lastButtonState = false;
static uint32_t s_pressStartMs = 0;
static bool s_longPressHandled = false;

// --- ADC moving average ---
static uint16_t s_adcSamples[ADC_SAMPLES];
static uint8_t s_adcIndex = 0;
static uint32_t s_lastAdcReadMs = 0;

void inputBegin(uint8_t defaultBrightness) {
#ifndef NATIVE_BUILD
    #ifdef ESP32
        pinMode(PIN_SWITCH, INPUT_PULLUP);
    #elif defined(ESP8266)
        pinMode(PIN_SWITCH, INPUT_PULLUP);
    #endif
#endif

    for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
        s_adcSamples[i] = defaultBrightness;
    }
}

// --- Read brightness pot with moving average ---
static uint8_t readBrightness(uint32_t nowMs) {
    if (nowMs - s_lastAdcReadMs < ADC_READ_INTERVAL) {
        // Return current average without new sample
    } else {
        s_lastAdcReadMs = nowMs;
#ifndef NATIVE_BUILD
        uint16_t raw = analogRead(PIN_BRIGHTNESS);
    #ifdef ESP32
        // ESP32 ADC is 12-bit (0-4095)
        s_adcSamples[s_adcIndex] = raw >> 4;  // scale to 0-255
    #else
        // ESP8266 ADC is 10-bit (0-1023)
        s_adcSamples[s_adcIndex] = raw >> 2;  // scale to 0-255
    #endif
        s_adcIndex = (s_adcIndex + 1) % ADC_SAMPLES;
#endif
    }

    uint16_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
        sum += s_adcSamples[i];
    }
    return (uint8_t)(sum / ADC_SAMPLES);
}

void inputUpdate(InputState& state, uint32_t nowMs) {
    state.modeChanged = false;

#ifndef NATIVE_BUILD
    // --- Button handling ---
    bool pressed = (digitalRead(PIN_SWITCH) == LOW);  // active low w/ pullup

    if (pressed && !s_lastButtonState) {
        // Button just pressed
        s_pressStartMs = nowMs;
        s_longPressHandled = false;
    }

    if (pressed && !s_longPressHandled) {
        // Check for long press while held
        if (nowMs - s_pressStartMs >= LONG_PRESS_MS) {
            // Long press: toggle power
            state.powerOn = !state.powerOn;
            s_longPressHandled = true;
        }
    }

    if (!pressed && s_lastButtonState) {
        // Button just released
        if (!s_longPressHandled) {
            // Short press: next mode (or power on if off)
            if (!state.powerOn) {
                state.powerOn = true;
            } else {
                state.currentMode = nextMode(state.currentMode);
                state.modeChanged = true;
            }
        }
    }

    s_lastButtonState = pressed;
#endif

    // --- Brightness from pot ---
    state.brightness = readBrightness(nowMs);
}
