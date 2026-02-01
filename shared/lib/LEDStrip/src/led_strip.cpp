#include "led_strip.h"
#include <Adafruit_NeoPixel.h>

static inline Adafruit_NeoPixel* neo(void* ptr) {
    return static_cast<Adafruit_NeoPixel*>(ptr);
}

LEDStrip::LEDStrip(uint8_t pin, uint16_t numLeds, StripType type)
    : _pin(pin), _numLeds(numLeds), _brightness(255), _type(type), _neopixel(nullptr) {
    _pixels = new RGBW[numLeds];
    clear();
}

LEDStrip::~LEDStrip() {
    delete[] _pixels;
    delete neo(_neopixel);
    _neopixel = nullptr;
}

void LEDStrip::begin() {
    uint16_t flags;
    switch (_type) {
        case StripType::WS2815B_RGB:
            flags = NEO_GRB + NEO_KHZ800;
            break;
        case StripType::SK6812_RGBW:
        default:
            flags = NEO_GRBW + NEO_KHZ800;
            break;
    }

    _neopixel = new Adafruit_NeoPixel(_numLeds, _pin, flags);
    neo(_neopixel)->begin();
    neo(_neopixel)->setBrightness(_brightness);
    neo(_neopixel)->show();
}

void LEDStrip::show() {
    Adafruit_NeoPixel* np = neo(_neopixel);
    for (uint16_t i = 0; i < _numLeds; i++) {
        const RGBW& px = _pixels[i];
        if (_type == StripType::WS2815B_RGB) {
            // Fold white channel back into RGB for 3-channel strips
            uint8_t r = (uint8_t)((px.r + px.w > 255) ? 255 : px.r + px.w);
            uint8_t g = (uint8_t)((px.g + px.w > 255) ? 255 : px.g + px.w);
            uint8_t b = (uint8_t)((px.b + px.w > 255) ? 255 : px.b + px.w);
            np->setPixelColor(i, np->Color(r, g, b));
        } else {
            np->setPixelColor(i, np->Color(px.r, px.g, px.b, px.w));
        }
    }
    np->show();
}

void LEDStrip::setPixel(uint16_t index, const RGBW& color) {
    if (index < _numLeds) {
        _pixels[index] = color;
    }
}

void LEDStrip::fill(const RGBW& color) {
    for (uint16_t i = 0; i < _numLeds; i++) {
        _pixels[i] = color;
    }
}

void LEDStrip::clear() {
    fill(RGBW(0, 0, 0, 0));
}

void LEDStrip::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    if (_neopixel) {
        neo(_neopixel)->setBrightness(brightness);
    }
}
