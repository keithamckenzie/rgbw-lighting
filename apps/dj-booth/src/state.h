#pragma once

#include <stdint.h>
#include "effects.h"
#include "rgbw.h"

struct LightState {
    bool powerOn;
    uint8_t brightness;
    HSV color;
    EffectMode effectMode;
    bool modeChanged;
};

LightState makeDefaultLightState();
void lightStateBeginFrame(LightState& state);
HSV normalizeHSV(const HSV& color);
RGBW lightStateColorRGBW(const LightState& state);
