#pragma once

#include <stdint.h>
#include "config.h"
#include "effects.h"

struct AudioState;  // forward declaration

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
// audio may be null (ESP8266 or before audioBegin). When non-null and
// AUDIO_ENABLED, mode changes are deferred to the next predicted beat
// boundary for musical synchronization.
void inputUpdate(InputState& state, uint32_t nowMs,
                 const AudioState* audio = nullptr);
