#pragma once

#include <stdint.h>
#include "rgbw.h"

class LEDStrip {
public:
    LEDStrip(uint8_t pin, uint16_t numLeds);
    ~LEDStrip();

    void begin();
    void show();

    void setPixel(uint16_t index, const RGBW& color);
    void fill(const RGBW& color);
    void clear();

    void setBrightness(uint8_t brightness);
    uint16_t numPixels() const { return _numLeds; }

private:
    uint8_t _pin;
    uint16_t _numLeds;
    uint8_t _brightness;
    RGBW* _pixels;
};
