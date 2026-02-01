#pragma once

#include <stdint.h>
#include "rgbw.h"

enum class StripType : uint8_t {
    SK6812_RGBW,   // 4-channel RGBW, 5V, GRB+W byte order
    WS2815B_RGB,   // 3-channel RGB, 12V, GRB byte order, backup data line
};

class LEDStrip {
public:
    LEDStrip(uint8_t pin, uint16_t numLeds, StripType type = StripType::SK6812_RGBW);
    ~LEDStrip();

    void begin();
    void show();

    void setPixel(uint16_t index, const RGBW& color);
    void fill(const RGBW& color);
    void clear();

    void setBrightness(uint8_t brightness);
    uint16_t numPixels() const { return _numLeds; }
    StripType stripType() const { return _type; }

private:
    uint8_t _pin;
    uint16_t _numLeds;
    uint8_t _brightness;
    StripType _type;
    RGBW* _pixels;
    void* _neopixel;  // Adafruit_NeoPixel*, opaque to avoid header dependency
};
