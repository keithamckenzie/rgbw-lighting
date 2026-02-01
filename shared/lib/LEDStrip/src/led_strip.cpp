#include "led_strip.h"
#include <NeoPixelBusLg.h>

// --- Platform method selection ---
// ESP32:  NeoEsp32RmtN* methods with auto-assigned RMT channels via 3-arg
//         NeoPixelBusLg constructor (numLeds, pin, NeoBusChannel).
// AVR:    Generic aliases resolve to bit-bang methods (no channel concept).
#ifdef ESP32
using Sk6812Method = NeoEsp32RmtNSk6812Method;
using Ws2812xMethod = NeoEsp32RmtNWs2812xMethod;
#else
using Sk6812Method = NeoSk6812Method;
using Ws2812xMethod = NeoWs2812xMethod;
#endif

// --- RMT channel counter (ESP32 only) ---
// Each LEDStrip instance claims the next available RMT TX channel.
// Exceeding the hardware limit (8 on ESP32, 4 on S2/S3, 2 on C3) will
// cause NeoPixelBus to fail at Begin().
#ifdef ESP32
static uint8_t s_nextRmtChannel = 0;
#endif

// --- Impl specialization: SK6812 RGBW ---
template<>
struct LEDStrip<StripType::SK6812_RGBW>::Impl {
    NeoPixelBusLg<NeoGrbwFeature, Sk6812Method, NeoGammaNullMethod> bus;
#ifdef ESP32
    Impl(uint16_t n, uint8_t p) : bus(n, p, (NeoBusChannel)s_nextRmtChannel++) {}
#else
    Impl(uint16_t n, uint8_t p) : bus(n, p) {}
#endif

    void setPixel(uint16_t i, const RGBW& px) {
        bus.SetPixelColor(i, RgbwColor(px.r, px.g, px.b, px.w));
    }
};

// --- Impl specialization: WS2815B RGB (white-fold) ---
template<>
struct LEDStrip<StripType::WS2815B_RGB>::Impl {
    NeoPixelBusLg<NeoGrbFeature, Ws2812xMethod, NeoGammaNullMethod> bus;
#ifdef ESP32
    Impl(uint16_t n, uint8_t p) : bus(n, p, (NeoBusChannel)s_nextRmtChannel++) {}
#else
    Impl(uint16_t n, uint8_t p) : bus(n, p) {}
#endif

    void setPixel(uint16_t i, const RGBW& px) {
        // Fold white channel back into RGB for 3-channel strips
        uint8_t r = (uint8_t)((px.r + px.w > 255) ? 255 : px.r + px.w);
        uint8_t g = (uint8_t)((px.g + px.w > 255) ? 255 : px.g + px.w);
        uint8_t b = (uint8_t)((px.b + px.w > 255) ? 255 : px.b + px.w);
        bus.SetPixelColor(i, RgbColor(r, g, b));
    }
};

// --- Template method implementations ---

template<StripType Type>
LEDStrip<Type>::LEDStrip(uint8_t pin, uint16_t numLeds)
    : _impl(nullptr), _pin(pin), _numLeds(numLeds), _brightness(255), _pixels(nullptr) {
    _pixels = new RGBW[numLeds];
    clear();
}

template<StripType Type>
LEDStrip<Type>::~LEDStrip() {
    delete[] _pixels;
    delete _impl;
}

template<StripType Type>
void LEDStrip<Type>::begin() {
    _impl = new Impl(_numLeds, _pin);
    _impl->bus.Begin();
    _impl->bus.SetLuminance(_brightness);
    _impl->bus.Show();
}

template<StripType Type>
void LEDStrip<Type>::show() {
    _impl->bus.SetLuminance(_brightness);
    for (uint16_t i = 0; i < _numLeds; i++) {
        _impl->setPixel(i, _pixels[i]);
    }
    _impl->bus.Show();
}

template<StripType Type>
void LEDStrip<Type>::setPixel(uint16_t index, const RGBW& color) {
    if (index < _numLeds) {
        _pixels[index] = color;
    }
}

template<StripType Type>
void LEDStrip<Type>::fill(const RGBW& color) {
    for (uint16_t i = 0; i < _numLeds; i++) {
        _pixels[i] = color;
    }
}

template<StripType Type>
void LEDStrip<Type>::clear() {
    fill(RGBW(0, 0, 0, 0));
}

template<StripType Type>
void LEDStrip<Type>::setBrightness(uint8_t brightness) {
    _brightness = brightness;
}

// --- Explicit instantiation ---
template class LEDStrip<StripType::SK6812_RGBW>;
template class LEDStrip<StripType::WS2815B_RGB>;
