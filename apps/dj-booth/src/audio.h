#pragma once

#include <stdint.h>

#include "config.h"

#define AUDIO_NUM_BANDS 8

struct AudioState {
    float energy;
    bool beatDetected;
    float bpm;
    float bandEnergy[AUDIO_NUM_BANDS];
    float beatPhase;
    uint32_t nextBeatMs;
};

inline void audioResetState(AudioState& state) {
    state.energy = 0.0f;
    state.beatDetected = false;
    state.bpm = 0.0f;
    state.beatPhase = 0.0f;
    state.nextBeatMs = 0;
    for (uint8_t index = 0; index < AUDIO_NUM_BANDS; index++) {
        state.bandEnergy[index] = 0.0f;
    }
}

#ifdef NATIVE_BUILD

inline void audioBegin() {}

inline void audioUpdate(AudioState& state, uint32_t nowMs) {
    (void)nowMs;
    audioResetState(state);
}

inline uint32_t audioMsToNextBeat(
    const AudioState& state,
    uint32_t nowMs,
    uint32_t maxDelayMs = 300
) {
    (void)state;
    (void)nowMs;
    (void)maxDelayMs;
    return 0;
}

#else

void audioBegin();
void audioUpdate(AudioState& state, uint32_t nowMs);
uint32_t audioMsToNextBeat(const AudioState& state, uint32_t nowMs, uint32_t maxDelayMs = 300);

#endif
