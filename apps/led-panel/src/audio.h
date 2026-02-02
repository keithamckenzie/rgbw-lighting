#pragma once

#include <stdint.h>
#include "config.h"

#define AUDIO_NUM_BANDS 8

struct AudioState {
    float    energy;                       // 0.0-1.0 normalized RMS
    bool     beatDetected;                 // true for one frame on beat
    float    bpm;                          // estimated BPM (smoothed)
    float    bandEnergy[AUDIO_NUM_BANDS];  // per-band 0.0-1.0
    float    beatPhase;                    // 0.0-1.0 position within beat
    uint32_t nextBeatMs;                   // predicted next beat timestamp
};

// Call once in setup()
void audioBegin();

// Call once per frame. Updates state in-place.
void audioUpdate(AudioState& state, uint32_t nowMs);

// Returns ms until the nearest beat boundary (forward only).
// If no BPM data available (bpm < 1), returns 0 (no delay).
// maxDelayMs caps the wait to avoid stalling on lost tracking.
uint32_t audioMsToNextBeat(const AudioState& state, uint32_t nowMs,
                           uint32_t maxDelayMs = 300);
