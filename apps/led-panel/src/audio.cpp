#include "audio.h"

#if AUDIO_ENABLED

#include <Arduino.h>
#include <math.h>

// --- Sliding window for beat detection ---
#define ENERGY_WINDOW 16
static float s_energyHistory[ENERGY_WINDOW];
static uint8_t s_energyIndex = 0;
static uint32_t s_lastBeatMs = 0;
static float s_bpmEma = 120.0f;

void audioBegin() {
    pinMode(PIN_MIC, INPUT);
    for (uint8_t i = 0; i < ENERGY_WINDOW; i++) {
        s_energyHistory[i] = 0.0f;
    }
}

void audioUpdate(AudioState& state, uint32_t nowMs) {
    state.beatDetected = false;

    // --- Sample burst (assumes default 12-bit ADC, max sample ±2048) ---
    int32_t sumSq = 0;
    for (uint16_t i = 0; i < AUDIO_SAMPLE_COUNT; i++) {
        int16_t sample = analogRead(PIN_MIC) - 2048;  // center around zero (12-bit ADC)
        sumSq += (int32_t)sample * sample;
    }

    // RMS calculation
    float meanSq = (float)sumSq / AUDIO_SAMPLE_COUNT;
    float rms = (meanSq > 0.0f) ? sqrtf(meanSq) : 0.0f;

    // Normalize to 0.0-1.0 (2048 is max amplitude for 12-bit centered)
    float energy = rms / 2048.0f;
    if (energy > 1.0f) energy = 1.0f;
    state.energy = energy;

    // --- Sliding window average ---
    s_energyHistory[s_energyIndex] = energy;
    s_energyIndex = (s_energyIndex + 1) % ENERGY_WINDOW;

    float avgEnergy = 0.0f;
    for (uint8_t i = 0; i < ENERGY_WINDOW; i++) {
        avgEnergy += s_energyHistory[i];
    }
    avgEnergy /= ENERGY_WINDOW;

    // --- Beat detection ---
    if (energy > avgEnergy * BEAT_THRESHOLD &&
        energy > 0.05f &&
        (nowMs - s_lastBeatMs) >= BEAT_COOLDOWN_MS) {
        state.beatDetected = true;

        // BPM tracking from beat interval
        if (s_lastBeatMs > 0) {
            uint32_t interval = nowMs - s_lastBeatMs;
            if (interval > 250 && interval < 2000) {  // 30-240 BPM range
                float instantBpm = 60000.0f / interval;
                s_bpmEma = s_bpmEma * (1.0f - BPM_EMA_ALPHA) + instantBpm * BPM_EMA_ALPHA;
            }
        }
        s_lastBeatMs = nowMs;
    }

    state.bpm = s_bpmEma;
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
}

#endif
