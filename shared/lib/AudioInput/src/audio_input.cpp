#include "audio_input.h"

#ifdef ESP32

#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <new>

#include "dsps_fft2r.h"
#include "dsps_wind_hann.h"

static const char* TAG = "AudioInput";

// Band frequency boundaries (Hz) — one more than AUDIO_INPUT_NUM_BANDS
// Octave-spaced from ~43 Hz to ~11025 Hz
static const float BAND_FREQ[AUDIO_INPUT_NUM_BANDS + 1] = {
    43.0f, 86.0f, 172.0f, 345.0f, 689.0f, 1378.0f, 2756.0f, 5512.0f, 11025.0f
};

#define ENERGY_HISTORY_LEN 32

struct AudioInput::Impl {
    AudioInputConfig config;
    TaskHandle_t     taskHandle;
    QueueHandle_t    spectrumQueue;
    SemaphoreHandle_t taskExitSem;
    volatile bool    running;

    // Pre-allocated buffers
    int32_t* rawSamples;    // [fftSize] mono samples for processing
    int32_t* i2sReadBuf;    // [fftSize * 2] stereo read buffer (ADC mode only)
    float*   fftData;       // [fftSize * 2] interleaved real/imag
    float*   hannWindow;    // [fftSize]
    float*   magnitudes;    // [fftSize / 2]

    // Dynamic band bin ranges (computed from config in begin())
    uint16_t bandBins[AUDIO_INPUT_NUM_BANDS][2];

    // Beat detection state
    float    energyHistory[ENERGY_HISTORY_LEN];
    uint8_t  energyIndex;
    uint32_t lastBeatMs;
    float    bpmEma;

    // Band smoothing state
    float    smoothedBands[AUDIO_INPUT_NUM_BANDS];

    // DC blocking filter state (y[n] = x[n] - x[n-1] + alpha * y[n-1])
    float    dcPrevX;   // x[n-1]
    float    dcPrevY;   // y[n-1]
};

// --- Default config ---
AudioInputConfig audioInputDefaultConfig(AudioInputMode mode) {
    AudioInputConfig cfg;
    cfg.mode         = mode;
    cfg.pinSCK       = 26;
    cfg.pinWS        = 25;
    cfg.pinSD        = 33;
    cfg.pinMCLK      = 0;
    cfg.sampleRate   = 44100;
    cfg.fftSize      = 1024;
    cfg.dmaBufCount  = 4;
    cfg.dmaBufLen    = 512;
    cfg.taskCore     = 1;
    cfg.taskPriority = 4;
    cfg.beatThreshold  = 1.5f;
    cfg.beatCooldownMs = 200;
    cfg.bpmAlpha       = 0.15f;
    cfg.smoothRise     = 0.3f;
    cfg.smoothFall     = 0.05f;
    return cfg;
}

// --- Audio processing task ---
void audioTaskFunc(void* param) {
    AudioInput::Impl* impl = static_cast<AudioInput::Impl*>(param);
    const AudioInputConfig& cfg = impl->config;
    const uint16_t fftSize = cfg.fftSize;
    const uint16_t halfFFT = fftSize / 2;
    const float maxAmplitude = 8388608.0f; // 2^23 for 24-bit audio
    const bool stereoMode = (cfg.mode == AudioInputMode::I2S_ADC);

    ESP_LOGI(TAG, "Task started: %" PRIu32 " Hz, %u-pt FFT, core %u, pri %u",
             cfg.sampleRate, fftSize, cfg.taskCore, cfg.taskPriority);

    while (impl->running) {
        // 1. Read I2S data (blocks until samples available)
        size_t bytesRead = 0;
        esp_err_t err;

        if (stereoMode) {
            // ADC mode: read interleaved stereo (L/R), extract left channel
            const size_t stereoBytes = fftSize * 2 * sizeof(int32_t);
            err = i2s_read(I2S_NUM_0, impl->i2sReadBuf,
                           stereoBytes, &bytesRead, pdMS_TO_TICKS(200));
            if (err != ESP_OK || bytesRead < stereoBytes) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            // Deinterleave: copy left channel (even indices) to rawSamples
            for (uint16_t i = 0; i < fftSize; i++) {
                impl->rawSamples[i] = impl->i2sReadBuf[i * 2];
            }
        } else {
            // Mic mode: mono, read directly into rawSamples
            const size_t monoBytes = fftSize * sizeof(int32_t);
            err = i2s_read(I2S_NUM_0, impl->rawSamples,
                           monoBytes, &bytesRead, pdMS_TO_TICKS(200));
            if (err != ESP_OK || bytesRead < monoBytes) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
        }

        // 2. Convert 32-bit I2S data to float, apply DC blocking filter, compute RMS
        // DC blocker: y[n] = x[n] - x[n-1] + alpha * y[n-1], alpha = 0.995
        const float dcAlpha = 0.995f;
        float sumSq = 0.0f;
        for (uint16_t i = 0; i < fftSize; i++) {
            // Shift right 8 to get 24-bit data from 32-bit frame
            float x = (float)(impl->rawSamples[i] >> 8) / maxAmplitude;

            // Apply DC blocking filter
            float y = x - impl->dcPrevX + dcAlpha * impl->dcPrevY;
            impl->dcPrevX = x;
            impl->dcPrevY = y;

            sumSq += y * y;

            // Fill interleaved complex array: [real, imag, real, imag, ...]
            impl->fftData[i * 2]     = y * impl->hannWindow[i];
            impl->fftData[i * 2 + 1] = 0.0f;
        }

        float rms = sqrtf(sumSq / fftSize);
        if (rms > 1.0f) rms = 1.0f;

        // 3. FFT (in-place)
        dsps_fft2r_fc32(impl->fftData, fftSize);
        dsps_bit_rev_fc32(impl->fftData, fftSize);

        // 4. Compute magnitudes for bins 0..halfFFT
        for (uint16_t i = 0; i < halfFFT; i++) {
            float re = impl->fftData[i * 2];
            float im = impl->fftData[i * 2 + 1];
            impl->magnitudes[i] = sqrtf(re * re + im * im);
        }

        // 5. Compute 8-band energies with asymmetric EMA smoothing
        AudioSpectrum spectrum;
        for (uint8_t band = 0; band < AUDIO_INPUT_NUM_BANDS; band++) {
            uint16_t startBin = impl->bandBins[band][0];
            uint16_t endBin   = impl->bandBins[band][1];
            if (endBin > halfFFT) endBin = halfFFT;

            float bandSum = 0.0f;
            uint16_t binCount = 0;
            for (uint16_t bin = startBin; bin < endBin; bin++) {
                bandSum += impl->magnitudes[bin];
                binCount++;
            }

            float bandAvg = (binCount > 0) ? (bandSum / binCount) : 0.0f;

            // Normalize: scale factor chosen empirically to map typical
            // music levels to 0.0-1.0 range
            float rawEnergy = bandAvg * 4.0f;
            if (rawEnergy > 1.0f) rawEnergy = 1.0f;

            // Asymmetric EMA: fast attack, slow decay
            float alpha = (rawEnergy > impl->smoothedBands[band])
                          ? cfg.smoothRise : cfg.smoothFall;
            impl->smoothedBands[band] =
                impl->smoothedBands[band] * (1.0f - alpha) + rawEnergy * alpha;

            spectrum.bandEnergy[band] = impl->smoothedBands[band];
        }

        spectrum.rmsEnergy = rms;
        spectrum.timestampMs = (uint32_t)(esp_timer_get_time() / 1000ULL);

        // 6. Beat detection on bass bands (0 + 1)
        float bassEnergy = (spectrum.bandEnergy[0] + spectrum.bandEnergy[1]) * 0.5f;

        impl->energyHistory[impl->energyIndex] = bassEnergy;
        impl->energyIndex = (impl->energyIndex + 1) % ENERGY_HISTORY_LEN;

        float avgEnergy = 0.0f;
        for (uint8_t i = 0; i < ENERGY_HISTORY_LEN; i++) {
            avgEnergy += impl->energyHistory[i];
        }
        avgEnergy /= ENERGY_HISTORY_LEN;

        uint32_t nowMs = spectrum.timestampMs;
        spectrum.beatDetected = false;

        if (bassEnergy > avgEnergy * cfg.beatThreshold &&
            bassEnergy > 0.05f &&
            (nowMs - impl->lastBeatMs) >= cfg.beatCooldownMs) {
            spectrum.beatDetected = true;

            // BPM tracking from beat interval
            if (impl->lastBeatMs > 0) {
                uint32_t interval = nowMs - impl->lastBeatMs;
                if (interval > 250 && interval < 2000) {  // 30-240 BPM range
                    float instantBpm = 60000.0f / interval;
                    impl->bpmEma = impl->bpmEma * (1.0f - cfg.bpmAlpha)
                                 + instantBpm * cfg.bpmAlpha;
                }
            }
            impl->lastBeatMs = nowMs;
        }

        spectrum.bpm = impl->bpmEma;

        // 7. Beat phase and prediction
        if (impl->bpmEma > 1.0f && impl->lastBeatMs > 0) {
            float beatPeriodMs = 60000.0f / impl->bpmEma;
            uint32_t elapsed = nowMs - impl->lastBeatMs;
            spectrum.beatPhase = (float)elapsed / beatPeriodMs;
            spectrum.beatPhase = fmodf(spectrum.beatPhase, 1.0f);
            // Predict next beat: lastBeat + ceil(elapsed/period) * period
            uint32_t periodsElapsed = (elapsed > 0)
                ? (uint32_t)(elapsed / (uint32_t)beatPeriodMs) + 1
                : 1;
            spectrum.nextBeatMs = impl->lastBeatMs
                + (uint32_t)(periodsElapsed * beatPeriodMs);
        } else {
            spectrum.beatPhase = 0.0f;
            spectrum.nextBeatMs = 0;
        }

        // 8. Publish (latest-wins, never blocks)
        xQueueOverwrite(impl->spectrumQueue, &spectrum);
    }

    // Task cleanup: signal we're done, then delete ourselves
    ESP_LOGI(TAG, "Task exiting");
    xSemaphoreGive(impl->taskExitSem);
    vTaskDelete(NULL);
}

// --- AudioInput public methods ---

AudioInput::AudioInput() : _impl(nullptr) {}

AudioInput::~AudioInput() {
    end();
}

AudioInputError AudioInput::begin(const AudioInputConfig& config) {
    if (_impl != nullptr && _impl->running) {
        return AudioInputError::AlreadyRunning;
    }

    // Validate pins
    if (config.pinSCK < 0 || config.pinWS < 0 || config.pinSD < 0 ||
        (config.mode == AudioInputMode::I2S_ADC && config.pinMCLK < 0)) {
        return AudioInputError::InvalidPin;
    }

    // Validate FFT config (fftSize must be power of 2, sampleRate must
    // satisfy Nyquist for the top band frequency)
    if (config.sampleRate == 0 || config.fftSize < 64 ||
        (config.fftSize & (config.fftSize - 1)) != 0 ||
        config.sampleRate < (uint32_t)(2 * BAND_FREQ[AUDIO_INPUT_NUM_BANDS])) {
        ESP_LOGE(TAG, "Invalid config: sampleRate=%" PRIu32 " fftSize=%u (min rate=%" PRIu32 ")",
                 config.sampleRate, config.fftSize,
                 (uint32_t)(2 * BAND_FREQ[AUDIO_INPUT_NUM_BANDS]));
        return AudioInputError::InvalidConfig;
    }

    // Allocate impl
    _impl = new (std::nothrow) Impl();
    if (!_impl) {
        return AudioInputError::AllocFailed;
    }

    _impl->config = config;
    _impl->taskHandle = nullptr;
    _impl->spectrumQueue = nullptr;
    _impl->taskExitSem = nullptr;
    _impl->running = false;
    _impl->rawSamples = nullptr;
    _impl->i2sReadBuf = nullptr;
    _impl->fftData = nullptr;
    _impl->hannWindow = nullptr;
    _impl->magnitudes = nullptr;

    // Create task-exit semaphore (binary, starts empty)
    _impl->taskExitSem = xSemaphoreCreateBinary();
    if (!_impl->taskExitSem) {
        ESP_LOGE(TAG, "Semaphore create failed");
        end();
        return AudioInputError::AllocFailed;
    }

    const uint16_t fftSize = config.fftSize;

    // Allocate buffers
    _impl->rawSamples = new (std::nothrow) int32_t[fftSize];
    _impl->fftData    = new (std::nothrow) float[fftSize * 2];
    _impl->hannWindow = new (std::nothrow) float[fftSize];
    _impl->magnitudes = new (std::nothrow) float[fftSize / 2];

    // ADC mode uses stereo framing — need 2x buffer for deinterleaving
    if (config.mode == AudioInputMode::I2S_ADC) {
        _impl->i2sReadBuf = new (std::nothrow) int32_t[fftSize * 2];
    }

    if (!_impl->rawSamples || !_impl->fftData ||
        !_impl->hannWindow || !_impl->magnitudes ||
        (config.mode == AudioInputMode::I2S_ADC && !_impl->i2sReadBuf)) {
        end();
        return AudioInputError::AllocFailed;
    }

    // Pre-compute Hann window
    dsps_wind_hann_f32(_impl->hannWindow, fftSize);

    // Init ESP-DSP FFT tables
    esp_err_t dspErr = dsps_fft2r_init_fc32(NULL, fftSize);
    if (dspErr != ESP_OK) {
        ESP_LOGE(TAG, "FFT init failed: %d", dspErr);
        end();
        return AudioInputError::FFTInitFailed;
    }

    // Configure I2S
    i2s_config_t i2sCfg = {};
    i2sCfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2sCfg.sample_rate = config.sampleRate;
    i2sCfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2sCfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sCfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sCfg.dma_buf_count = config.dmaBufCount;
    i2sCfg.dma_buf_len = config.dmaBufLen;

    if (config.mode == AudioInputMode::I2S_ADC) {
        // External ADC (PCM1808): stereo framing, APLL for MCLK
        i2sCfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
        i2sCfg.use_apll = true;
        i2sCfg.fixed_mclk = 256 * config.sampleRate;
    } else {
        // I2S microphone: mono left channel only
        i2sCfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
        i2sCfg.use_apll = false;
        i2sCfg.fixed_mclk = 0;
    }

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sCfg, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S driver install failed: %d", err);
        end();
        return AudioInputError::I2SDriverFailed;
    }

    i2s_pin_config_t pinCfg = {};
    pinCfg.bck_io_num   = config.pinSCK;
    pinCfg.ws_io_num    = config.pinWS;
    pinCfg.data_in_num  = config.pinSD;
    pinCfg.data_out_num = I2S_PIN_NO_CHANGE;
    pinCfg.mck_io_num   = (config.mode == AudioInputMode::I2S_ADC)
                         ? config.pinMCLK : I2S_PIN_NO_CHANGE;

    err = i2s_set_pin(I2S_NUM_0, &pinCfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S set pin failed: %d", err);
        i2s_driver_uninstall(I2S_NUM_0);
        end();
        return AudioInputError::I2SSetPinFailed;
    }

    // Compute band bin ranges from sample rate and FFT size.
    // Start bins use floor, end bins use ceil to guarantee each band
    // covers at least one FFT bin (e.g. sub-bass at 44100/1024).
    {
        float binWidth = (float)config.sampleRate / config.fftSize;
        uint16_t maxBin = config.fftSize / 2;
        for (uint8_t i = 0; i < AUDIO_INPUT_NUM_BANDS; i++) {
            uint16_t startBin = (uint16_t)(BAND_FREQ[i] / binWidth);
            uint16_t endBin   = (uint16_t)ceilf(BAND_FREQ[i + 1] / binWidth);
            if (startBin < 1) startBin = 1;
            if (endBin > maxBin) endBin = maxBin;
            if (endBin <= startBin) endBin = startBin + 1;
            _impl->bandBins[i][0] = startBin;
            _impl->bandBins[i][1] = endBin;
        }
    }

    // Init beat detection state
    memset(_impl->energyHistory, 0, sizeof(_impl->energyHistory));
    _impl->energyIndex = 0;
    _impl->lastBeatMs  = 0;
    _impl->bpmEma      = 120.0f;
    memset(_impl->smoothedBands, 0, sizeof(_impl->smoothedBands));
    _impl->dcPrevX = 0.0f;
    _impl->dcPrevY = 0.0f;

    // Create spectrum queue (length 1, latest-wins)
    _impl->spectrumQueue = xQueueCreate(1, sizeof(AudioSpectrum));
    if (!_impl->spectrumQueue) {
        ESP_LOGE(TAG, "Queue create failed");
        i2s_driver_uninstall(I2S_NUM_0);
        end();
        return AudioInputError::TaskCreateFailed;
    }

    // Launch audio processing task
    _impl->running = true;
    uint8_t core = config.taskCore;
#if CONFIG_FREERTOS_UNICORE
    core = 0;
#endif
    BaseType_t taskOk = xTaskCreatePinnedToCore(
        audioTaskFunc,
        "audioInput",
        8192,
        _impl,
        config.taskPriority,
        &_impl->taskHandle,
        core
    );

    if (taskOk != pdPASS) {
        ESP_LOGE(TAG, "Task create failed");
        _impl->running = false;
        vQueueDelete(_impl->spectrumQueue);
        _impl->spectrumQueue = nullptr;
        i2s_driver_uninstall(I2S_NUM_0);
        end();
        return AudioInputError::TaskCreateFailed;
    }

    ESP_LOGI(TAG, "Started: %" PRIu32 " Hz, %u-pt FFT, core %u, pri %u",
             config.sampleRate, fftSize, core, config.taskPriority);

    return AudioInputError::Ok;
}

void AudioInput::end() {
    if (!_impl) return;

    if (_impl->running) {
        _impl->running = false;

        // Wait for task to signal exit (covers 200ms i2s_read timeout + processing)
        if (_impl->taskHandle && _impl->taskExitSem) {
            if (xSemaphoreTake(_impl->taskExitSem, pdMS_TO_TICKS(1000)) != pdTRUE) {
                // Task didn't exit cleanly. Stop I2S first to unblock any
                // pending i2s_read(), then force-delete the task.
                ESP_LOGW(TAG, "Task exit wait timed out, stopping I2S and force-deleting");
                i2s_stop(I2S_NUM_0);
                // Brief yield so the task can observe the stop and exit
                vTaskDelay(pdMS_TO_TICKS(50));
                if (xSemaphoreTake(_impl->taskExitSem, 0) != pdTRUE) {
                    vTaskDelete(_impl->taskHandle);
                }
            }
            _impl->taskHandle = nullptr;
        }

        i2s_driver_uninstall(I2S_NUM_0);
    }

    // Release ESP-DSP FFT tables (safe to call even if init wasn't reached)
    dsps_fft2r_deinit_fc32();

    if (_impl->spectrumQueue) {
        vQueueDelete(_impl->spectrumQueue);
        _impl->spectrumQueue = nullptr;
    }

    delete[] _impl->rawSamples;
    delete[] _impl->i2sReadBuf;
    delete[] _impl->fftData;
    delete[] _impl->hannWindow;
    delete[] _impl->magnitudes;

    _impl->rawSamples  = nullptr;
    _impl->i2sReadBuf  = nullptr;
    _impl->fftData     = nullptr;
    _impl->hannWindow  = nullptr;
    _impl->magnitudes  = nullptr;

    if (_impl->taskExitSem) {
        vSemaphoreDelete(_impl->taskExitSem);
        _impl->taskExitSem = nullptr;
    }

    delete _impl;
    _impl = nullptr;
}

bool AudioInput::getSpectrum(AudioSpectrum& out) const {
    if (!_impl || !_impl->spectrumQueue) return false;
    return xQueuePeek(_impl->spectrumQueue, &out, 0) == pdTRUE;
}

bool AudioInput::isRunning() const {
    return _impl && _impl->running;
}

uint32_t AudioInput::getStackHighWaterMark() const {
    if (!_impl || !_impl->taskHandle) return 0;
    return (uint32_t)uxTaskGetStackHighWaterMark(_impl->taskHandle);
}

#endif // ESP32
