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
    ledcWrite(_pins.r, scaled.r);
    ledcWrite(_pins.g, scaled.g);
    ledcWrite(_pins.b, scaled.b);
    ledcWrite(_pins.w, scaled.w);
#else
    analogWrite(_pins.r, scaled.r);
    analogWrite(_pins.g, scaled.g);
    analogWrite(_pins.b, scaled.b);
    analogWrite(_pins.w, scaled.w);
#endif
}
