#include "led_pwm.h"
#include <Arduino.h>

// LEDC channel assignments for ESP32
#define LEDC_CHANNEL_R 0
#define LEDC_CHANNEL_G 1
#define LEDC_CHANNEL_B 2
#define LEDC_CHANNEL_W 3

LEDPWM::LEDPWM(const PWMPins& pins, uint32_t frequency, uint8_t resolution)
    : _pins(pins), _frequency(frequency), _resolution(resolution), _brightness(255) {
    _currentColor = RGBW(0, 0, 0, 0);
}

void LEDPWM::begin() {
#ifdef ESP32
    ledcAttach(_pins.r, _frequency, _resolution);
    ledcAttach(_pins.g, _frequency, _resolution);
    ledcAttach(_pins.b, _frequency, _resolution);
    ledcAttach(_pins.w, _frequency, _resolution);
#else
    pinMode(_pins.r, OUTPUT);
    pinMode(_pins.g, OUTPUT);
    pinMode(_pins.b, OUTPUT);
    pinMode(_pins.w, OUTPUT);
#endif
    off();
}

void LEDPWM::setColor(const RGBW& color) {
    _currentColor = color;
    _applyColor();
}

void LEDPWM::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    _applyColor();
}

void LEDPWM::off() {
    _currentColor = RGBW(0, 0, 0, 0);
    _applyColor();
}

void LEDPWM::_applyColor() {
    RGBW scaled = scaleBrightness(_currentColor, _brightness);
#ifdef ESP32
    // Scale 8-bit channel values to match configured resolution
    uint32_t maxDuty = (1 << _resolution) - 1;
    ledcWrite(_pins.r, (uint32_t)scaled.r * maxDuty / 255);
    ledcWrite(_pins.g, (uint32_t)scaled.g * maxDuty / 255);
    ledcWrite(_pins.b, (uint32_t)scaled.b * maxDuty / 255);
    ledcWrite(_pins.w, (uint32_t)scaled.w * maxDuty / 255);
#else
    analogWrite(_pins.r, scaled.r);
    analogWrite(_pins.g, scaled.g);
    analogWrite(_pins.b, scaled.b);
    analogWrite(_pins.w, scaled.w);
#endif
}
