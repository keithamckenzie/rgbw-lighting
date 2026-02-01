#include "led_strip.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel* _neopixel = nullptr;

LEDStrip::LEDStrip(uint8_t pin, uint16_t numLeds)
    : _pin(pin), _numLeds(numLeds), _brightness(255) {
    _pixels = new RGBW[numLeds];
    clear();
}

LEDStrip::~LEDStrip() {
    delete[] _pixels;
    delete _neopixel;
    _neopixel = nullptr;
}

void LEDStrip::begin() {
    _neopixel = new Adafruit_NeoPixel(_numLeds, _pin, NEO_GRBW + NEO_KHZ800);
    _neopixel->begin();
    _neopixel->setBrightness(_brightness);
    _neopixel->show();
}

void LEDStrip::show() {
    for (uint16_t i = 0; i < _numLeds; i++) {
        _neopixel->setPixelColor(i, _neopixel->Color(
            _pixels[i].r, _pixels[i].g, _pixels[i].b, _pixels[i].w));
    }
    _neopixel->show();
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
        _neopixel->setBrightness(brightness);
    }
}
