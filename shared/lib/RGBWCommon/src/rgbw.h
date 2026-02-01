#pragma once

#include <stdint.h>

struct RGBW {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;

    RGBW() : r(0), g(0), b(0), w(0) {}
    RGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w) : r(r), g(g), b(b), w(w) {}
};

struct HSV {
    uint16_t h;  // 0-360
    uint8_t s;   // 0-255
    uint8_t v;   // 0-255
};

// Convert HSV to RGBW
RGBW hsvToRGBW(const HSV& hsv);

// Linearly interpolate between two RGBW colors (t = 0.0 to 1.0)
RGBW lerpRGBW(const RGBW& a, const RGBW& b, float t);

// Scale brightness (0-255)
RGBW scaleBrightness(const RGBW& color, uint8_t brightness);

// Convert RGBW to RGB by folding white channel back into R, G, B (clamped to 255)
RGBW rgbwToRgb(const RGBW& color);
