#pragma once

#include <stdint.h>
#include "rgbw.h"

struct PWMPins {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
};

class LEDPWM {
public:
    LEDPWM(const PWMPins& pins, uint32_t frequency = 19531, uint8_t resolution = 12);

    void begin();
    void setColor(const RGBW& color);
    void setBrightness(uint8_t brightness);
    void off();

private:
    PWMPins _pins;
    uint32_t _frequency;
    uint8_t _resolution;
    uint8_t _brightness;
    RGBW _currentColor;

    void _applyColor();
};
