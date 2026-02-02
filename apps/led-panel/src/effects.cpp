#include "effects.h"
#include "matrix.h"
#include "audio.h"

// --- Pseudo-random (xorshift32) for fire/twinkle ---
static uint32_t s_rngState = 12345;
static uint8_t rng8() {
    s_rngState ^= s_rngState << 13;
    s_rngState ^= s_rngState >> 17;
    s_rngState ^= s_rngState << 5;
    return (uint8_t)(s_rngState & 0xFF);
}

// --- Sine approximation (0-255 input, 0-255 output) ---
// Quarter-wave LUT, 64 entries
static const uint8_t s_sineTable[64] = {
      0,   6,  12,  19,  25,  31,  37,  44,
     50,  56,  62,  68,  74,  80,  86,  92,
     98, 103, 109, 115, 120, 126, 131, 136,
    142, 147, 152, 157, 162, 167, 171, 176,
    181, 185, 189, 193, 197, 201, 205, 209,
    212, 216, 219, 222, 225, 228, 231, 234,
    236, 238, 241, 243, 244, 246, 248, 249,
    251, 252, 253, 254, 254, 255, 255, 255
};

static uint8_t sin8(uint8_t theta) {
    uint8_t idx = theta & 0x3F;  // lower 6 bits = index
    uint8_t quadrant = (theta >> 6) & 0x03;
    uint8_t val;
    switch (quadrant) {
        case 0: val = s_sineTable[idx]; break;
        case 1: val = s_sineTable[63 - idx]; break;
        case 2: val = 255 - s_sineTable[idx]; break;
        default: val = 255 - s_sineTable[63 - idx]; break;
    }
    return val;
}

// ====================================================================
// Effect 1: Solid — slowly rotating hue, fills entire panel
// ====================================================================
static void effectSolid(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    uint16_t hue = (ctx.elapsedMs / 50) % 360;
    HSV hsv = { hue, 255, 200 };
    RGBW color = hsvToRGBW(hsv);
    for (uint16_t i = 0; i < numPixels; i++) {
        buffer[i] = color;
    }
}

// ====================================================================
// Effect 2: Rainbow Cycle — all pixels same hue, cycling through spectrum
// ====================================================================
static void effectRainbowCycle(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    uint16_t hue = (ctx.elapsedMs / 20) % 360;
    HSV hsv = { hue, 255, 255 };
    RGBW color = hsvToRGBW(hsv);
    for (uint16_t i = 0; i < numPixels; i++) {
        buffer[i] = color;
    }
}

// ====================================================================
// Effect 3: Rainbow Wave — horizontal gradient with time shift
// ====================================================================
static void effectRainbowWave(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    uint16_t timeOffset = (ctx.elapsedMs / 30) % 360;
    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            uint16_t hue = (x * 360 / PANEL_WIDTH + timeOffset) % 360;
            HSV hsv = { hue, 255, 255 };
            RGBW color = hsvToRGBW(hsv);
            uint16_t idx = mapXY(x, y);
            if (idx < numPixels) {
                buffer[idx] = color;
            }
        }
    }
}

// ====================================================================
// Effect 4: Gradient Sweep — two-color gradient sweeping vertically
// ====================================================================
static void effectGradientSweep(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    // Slowly rotate between color pairs
    uint16_t hue1 = (ctx.elapsedMs / 40) % 360;
    uint16_t hue2 = (hue1 + 150) % 360;
    HSV hsv1 = { hue1, 255, 255 };
    HSV hsv2 = { hue2, 255, 255 };
    RGBW color1 = hsvToRGBW(hsv1);
    RGBW color2 = hsvToRGBW(hsv2);

    // Animate the sweep position
    uint8_t phase = sin8((uint8_t)(ctx.elapsedMs / 15));
    int16_t offset = (int16_t)phase - 128;  // -128 to +127

    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        // t sweeps from 0.0 to 1.0 across height, shifted by animated offset
        int16_t shifted = (int16_t)((PANEL_HEIGHT > 1) ? (y * 255 / (PANEL_HEIGHT - 1)) : 0) + offset;
        float t;
        if (shifted <= 0) t = 0.0f;
        else if (shifted >= 255) t = 1.0f;
        else t = shifted / 255.0f;

        RGBW rowColor = lerpRGBW(color1, color2, t);
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            uint16_t idx = mapXY(x, y);
            if (idx < numPixels) {
                buffer[idx] = rowColor;
            }
        }
    }
}

// ====================================================================
// Effect 5: Fire — bottom-up heat simulation
// ====================================================================
static uint8_t s_heat[NUM_PIXELS];

static RGBW heatToColor(uint8_t temperature) {
    // Map temperature to black -> red -> yellow -> white(W)
    if (temperature < 85) {
        // black to red
        uint8_t r = temperature * 3;
        return RGBW(r, 0, 0, 0);
    } else if (temperature < 170) {
        uint8_t t = temperature - 85;
        // red to yellow
        return RGBW(255, t * 3, 0, 0);
    } else {
        uint8_t t = temperature - 170;
        // yellow to white (bring up W channel)
        return RGBW(255, 255, t * 3, t * 3);
    }
}

static void effectFire(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    (void)ctx;

    // Step 1: Cool down every cell a little
    for (uint16_t i = 0; i < numPixels; i++) {
        uint8_t cooldown = rng8() % 20;
        s_heat[i] = (s_heat[i] > cooldown) ? s_heat[i] - cooldown : 0;
    }

    // Step 2: Heat rises — diffuse upward (y decreases = upward)
    for (uint16_t y = 0; y < PANEL_HEIGHT - 1; y++) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            uint16_t thisIdx = mapXY(x, y);
            uint16_t belowIdx = mapXY(x, y + 1);
            // Average with neighbors below
            uint16_t belowLeft = (x > 0) ? mapXY(x - 1, y + 1) : belowIdx;
            uint16_t belowRight = (x < PANEL_WIDTH - 1) ? mapXY(x + 1, y + 1) : belowIdx;
            if (thisIdx < numPixels && belowIdx < numPixels &&
                belowLeft < numPixels && belowRight < numPixels) {
                s_heat[thisIdx] = ((uint16_t)s_heat[belowLeft] +
                                   (uint16_t)s_heat[belowIdx] +
                                   (uint16_t)s_heat[belowRight]) / 3;
            }
        }
    }

    // Step 3: Ignite new sparks at the bottom row
    for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
        if (rng8() < 80) {
            uint16_t idx = mapXY(x, PANEL_HEIGHT - 1);
            if (idx < numPixels) {
                s_heat[idx] = 200 + rng8() % 56;
            }
        }
    }

    // Step 4: Map heat to color
    for (uint16_t i = 0; i < numPixels; i++) {
        buffer[i] = heatToColor(s_heat[i]);
    }
}

// ====================================================================
// Effect 6: Sound Reactive (ESP32 only)
// ====================================================================
#if AUDIO_ENABLED
static void effectSoundReactive(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    const AudioState* audio = ctx.audio;

    float bass   = 0.0f;
    float mids   = 0.0f;
    float highs  = 0.0f;
    float energy = 0.0f;
    bool  beat   = false;

    if (audio) {
        // Bass: bands 0+1 (sub-bass + bass, ~43-172 Hz)
        bass = (audio->bandEnergy[0] + audio->bandEnergy[1]) * 0.5f;
        // Mids: bands 2+3+4 (low-mid through upper-mid, ~172-1378 Hz)
        mids = (audio->bandEnergy[2] + audio->bandEnergy[3] + audio->bandEnergy[4]) / 3.0f;
        // Highs: bands 5+6+7 (presence through air, ~1378-11025 Hz)
        highs = (audio->bandEnergy[5] + audio->bandEnergy[6] + audio->bandEnergy[7]) / 3.0f;
        energy = audio->energy;
        beat = audio->beatDetected;
    }

    // Map frequency bands to RGB channels
    uint8_t r = (uint8_t)(bass  * 255);
    uint8_t g = (uint8_t)(mids  * 255);
    uint8_t b = (uint8_t)(highs * 255);
    uint8_t w = (uint8_t)(energy * 80);  // subtle white from overall energy

    RGBW color = RGBW(r, g, b, w);

    for (uint16_t i = 0; i < numPixels; i++) {
        buffer[i] = color;
    }

    // Beat flash: white burst
    if (beat) {
        for (uint16_t i = 0; i < numPixels; i++) {
            buffer[i].w = 255;
        }
    }
}
#endif

// ====================================================================
// Effect 7: Spectrum — per-pixel frequency band visualization
// ====================================================================
#if AUDIO_ENABLED

// Band colors: sub-bass through air, warm-to-cool gradient
static const RGBW s_bandColors[AUDIO_NUM_BANDS] = {
    RGBW(255,   0,   0,   0),  // Band 0: Sub-bass  — red
    RGBW(255,  80,   0,   0),  // Band 1: Bass      — orange
    RGBW(255, 200,   0,   0),  // Band 2: Low-mid   — yellow
    RGBW(  0, 255,   0,   0),  // Band 3: Mid       — green
    RGBW(  0, 200, 255,   0),  // Band 4: Upper-mid — cyan
    RGBW(  0,  80, 255,   0),  // Band 5: Presence  — blue
    RGBW(120,   0, 255,   0),  // Band 6: Brilliance— purple
    RGBW(255,   0, 200,   0),  // Band 7: Air       — magenta
};

static void effectSpectrum(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    const AudioState* audio = ctx.audio;

    // Clear buffer
    for (uint16_t i = 0; i < numPixels; i++) {
        buffer[i] = RGBW(0, 0, 0, 0);
    }

    // Divide panel width into 8 band columns
    uint16_t colsPerBand = PANEL_WIDTH / AUDIO_NUM_BANDS;
    if (colsPerBand < 1) colsPerBand = 1;

    for (uint8_t band = 0; band < AUDIO_NUM_BANDS; band++) {
        float energy = 0.0f;
        if (audio) {
            energy = audio->bandEnergy[band];
        }

        // Bar height proportional to energy (bottom-up)
        uint16_t barHeight = (uint16_t)(energy * PANEL_HEIGHT);
        if (barHeight > PANEL_HEIGHT) barHeight = PANEL_HEIGHT;

        RGBW color = s_bandColors[band];
        uint16_t xStart = band * colsPerBand;
        uint16_t xEnd = xStart + colsPerBand;
        if (band == AUDIO_NUM_BANDS - 1) {
            xEnd = PANEL_WIDTH;  // last band gets remaining columns
        }

        for (uint16_t x = xStart; x < xEnd; x++) {
            for (uint16_t barY = 0; barY < barHeight; barY++) {
                // Bottom-up: y=0 is top, so filled from PANEL_HEIGHT-1 upward
                uint16_t y = PANEL_HEIGHT - 1 - barY;
                uint16_t idx = mapXY(x, y);
                if (idx < numPixels) {
                    // Dim toward the top of the bar for gradient feel
                    uint8_t intensity = 255 - (uint8_t)((uint16_t)barY * 128 / PANEL_HEIGHT);
                    buffer[idx] = RGBW(
                        (uint8_t)((uint16_t)color.r * intensity / 255),
                        (uint8_t)((uint16_t)color.g * intensity / 255),
                        (uint8_t)((uint16_t)color.b * intensity / 255),
                        0
                    );
                }
            }
        }
    }

    // Beat flash: brief white on the peak row
    if (audio && audio->beatDetected) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            uint16_t idx = mapXY(x, 0);
            if (idx < numPixels) {
                buffer[idx].w = 180;
            }
        }
    }
}
#endif

// ====================================================================
// Effect 8: Twinkle — random pixels spark and fade
// ====================================================================
static uint8_t s_sparkState[NUM_PIXELS];

static void effectTwinkle(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    (void)ctx;

    // Fade all sparks
    for (uint16_t i = 0; i < numPixels; i++) {
        if (s_sparkState[i] > 4) {
            s_sparkState[i] -= 4;
        } else {
            s_sparkState[i] = 0;
        }
    }

    // Ignite new sparks (about 5 per frame on a 864-pixel panel)
    for (uint8_t i = 0; i < 5; i++) {
        uint16_t idx = ((uint16_t)rng8() << 8 | rng8()) % numPixels;
        s_sparkState[idx] = 255;
    }

    // Map spark state to warm white
    for (uint16_t i = 0; i < numPixels; i++) {
        uint8_t v = s_sparkState[i];
        buffer[i] = RGBW(v / 4, v / 6, 0, v);
    }
}

// ====================================================================
// Effect 8: Breathing — sinusoidal brightness modulation
// ====================================================================
static void effectBreathing(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    // ~3s period = 333ms per cycle of sin8 input
    uint8_t phase = (uint8_t)(ctx.elapsedMs / 12);
    uint8_t val = sin8(phase);

    // Warm white with modulated brightness
    RGBW color = RGBW(val / 5, val / 8, 0, val);
    for (uint16_t i = 0; i < numPixels; i++) {
        buffer[i] = color;
    }
}

// ====================================================================
// Effect table and mode management
// ====================================================================
static const EffectFn s_effectTable[] = {
    effectSolid,
    effectRainbowCycle,
    effectRainbowWave,
    effectGradientSweep,
    effectFire,
#if AUDIO_ENABLED
    effectSoundReactive,
    effectSpectrum,
#else
    effectSolid,  // stub: SoundReactive falls back to Solid when audio disabled
    effectSolid,  // stub: Spectrum falls back to Solid when audio disabled
#endif
    effectTwinkle,
    effectBreathing,
};

EffectFn getEffect(EffectMode mode) {
    uint8_t idx = (uint8_t)mode;
    if (idx >= (uint8_t)EffectMode::COUNT) {
        return effectSolid;
    }
    return s_effectTable[idx];
}

EffectMode nextMode(EffectMode current) {
    uint8_t next = (uint8_t)current + 1;
#if !AUDIO_ENABLED
    // Skip audio effects when audio is disabled
    if (next == (uint8_t)EffectMode::SoundReactive) {
        next = (uint8_t)EffectMode::Twinkle;  // jump past SoundReactive + Spectrum
    }
#endif
    if (next >= (uint8_t)EffectMode::COUNT) {
        next = 0;
    }
    return (EffectMode)next;
}

static const char* const s_effectNames[] = {
    "Solid",
    "RainbowCycle",
    "RainbowWave",
    "GradientSweep",
    "Fire",
#if AUDIO_ENABLED
    "SoundReactive",
    "Spectrum",
#else
    "SoundReactive (off)",
    "Spectrum (off)",
#endif
    "Twinkle",
    "Breathing",
};

const char* effectName(EffectMode mode) {
    uint8_t idx = (uint8_t)mode;
    if (idx >= (uint8_t)EffectMode::COUNT) {
        return "Unknown";
    }
    return s_effectNames[idx];
}
