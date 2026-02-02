#pragma once

#include <stdint.h>
#include "config.h"
#include "effects.h"

struct InputState {
    bool     powerOn;
    uint8_t  brightness;     // 0-255 from pot
    EffectMode currentMode;
    bool     modeChanged;    // true for one frame after mode switch
};

// Call once in setup(). defaultBrightness pre-fills the ADC moving average
// so readBrightness() returns a sane value before the first real ADC read.
void inputBegin(uint8_t defaultBrightness);

// Call once per frame. Returns updated state.
void inputUpdate(InputState& state, uint32_t nowMs);
