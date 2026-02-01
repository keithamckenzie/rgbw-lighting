#pragma once

#include <stdint.h>
#include "rgbw.h"

enum class StripType : uint8_t {
    SK6812_RGBW,   // 4-channel RGBW, 5V, GRB+W byte order
    WS2815B_RGB,   // 3-channel RGB, 12V, GRB byte order, backup data line
};

template<StripType Type>
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
    static constexpr StripType stripType() { return Type; }

private:
    struct Impl;          // specialized per StripType in .cpp
    Impl* _impl;
    uint8_t _pin;
    uint16_t _numLeds;
    uint8_t _brightness;
    RGBW* _pixels;
};

extern template class LEDStrip<StripType::SK6812_RGBW>;
extern template class LEDStrip<StripType::WS2815B_RGB>;
