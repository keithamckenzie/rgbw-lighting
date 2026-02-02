#pragma once

#ifdef ESP32

#include <stdint.h>

enum class AudioInputMode : uint8_t {
    I2S_MIC,   // ICS-43434, ICS-43432, INMP441, SPH0645 (no MCLK)
    I2S_ADC,   // PCM1808 external ADC (requires MCLK, see pinMCLK)
};

#define AUDIO_INPUT_NUM_BANDS 8

struct AudioInputConfig {
    AudioInputMode mode;
    int8_t  pinSCK;           // BCLK (default: 26)
    int8_t  pinWS;            // LRCLK (default: 25)
    int8_t  pinSD;            // Data in (default: 33)
    int8_t  pinMCLK;          // MCLK for I2S_ADC mode (default: 0)
    uint32_t sampleRate;      // Hz (default: 44100)
    uint16_t fftSize;         // Must be power of 2 (default: 1024)
    uint8_t  dmaBufCount;     // Ring buffer count (default: 4)
    uint16_t dmaBufLen;       // Samples per DMA buffer (default: 512)
    uint8_t  taskCore;        // FreeRTOS core (default: 1)
    uint8_t  taskPriority;    // FreeRTOS priority (default: 4)
    // Beat detection tuning
    float    beatThreshold;   // Energy/average ratio (default: 1.5)
    uint16_t beatCooldownMs;  // Min ms between beats (default: 200)
    float    bpmAlpha;        // BPM EMA smoothing (default: 0.15)
    // Band smoothing
    float    smoothRise;      // Attack EMA alpha (default: 0.3)
    float    smoothFall;      // Decay EMA alpha (default: 0.05)
};

// Get a config struct populated with sensible defaults for the given mode
AudioInputConfig audioInputDefaultConfig(AudioInputMode mode);

struct AudioSpectrum {
    float    bandEnergy[AUDIO_INPUT_NUM_BANDS]; // 0.0-1.0 per band
    float    rmsEnergy;                          // 0.0-1.0 overall
    bool     beatDetected;
    float    bpm;
    float    beatPhase;     // 0.0-1.0 position within current beat period
    uint32_t nextBeatMs;    // predicted timestamp of next beat
    uint32_t timestampMs;
};

enum class AudioInputError : uint8_t {
    Ok = 0,
    InvalidPin,
    InvalidConfig,
    AllocFailed,
    I2SDriverFailed,
    I2SSetPinFailed,
    FFTInitFailed,
    TaskCreateFailed,
    AlreadyRunning,
};

class AudioInput {
public:
    AudioInput();
    ~AudioInput();

    AudioInputError begin(const AudioInputConfig& config);
    void end();

    // Non-blocking peek at latest spectrum data. Returns false if no data available.
    bool getSpectrum(AudioSpectrum& out) const;

    bool isRunning() const;
    uint32_t getStackHighWaterMark() const;

private:
    AudioInput(const AudioInput&);
    AudioInput& operator=(const AudioInput&);

    struct Impl;   // PIMPL hides FreeRTOS/I2S/ESP-DSP headers
    Impl* _impl;

    friend void audioTaskFunc(void* param);
};

#endif // ESP32
