#include "audio.h"

#if AUDIO_ENABLED

#include <Arduino.h>
#include "audio_input.h"

static_assert(AUDIO_NUM_BANDS == AUDIO_INPUT_NUM_BANDS,
              "AUDIO_NUM_BANDS must match AUDIO_INPUT_NUM_BANDS");

#define AUDIO_STALE_MS 500

static AudioInput s_audioInput;

void audioBegin() {
    AudioInputMode mode = (AUDIO_INPUT_MODE == 1)
        ? AudioInputMode::I2S_ADC
        : AudioInputMode::I2S_MIC;
    AudioInputConfig cfg = audioInputDefaultConfig(mode);
    cfg.pinSCK  = PIN_I2S_SCK;
    cfg.pinWS   = PIN_I2S_WS;
    cfg.pinSD   = PIN_I2S_SD;
    cfg.pinMCLK = PIN_I2S_MCLK;
    cfg.beatThreshold  = BEAT_THRESHOLD;
    cfg.beatCooldownMs = BEAT_COOLDOWN_MS;
    cfg.bpmAlpha       = BPM_EMA_ALPHA;

    AudioInputError err = s_audioInput.begin(cfg);
    if (err != AudioInputError::Ok) {
        Serial.print(F("AudioInput begin failed: "));
        Serial.println((uint8_t)err);
    }
}

void audioUpdate(AudioState& state, uint32_t nowMs) {
    AudioSpectrum spectrum;
    if (s_audioInput.getSpectrum(spectrum)) {
        if (nowMs - spectrum.timestampMs > AUDIO_STALE_MS) {
            // Data is stale (task may have died) — zero transients
            state.energy = 0.0f;
            state.beatDetected = false;
            state.bpm = 0.0f;
            state.beatPhase = 0.0f;
            state.nextBeatMs = 0;
            for (uint8_t i = 0; i < AUDIO_NUM_BANDS; i++) {
                state.bandEnergy[i] = 0.0f;
            }
        } else {
            // Fresh data
            state.energy = spectrum.rmsEnergy;
            state.beatDetected = spectrum.beatDetected;
            state.bpm = spectrum.bpm;
            state.beatPhase = spectrum.beatPhase;
            state.nextBeatMs = spectrum.nextBeatMs;
            for (uint8_t i = 0; i < AUDIO_NUM_BANDS; i++) {
                state.bandEnergy[i] = spectrum.bandEnergy[i];
            }
        }
    } else {
        // No data at all — zero everything
        state.energy = 0.0f;
        state.beatDetected = false;
        state.bpm = 0.0f;
        state.beatPhase = 0.0f;
        state.nextBeatMs = 0;
        for (uint8_t i = 0; i < AUDIO_NUM_BANDS; i++) {
            state.bandEnergy[i] = 0.0f;
        }
    }
}

uint32_t audioMsToNextBeat(const AudioState& state, uint32_t nowMs,
                           uint32_t maxDelayMs) {
    if (state.bpm < 1.0f || state.nextBeatMs == 0) {
        return 0;  // no tracking, don't delay
    }
    if (state.nextBeatMs <= nowMs) {
        return 0;  // beat is now or past
    }
    uint32_t delta = state.nextBeatMs - nowMs;
    return (delta > maxDelayMs) ? 0 : delta;
}

#else
// --- ESP8266 / non-audio stubs ---

void audioBegin() {
    // No-op
}

void audioUpdate(AudioState& state, uint32_t nowMs) {
    (void)nowMs;
    state.energy = 0.0f;
    state.beatDetected = false;
    state.bpm = 0.0f;
    state.beatPhase = 0.0f;
    state.nextBeatMs = 0;
    for (uint8_t i = 0; i < AUDIO_NUM_BANDS; i++) {
        state.bandEnergy[i] = 0.0f;
    }
}

uint32_t audioMsToNextBeat(const AudioState& state, uint32_t nowMs,
                           uint32_t maxDelayMs) {
    (void)state;
    (void)nowMs;
    (void)maxDelayMs;
    return 0;
}

#endif
