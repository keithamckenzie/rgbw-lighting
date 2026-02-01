# Audio Input and Sound-Reactive Lighting

I2S microphones, RCA audio input, ADC configuration, FFT processing with ESP-DSP, beat detection, and frequency band mapping.

## Audio Input Sources

### I2S Microphones (INMP441, SPH0645)

- Connect via I2S interface (not I2C despite the confusing naming).
- ESP32 I2S pins are configurable. Use `i2s_driver_install()` + `i2s_set_pin()`.
- Sample at 44100 Hz for full audio spectrum or 22050 Hz for bass/mid only.
- I2S provides 24-bit samples in DMA buffers without CPU blocking.

### I2C Microphones

- Some MEMS microphones (e.g., analog output with I2C config) use I2C for configuration only.
- Audio data comes through ADC or I2S, not the I2C bus.
- See [I2C Best Practices](esp32-internals.md#i2c-best-practices) for bus management.

### RCA Audio Input (Line-Level)

- **Use ADC1 pins only** (GPIO 32-39) since ADC2 is unavailable when WiFi is active.
- Consumer RCA line-level audio is ~0.9 Vpp nominal (-10 dBV) AC centered at 0V. Pro line-level (+4 dBu) is ~3.5 Vpp -- the bias circuit must handle either level or be designed for the expected source.
- **Required input circuit:** Bias the signal to Vcc/2 (1.65V) with a resistive voltage divider (2x 100k ohm), plus a DC blocking capacitor (10 uF) on the input.
- Configure ADC: 12-bit width, 11 dB attenuation (measurable up to ~3.9V, but accuracy is best in a narrower range; use the ESP-IDF ADC calibration API for reliable readings).
- For continuous sampling, use I2S in ADC mode for DMA-based reading (avoids CPU blocking).

#### RCA Input Circuit

```
RCA Signal ──┤├── 10uF DC block ──┬── GPIO 32 (ADC1_CH4)
                                    │
                                  100k
                                    │
                               3.3V ┤
                                    │
                                  100k
                                    │
                                   GND

Bias point: 3.3V / 2 = 1.65V
Input range: ~0.75V to ~2.55V (for 0.9 Vpp consumer line level)
```

### ADC Accuracy and Calibration (ESP32)

- ESP32 ADC reference voltage varies between chips (roughly 1000-1200 mV). Use the ESP-IDF ADC calibration driver to convert raw readings to calibrated millivolts (e.g., `adc_cali_raw_to_voltage()`) when amplitude accuracy matters.
- If noise is an issue, add a small (for example 100 nF) bypass capacitor close to the ADC input and keep wiring short.

## Continuous Audio Sampling (DMA)

### Legacy I2S ADC Mode (Arduino-ESP32 v2.x / ESP-IDF v4.4)

```cpp
#include <driver/i2s.h>
#include <driver/adc.h>

#define SAMPLE_RATE     44100
#define I2S_PORT        I2S_NUM_0
#define ADC_CHANNEL     ADC1_CHANNEL_4  // GPIO 32
#define DMA_BUF_COUNT   4               // Number of DMA buffers
#define DMA_BUF_LEN     1024            // Samples per buffer

void setupI2SADC() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_adc_mode(ADC_UNIT_1, ADC_CHANNEL);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
    i2s_adc_enable(I2S_PORT);
}
```

### ADC Continuous Driver (Arduino-ESP32 v3.x / ESP-IDF v5.x)

The legacy I2S ADC mode is not available in ESP-IDF v5+. Use the continuous ADC driver instead:

```cpp
#include <esp_adc/adc_continuous.h>

static adc_continuous_handle_t adc_handle = NULL;

void setupADCContinuous() {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 4096,
        .conv_frame_size = 1024,
    };
    adc_continuous_new_handle(&adc_config, &adc_handle);

    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = ADC_CHANNEL_4,   // GPIO 32
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    adc_continuous_config_t config = {
        .pattern_num = 1,
        .adc_pattern = &adc_pattern,
        .sample_freq_hz = 44100,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    adc_continuous_config(adc_handle, &config);
    adc_continuous_start(adc_handle);
}
```

### Buffer Sizing Guidelines

- `dma_buf_len`: Samples per buffer. Controls latency. At 44100 Hz, 1024 samples = 23.2 ms.
- `dma_buf_count`: Number of DMA buffers (ring buffer). Minimum 2 (double-buffering). Use 4 for safety.
- Total DMA memory: `buf_count * buf_len * bytes_per_sample`. For 4 * 1024 * 2 bytes = 8 KB.

### Reading and Converting Samples

```cpp
uint16_t raw_samples[DMA_BUF_LEN];
size_t bytes_read = 0;

// Blocks until a full buffer is available
i2s_read(I2S_PORT, raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);

// Convert to float and remove DC offset
// ESP32 I2S ADC mode: mask to 12-bit sample data
float samples[DMA_BUF_LEN];
for (int i = 0; i < DMA_BUF_LEN; i++) {
    samples[i] = (float)(raw_samples[i] & 0x0FFF) - 2048.0f;  // 12-bit, center at 0
}
```

The `& 0x0FFF` mask extracts the 12-bit ADC sample. The ESP32 ADC is 12-bit (0-4095), so only the lower 12 bits contain valid sample data. (Note: the I2S ADC legacy driver was removed in ESP-IDF v5; see the `adc_continuous` API for IDF v5+.)

### Sample Rate Selection

| Sample Rate | Nyquist Freq | FFT Resolution (1024-pt) | Use Case |
|-------------|-------------|--------------------------|----------|
| 22050 Hz | 11025 Hz | ~21.5 Hz/bin | Bass/mid only; saves CPU |
| 44100 Hz | 22050 Hz | ~43.1 Hz/bin | Full audio spectrum (standard) |
| 48000 Hz | 24000 Hz | ~46.9 Hz/bin | Matches some audio interfaces |

For LED sound-reactive lighting, 22050 Hz is often sufficient since LED response above 10 kHz is rarely perceptible. It halves DMA bandwidth and FFT computation.

## FFT Processing

### ESP-DSP (Recommended for ESP32)

ESP-DSP uses Xtensa SIMD instructions for hardware-accelerated FFT. Significantly faster than generic C implementations.

```cpp
#include "dsps_fft2r.h"
#include "dsps_wind_hann.h"

#define FFT_SIZE 1024

// Initialization (call once in setup)
dsps_fft2r_init_fc32(NULL, FFT_SIZE);

// Pre-compute window function
float wind[FFT_SIZE];
dsps_wind_hann_f32(wind, FFT_SIZE);

// Prepare interleaved complex array: [re0, im0, re1, im1, ...]
float fft_data[FFT_SIZE * 2];
for (int i = 0; i < FFT_SIZE; i++) {
    fft_data[i * 2]     = samples[i] * wind[i];  // real (windowed)
    fft_data[i * 2 + 1] = 0;                      // imaginary
}

// Execute FFT (in-place)
dsps_fft2r_fc32(fft_data, FFT_SIZE);
dsps_bit_rev_fc32(fft_data, FFT_SIZE);  // Required bit-reverse

// Convert to magnitude
for (int i = 0; i < FFT_SIZE / 2; i++) {
    float re = fft_data[i * 2];
    float im = fft_data[i * 2 + 1];
    float magnitude = sqrtf(re * re + im * im) / FFT_SIZE;
}

// Cleanup (call once when done)
dsps_fft2r_deinit_fc32();
```

**PlatformIO dependency:**
```ini
lib_deps =
    espressif/esp-dsp@^1.4.0
```

### Performance: ESP-DSP vs arduinoFFT

Approximate execution times on ESP32 (single core, 240 MHz). ESP-DSP values are derived from [Espressif's published benchmark tables](https://docs.espressif.com/projects/esp-dsp/en/latest/esp32/esp-dsp-benchmarks.html); arduinoFFT values are from community testing. Actual performance depends on compiler flags, cache state, and data alignment:

| FFT Size | ESP-DSP (Xtensa SIMD) | arduinoFFT (generic C++) | Speedup |
|----------|----------------------|--------------------------|---------|
| 256 | ~30-50 us | ~500-700 us | ~10-15x |
| 512 | ~70-110 us | ~1.2-1.5 ms | ~12-15x |
| 1024 | ~160-250 us | ~2.8-3.5 ms | ~14-17x |
| 2048 | ~380-550 us | ~6.5-8.0 ms | ~14-17x |

For a 1024-point FFT at 44100 Hz (23.2 ms per audio frame), ESP-DSP uses <250 us (about 1% of CPU time), leaving the CPU essentially free.

Use `arduinoFFT` only for AVR or host-side testing where ESP-DSP is unavailable.

### Window Functions

Always apply a window function before FFT to reduce spectral leakage:

| Window | Main Lobe Width | Side Lobe Level | Best For |
|--------|----------------|-----------------|----------|
| Hann | Moderate | -31 dB | General audio analysis (recommended default) |
| Blackman | Wide | -58 dB | Better frequency isolation |
| Blackman-Harris | Very wide | -92 dB | Precise frequency measurement |
| Flat-top | Widest | Varies | Amplitude accuracy |
| Rectangular (none) | Narrowest | -13 dB | Only for exactly periodic signals |

ESP-DSP provides: `dsps_wind_hann_f32`, `dsps_wind_blackman_f32`, `dsps_wind_blackman_harris_f32`, `dsps_wind_blackman_nuttall_f32`, `dsps_wind_nuttall_f32`, `dsps_wind_flat_top_f32`.

### Frequency Resolution

- Bin frequency: `bin_index * sample_rate / FFT_SIZE`
- 512 samples at 44100 Hz: ~86 Hz resolution
- 1024 samples at 44100 Hz: ~43 Hz resolution
- Process FFT in a dedicated FreeRTOS task pinned to Core 1.

## Frequency Band Mapping

Map FFT output to RGBW channels for visual effect:

### Simple 3-Band Mapping

```
Bass  (20-250 Hz)    -> Red
Mids  (250-2000 Hz)  -> Green
Highs (2000-16000 Hz)-> Blue
White                 -> Overall energy or beat pulse
```

### Octave-Based 8-Band Mapping

For more detailed visualization:

```cpp
// Bin = freq / (sampleRate / FFT_SIZE)
// At 44100 Hz, 1024-pt FFT: each bin = 43.07 Hz
struct FreqBand {
    uint16_t startBin;
    uint16_t endBin;
    float    smoothed;  // exponential moving average
};

FreqBand bands[] = {
    {1,   2,   0},  // ~43-86 Hz     (sub-bass)
    {2,   4,   0},  // ~86-172 Hz    (bass)
    {4,   8,   0},  // ~172-345 Hz   (low-mid)
    {8,   16,  0},  // ~345-689 Hz   (mid)
    {16,  32,  0},  // ~689-1378 Hz  (upper-mid)
    {32,  64,  0},  // ~1378-2756 Hz (presence)
    {64,  128, 0},  // ~2756-5512 Hz (brilliance)
    {128, 256, 0},  // ~5512-11025 Hz (air)
};
```

### Smoothing

Apply exponential moving average to reduce flicker:

```cpp
const float SMOOTH_RISE = 0.3f;   // Fast attack
const float SMOOTH_FALL = 0.05f;  // Slow decay

for (auto& band : bands) {
    float energy = computeBandEnergy(band.startBin, band.endBin, magnitudes);
    float alpha = (energy > band.smoothed) ? SMOOTH_RISE : SMOOTH_FALL;
    band.smoothed += alpha * (energy - band.smoothed);
}
```

## Beat Detection

### Energy-Based Algorithm

Track energy in the bass band (60-250 Hz) over a sliding window:

```cpp
#define HISTORY_SIZE 43  // ~1 second at 43 FPS

float energyHistory[HISTORY_SIZE];
int historyIdx = 0;

bool detectBeat(float currentBassEnergy) {
    // Compute running average
    float avg = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) avg += energyHistory[i];
    avg /= HISTORY_SIZE;

    // Store current
    energyHistory[historyIdx] = currentBassEnergy;
    historyIdx = (historyIdx + 1) % HISTORY_SIZE;

    // Beat if energy exceeds threshold * average
    float threshold = 1.5f;  // Tune: 1.3 (sensitive) to 2.0 (selective)
    return currentBassEnergy > threshold * avg && avg > 0.01f;
}
```

### VU-Meter Style Response

Apply exponential decay to peak values for smooth VU-meter behavior:

```cpp
float peak = 0;
const float DECAY = 0.95f;  // Per frame

void updateVU(float currentLevel) {
    if (currentLevel > peak) {
        peak = currentLevel;  // Instant attack
    } else {
        peak *= DECAY;        // Slow decay
    }
    // Map peak to brightness: uint8_t brightness = peak * 255;
}
```

### Beat Detection Tuning

- **Threshold (1.3-2.0):** Lower = more beats detected (good for electronic music), higher = only strong beats (good for varied music).
- **History window:** Shorter (0.5s) = responsive to tempo changes, longer (2s) = stable for consistent tempo.
- **Minimum interval:** Enforce minimum 200ms between beats to prevent double-triggering on kick drum transients.
- **DC blocking:** Remove any DC offset from audio samples before analysis. A simple high-pass filter (subtract running average) works.

### DC Blocker

```cpp
// Simple single-pole DC blocker
float dcBlocker(float input) {
    static float xPrev = 0, yPrev = 0;
    const float R = 0.995f;  // Pole near unit circle = very low cutoff
    float output = input - xPrev + R * yPrev;
    xPrev = input;
    yPrev = output;
    return output;
}
```

## Task Architecture for Audio-Reactive System

```
Audio Task (Core 1, priority 4, 8192 stack):
    while (true) {
        i2s_read(...)          // Block until DMA buffer ready
        applyWindow(samples)   // Hann window
        runFFT(samples)        // ESP-DSP, ~250 us
        computeBands(magnitudes, bands)
        detectBeat(bassEnergy)
        xQueueOverwrite(spectrumQueue, &spectrum)  // Latest wins
    }

LED Task (Core 1, priority 3, 4096 stack):
    while (true) {
        AudioSpectrum spectrum;
        if (xQueueReceive(spectrumQueue, &spectrum, 0) == pdTRUE) {
            mapSpectrumToColors(spectrum, pixels)
        }
        strip.show()
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(16))  // ~60 FPS
    }
```

### RAM Budget for Audio System

| Buffer | Size |
|--------|------|
| I2S DMA (4 x 1024 x 2B) | 8 KB |
| FFT input (1024 x 2 x 4B) | 8 KB |
| FFT window (1024 x 4B) | 4 KB |
| Band smoothing | ~64 B |
| Beat history (43 x 4B) | ~172 B |
| **Total** | **~20 KB** |
