# Audio Input and Sound-Reactive Lighting

I2S microphones, RCA audio input, ADC configuration, FFT processing with ESP-DSP, beat detection, and frequency band mapping.

## Audio Input Sources

### I2S Microphones (INMP441, SPH0645, ICS-43432)

- Connect via I2S interface (not I2C despite the confusing naming).
- ESP32 I2S pins are configurable. Use `i2s_driver_install()` + `i2s_set_pin()`.
- Sample at 44100 Hz for full audio spectrum or 22050 Hz for bass/mid only.
- I2S provides 24-bit samples in DMA buffers without CPU blocking.

| Microphone | Interface | Bits | SNR | Notes |
|-----------|-----------|------|-----|-------|
| INMP441 | I2S | 24-bit | 61 dB | Common, inexpensive, good for bass detection |
| SPH0645 | I2S | 24-bit | 65 dB | Adafruit breakout available, slightly better SNR |
| ICS-43432 | I2S | 24-bit | 65 dBA | Current-production TDK part. Flat frequency response, good all-around choice. Pin-compatible with the discontinued ICS-43434 |
| ICS-43434 | I2S | 24-bit | 64 dBA | **EOL** — TDK has discontinued this part. Existing boards work but may become harder to source. Replace with ICS-43432 for new builds |

All listed I2S mics use the same wiring and driver setup (BCLK, LRCLK/WS, DATA). The ICS-43432/43434 have a flatter frequency response across the audible range compared to the INMP441, which rolls off at higher frequencies. For beat detection (bass-heavy), any of them work well. For full-spectrum visualization, the SPH0645 or ICS-43432 are preferred.

### Microphone Placement and Vibration Isolation

MEMS microphones are sensitive to mechanical vibration as well as airborne sound. Without isolation, bass from nearby speakers or subwoofers transmits through the mounting surface and registers as low-frequency energy, corrupting beat detection.

- **Mount the mic on foam or rubber.** A small piece of open-cell foam or a rubber grommet between the breakout board and the enclosure decouples mechanical vibration. Even adhesive-backed foam tape helps.
- **Do not mount the mic inside or on a speaker enclosure.** Cabinet vibrations will dominate the signal. Place the mic at least 30 cm from any speaker, ideally on a separate surface.
- **Orient the sound port toward the room,** not against a wall or into the enclosure. Most MEMS breakout boards have a port hole on the top or bottom — check the specific board's datasheet.
- **Keep the mic away from airflow** (fans, HVAC vents). Moving air across the port creates broadband noise.

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

### Analog Microphone with Preamp

An analog electret or MEMS microphone (e.g., MAX4466 or MAX9814 breakout) can be connected to an ADC1 pin. These modules include a preamp that outputs an analog voltage already biased to mid-supply (VCC/2 for MAX4466, similar for MAX9814).

- Connect the output directly to an ADC1 pin (GPIO 32-39). **Do not add the RCA bias divider** — the breakout already biases its output, and a second divider will attenuate or double-bias the signal. Add an AC coupling capacitor (10 uF) only if the module's DC offset doesn't match your ADC midpoint.
- Gain is typically adjustable via a trim pot on the breakout board.
- Quality is lower than I2S microphones (limited by ESP32 ADC resolution and noise floor), but adequate for basic beat detection.
- Simpler wiring than I2S (single analog wire + power/GND).

Use I2S microphones (INMP441, SPH0645, ICS-43432) for anything beyond basic beat detection.

### External I2S ADC — PCM1808 (Line-Level Input)

The PCM1808 is a dedicated stereo ADC that outputs I2S digital audio. It provides significantly cleaner audio capture than the ESP32's built-in ADC, which has limited effective resolution and nonlinearity.

> **Do not connect speaker-level outputs** (amplifier outputs driving speakers) to the PCM1808 or any line-level ADC input. Speaker-level signals can be tens of volts and will damage the ADC. Use only line-level outputs (RCA, 3.5mm headphone jack, mixer send) as the audio source. If the only available source is speaker-level, use a dedicated speaker-to-line attenuator or DI box.

| Parameter | Value |
|-----------|-------|
| **Resolution** | 24-bit |
| **Sample rates** | 16-96 kHz |
| **Dynamic range** | 99 dB (typical, per datasheet) |
| **Interface** | I2S output (BCK, LRCK, DOUT) |
| **Input** | Differential or single-ended line-level |
| **Supply** | 5V analog (AVDD) + 3.3V digital (DVDD); breakout modules may include on-board regulators — check the specific module's schematic |

**Wiring to ESP32:**

```
PCM1808 BCK  ──── ESP32 GPIO (I2S BCLK)
PCM1808 LRCK ──── ESP32 GPIO (I2S LRCLK/WS)
PCM1808 DOUT ──── ESP32 GPIO (I2S DIN)
PCM1808 FMT  ──── GND (I2S mode)
PCM1808 SCK  ──── Master clock (see below)
```

The PCM1808 requires a master clock (SCK) at 256x, 384x, or 512x the sample rate (set via the MD0/MD1 mode pins; see datasheet Table 1). Options:
- Use the ESP32's `I2S_MODE_MASTER` to generate BCLK/LRCK and provide an external oscillator for SCK.
- Some breakout boards include an on-board oscillator (common: 24.576 MHz for 48/96 kHz, or 22.5792 MHz for 44.1/88.2 kHz).
- Check the specific module's documentation for clock and mode pin configuration.

**When to use PCM1808 vs built-in ADC:**
- **PCM1808:** When audio fidelity matters (full-spectrum analysis, precise BPM tracking, multi-band visualization). The 99 dB dynamic range vastly exceeds the ESP32's ~50 dB effective ADC performance.
- **Built-in ADC:** Adequate for simple beat detection (bass only) where the ESP32's ADC noise floor is acceptable. Simpler wiring, no extra hardware.

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

## BPM Tracking

Once beats are detected, derive a stable BPM estimate for predictive lighting effects (auto-advancing patterns, synchronized color changes, strobe timing).

### BPM Calculation from Beat Intervals

```cpp
#define MAX_INTERVALS 8

uint32_t beatTimes[MAX_INTERVALS];
uint8_t beatIdx = 0;
uint8_t beatCount = 0;
float smoothedBPM = 0;

void onBeatDetected() {
    uint32_t now = millis();
    beatTimes[beatIdx] = now;
    beatIdx = (beatIdx + 1) % MAX_INTERVALS;
    if (beatCount < MAX_INTERVALS) beatCount++;

    if (beatCount >= 2) {
        // Average interval over recent beats
        uint32_t totalInterval = 0;
        uint8_t intervals = 0;
        for (uint8_t i = 1; i < beatCount; i++) {
            uint8_t curr = (beatIdx - 1 - i + MAX_INTERVALS) % MAX_INTERVALS;
            uint8_t prev = (curr - 1 + MAX_INTERVALS) % MAX_INTERVALS;
            // Skipped: full ring buffer math; simplified here
        }
        // Simple: use last two beats
        uint8_t last = (beatIdx - 1 + MAX_INTERVALS) % MAX_INTERVALS;
        uint8_t prev = (beatIdx - 2 + MAX_INTERVALS) % MAX_INTERVALS;
        uint32_t interval = beatTimes[last] - beatTimes[prev];

        if (interval > 200 && interval < 2000) {  // 30-300 BPM range
            float instantBPM = 60000.0f / (float)interval;
            // Exponential moving average for stability
            const float alpha = 0.2f;
            smoothedBPM = (smoothedBPM == 0)
                ? instantBPM
                : smoothedBPM + alpha * (instantBPM - smoothedBPM);
        }
    }
}
```

### Beat Prediction

With a stable BPM estimate, predict when the next beat will occur:

```cpp
uint32_t lastBeatTime = 0;

uint32_t predictNextBeat() {
    if (smoothedBPM <= 0) return 0;
    uint32_t beatIntervalMs = (uint32_t)(60000.0f / smoothedBPM);
    return lastBeatTime + beatIntervalMs;
}

// In the LED update loop:
float beatPhase(uint32_t now) {
    if (smoothedBPM <= 0) return 0;
    uint32_t interval = (uint32_t)(60000.0f / smoothedBPM);
    uint32_t elapsed = now - lastBeatTime;
    return (float)(elapsed % interval) / (float)interval;  // 0.0 to 1.0
}
```

`beatPhase()` returns a 0.0-1.0 value representing position within the current beat cycle. Use this to drive continuous animations (pulsing, rotating patterns) that stay synchronized to the music.

## Beat Quantization

Beat quantization snaps user-triggered events (e.g., RF remote button presses, mode changes) to the nearest beat boundary. This makes transitions feel musically synchronized even when the trigger arrives slightly early or late.

### Why Quantize

- RF remotes add variable latency (estimated tens of milliseconds for encoding, transmission, and decoding; actual values depend on protocol and environment).
- Human button presses have ~50-100 ms of timing imprecision.
- Quantization absorbs this jitter by delaying the action to the next beat boundary.

### Implementation

```cpp
uint32_t quantizeToNextBeat(uint32_t triggerTime) {
    if (smoothedBPM <= 0) return triggerTime;  // No BPM, act immediately

    uint32_t interval = (uint32_t)(60000.0f / smoothedBPM);
    uint32_t elapsed = triggerTime - lastBeatTime;
    uint32_t posInBeat = elapsed % interval;

    // Snap to nearest beat boundary
    if (posInBeat < interval / 2) {
        // Closer to previous beat — trigger immediately (already past)
        return triggerTime;
    } else {
        // Closer to next beat — delay until next beat
        return triggerTime + (interval - posInBeat);
    }
}
```

### Quantization Options

| Strategy | Behavior | Use Case |
|----------|----------|----------|
| **Next beat** | Always wait for the next beat | Mode transitions, scene changes |
| **Nearest beat** | Snap to whichever boundary is closer | Color triggers, flash effects |
| **Immediate** | No quantization | Direct brightness control, power toggle |

For multi-zone systems, quantize all zone transitions to the same beat boundary so they appear synchronized.

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
