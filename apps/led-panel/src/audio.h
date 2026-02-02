#pragma once

#include <stdint.h>
#include "config.h"

struct AudioState {
    float    energy;         // 0.0 - 1.0 normalized RMS
    bool     beatDetected;   // true for one frame on beat
    float    bpm;            // estimated BPM (smoothed)
};

// Call once in setup()
void audioBegin();

// Call once per frame. Updates state in-place.
void audioUpdate(AudioState& state, uint32_t nowMs);
