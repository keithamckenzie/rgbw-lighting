#include "audio.h"

#if !defined(NATIVE_BUILD)

#if AUDIO_ENABLED

#include <Arduino.h>
#include <esp_log.h>

#include "audio_input.h"

static_assert(
    AUDIO_NUM_BANDS == AUDIO_INPUT_NUM_BANDS,
    "AUDIO_NUM_BANDS must match AUDIO_INPUT_NUM_BANDS"
);

namespace {

constexpr const char* TAG = "dj_booth.audio";
constexpr uint32_t AUDIO_STALE_MS = 500;

constexpr float kBeatThreshold = BEAT_THRESHOLD;
constexpr uint16_t kBeatCooldownMs = BEAT_COOLDOWN_MS;
constexpr float kBpmEmaAlpha = BPM_EMA_ALPHA;

AudioInput s_audioInput;

}  // namespace

void audioBegin() {
    const AudioInputMode mode = (AUDIO_INPUT_MODE == AUDIO_INPUT_MODE_ADC)
        ? AudioInputMode::I2S_ADC
        : AudioInputMode::I2S_MIC;

    AudioInputConfig config = audioInputDefaultConfig(mode);
    config.pinSCK = PIN_I2S_SCK;
    config.pinWS = PIN_I2S_WS;
    config.pinSD = PIN_I2S_SD;
    config.pinMCLK = PIN_I2S_MCLK;
    config.sampleRate = AUDIO_SAMPLE_RATE;
    config.fftSize = AUDIO_FFT_SIZE;
    config.beatThreshold = kBeatThreshold;
    config.beatCooldownMs = kBeatCooldownMs;
    config.bpmAlpha = kBpmEmaAlpha;

    const AudioInputError error = s_audioInput.begin(config);
    if (error != AudioInputError::Ok) {
        ESP_LOGW(TAG, "AudioInput begin failed: %u", static_cast<unsigned>(error));
    }
}

void audioUpdate(AudioState& state, uint32_t nowMs) {
    AudioSpectrum spectrum{};
    if (!s_audioInput.getSpectrum(spectrum)) {
        audioResetState(state);
        return;
    }

    if ((nowMs - spectrum.timestampMs) > AUDIO_STALE_MS) {
        audioResetState(state);
        return;
    }

    state.energy = spectrum.rmsEnergy;
    state.beatDetected = spectrum.beatDetected;
    state.bpm = spectrum.bpm;
    state.beatPhase = spectrum.beatPhase;
    state.nextBeatMs = spectrum.nextBeatMs;
    for (uint8_t index = 0; index < AUDIO_NUM_BANDS; index++) {
        state.bandEnergy[index] = spectrum.bandEnergy[index];
    }
}

uint32_t audioMsToNextBeat(const AudioState& state, uint32_t nowMs, uint32_t maxDelayMs) {
    if (state.bpm < 1.0f || state.nextBeatMs == 0 || state.nextBeatMs <= nowMs) {
        return 0;
    }

    const uint32_t deltaMs = state.nextBeatMs - nowMs;
    return (deltaMs > maxDelayMs) ? 0 : deltaMs;
}

#else

void audioBegin() {}

void audioUpdate(AudioState& state, uint32_t nowMs) {
    (void)nowMs;
    audioResetState(state);
}

uint32_t audioMsToNextBeat(const AudioState& state, uint32_t nowMs, uint32_t maxDelayMs) {
    (void)state;
    (void)nowMs;
    (void)maxDelayMs;
    return 0;
}

#endif

#endif
