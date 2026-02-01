#include "rgbw.h"

RGBW hsvToRGBW(const HSV& hsv) {
    if (hsv.s == 0) {
        return RGBW(0, 0, 0, hsv.v);
    }

    uint8_t region = hsv.h / 60;
    uint8_t remainder = (hsv.h - (region * 60)) * 255 / 60;

    uint8_t p = (hsv.v * (255 - hsv.s)) >> 8;
    uint8_t q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    uint8_t t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    uint8_t r, g, b;
    switch (region) {
        case 0:  r = hsv.v; g = t; b = p; break;
        case 1:  r = q; g = hsv.v; b = p; break;
        case 2:  r = p; g = hsv.v; b = t; break;
        case 3:  r = p; g = q; b = hsv.v; break;
        case 4:  r = t; g = p; b = hsv.v; break;
        default: r = hsv.v; g = p; b = q; break;
    }

    // Extract white component from the minimum of RGB
    uint8_t w = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return RGBW(r - w, g - w, b - w, w);
}

RGBW lerpRGBW(const RGBW& a, const RGBW& b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return RGBW(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.w + (b.w - a.w) * t
    );
}

RGBW scaleBrightness(const RGBW& color, uint8_t brightness) {
    return RGBW(
        (color.r * brightness) >> 8,
        (color.g * brightness) >> 8,
        (color.b * brightness) >> 8,
        (color.w * brightness) >> 8
    );
}
