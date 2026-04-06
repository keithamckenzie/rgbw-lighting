#include "effects.h"

#include "audio.h"
#include "matrix.h"
#include "state.h"

namespace {

uint32_t s_rngState = 12345;

uint8_t rng8() {
    s_rngState ^= s_rngState << 13;
    s_rngState ^= s_rngState >> 17;
    s_rngState ^= s_rngState << 5;
    return static_cast<uint8_t>(s_rngState & 0xFFU);
}

const uint8_t s_sineTable[64] = {
      0,   6,  12,  19,  25,  31,  37,  44,
     50,  56,  62,  68,  74,  80,  86,  92,
     98, 103, 109, 115, 120, 126, 131, 136,
    142, 147, 152, 157, 162, 167, 171, 176,
    181, 185, 189, 193, 197, 201, 205, 209,
    212, 216, 219, 222, 225, 228, 231, 234,
    236, 238, 241, 243, 244, 246, 248, 249,
    251, 252, 253, 254, 254, 255, 255, 255
};

uint8_t sin8(uint8_t theta) {
    const uint8_t index = theta & 0x3FU;
    const uint8_t quadrant = (theta >> 6) & 0x03U;

    switch (quadrant) {
        case 0:
            return s_sineTable[index];
        case 1:
            return s_sineTable[63U - index];
        case 2:
            return static_cast<uint8_t>(255U - s_sineTable[index]);
        default:
            return static_cast<uint8_t>(255U - s_sineTable[63U - index]);
    }
}

void effectSolid(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    const HSV color = (ctx.userColor != nullptr)
        ? normalizeHSV(*ctx.userColor)
        : HSV{0, 0, 255};
    const RGBW rgbw = hsvToRGBW(color);

    for (uint16_t index = 0; index < numPixels; index++) {
        buffer[index] = rgbw;
    }
}

void effectRainbowCycle(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    const uint16_t hue = static_cast<uint16_t>((ctx.elapsedMs / 20U) % 360U);
    const RGBW color = hsvToRGBW(HSV{hue, 255, 255});

    for (uint16_t index = 0; index < numPixels; index++) {
        buffer[index] = color;
    }
}

void effectRainbowWave(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    const uint16_t timeOffset = static_cast<uint16_t>((ctx.elapsedMs / 30U) % 360U);

    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            const uint16_t hue = static_cast<uint16_t>((x * 360U / PANEL_WIDTH + timeOffset) % 360U);
            const uint16_t index = mapXY(x, y);
            if (index < numPixels) {
                buffer[index] = hsvToRGBW(HSV{hue, 255, 255});
            }
        }
    }
}

void effectGradientSweep(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    const uint16_t hue1 = static_cast<uint16_t>((ctx.elapsedMs / 40U) % 360U);
    const uint16_t hue2 = static_cast<uint16_t>((hue1 + 150U) % 360U);
    const RGBW color1 = hsvToRGBW(HSV{hue1, 255, 255});
    const RGBW color2 = hsvToRGBW(HSV{hue2, 255, 255});

    const uint8_t phase = sin8(static_cast<uint8_t>(ctx.elapsedMs / 15U));
    const int16_t offset = static_cast<int16_t>(phase) - 128;

    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        const int16_t shifted = static_cast<int16_t>(
            ((PANEL_HEIGHT > 1U) ? (y * 255U / (PANEL_HEIGHT - 1U)) : 0U)
        ) + offset;

        float t = 0.0f;
        if (shifted >= 255) {
            t = 1.0f;
        } else if (shifted > 0) {
            t = shifted / 255.0f;
        }

        const RGBW rowColor = lerpRGBW(color1, color2, t);
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            const uint16_t index = mapXY(x, y);
            if (index < numPixels) {
                buffer[index] = rowColor;
            }
        }
    }
}

uint8_t s_heat[NUM_PIXELS];

RGBW heatToColor(uint8_t temperature) {
    if (temperature < 85U) {
        return RGBW(static_cast<uint8_t>(temperature * 3U), 0, 0, 0);
    }

    if (temperature < 170U) {
        const uint8_t t = static_cast<uint8_t>(temperature - 85U);
        return RGBW(255, static_cast<uint8_t>(t * 3U), 0, 0);
    }

    const uint8_t t = static_cast<uint8_t>(temperature - 170U);
    return RGBW(255, 255, static_cast<uint8_t>(t * 3U), static_cast<uint8_t>(t * 3U));
}

void effectFire(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    (void)ctx;

    for (uint16_t index = 0; index < numPixels; index++) {
        const uint8_t cooldown = static_cast<uint8_t>(rng8() % 20U);
        s_heat[index] = (s_heat[index] > cooldown) ? static_cast<uint8_t>(s_heat[index] - cooldown) : 0;
    }

    for (uint16_t y = 0; y < (PANEL_HEIGHT - 1U); y++) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            const uint16_t current = mapXY(x, y);
            const uint16_t below = mapXY(x, static_cast<uint16_t>(y + 1U));
            const uint16_t belowLeft = (x > 0U) ? mapXY(static_cast<uint16_t>(x - 1U), static_cast<uint16_t>(y + 1U)) : below;
            const uint16_t belowRight = (x < (PANEL_WIDTH - 1U))
                ? mapXY(static_cast<uint16_t>(x + 1U), static_cast<uint16_t>(y + 1U))
                : below;

            if (current < numPixels && below < numPixels && belowLeft < numPixels && belowRight < numPixels) {
                s_heat[current] = static_cast<uint8_t>(
                    (static_cast<uint16_t>(s_heat[belowLeft]) +
                     static_cast<uint16_t>(s_heat[below]) +
                     static_cast<uint16_t>(s_heat[belowRight])) / 3U
                );
            }
        }
    }

    for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
        if (rng8() < 80U) {
            const uint16_t index = mapXY(x, static_cast<uint16_t>(PANEL_HEIGHT - 1U));
            if (index < numPixels) {
                s_heat[index] = static_cast<uint8_t>(200U + (rng8() % 56U));
            }
        }
    }

    for (uint16_t index = 0; index < numPixels; index++) {
        buffer[index] = heatToColor(s_heat[index]);
    }
}

#if AUDIO_ENABLED

void effectSoundReactive(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    float bass = 0.0f;
    float mids = 0.0f;
    float highs = 0.0f;
    float energy = 0.0f;
    bool beat = false;

    if (ctx.audio != nullptr) {
        bass = (ctx.audio->bandEnergy[0] + ctx.audio->bandEnergy[1]) * 0.5f;
        mids = (ctx.audio->bandEnergy[2] + ctx.audio->bandEnergy[3] + ctx.audio->bandEnergy[4]) / 3.0f;
        highs = (ctx.audio->bandEnergy[5] + ctx.audio->bandEnergy[6] + ctx.audio->bandEnergy[7]) / 3.0f;
        energy = ctx.audio->energy;
        beat = ctx.audio->beatDetected;
    }

    RGBW color(
        static_cast<uint8_t>(bass * 255.0f),
        static_cast<uint8_t>(mids * 255.0f),
        static_cast<uint8_t>(highs * 255.0f),
        static_cast<uint8_t>(energy * 80.0f)
    );

    for (uint16_t index = 0; index < numPixels; index++) {
        buffer[index] = color;
    }

    if (beat) {
        for (uint16_t index = 0; index < numPixels; index++) {
            buffer[index].w = 255;
        }
    }
}

const RGBW s_bandColors[AUDIO_NUM_BANDS] = {
    RGBW(255,   0,   0,   0),
    RGBW(255,  80,   0,   0),
    RGBW(255, 200,   0,   0),
    RGBW(  0, 255,   0,   0),
    RGBW(  0, 200, 255,   0),
    RGBW(  0,  80, 255,   0),
    RGBW(120,   0, 255,   0),
    RGBW(255,   0, 200,   0),
};

void effectSpectrum(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    for (uint16_t index = 0; index < numPixels; index++) {
        buffer[index] = RGBW(0, 0, 0, 0);
    }

    uint16_t columnsPerBand = static_cast<uint16_t>(PANEL_WIDTH / AUDIO_NUM_BANDS);
    if (columnsPerBand < 1U) {
        columnsPerBand = 1U;
    }

    for (uint8_t band = 0; band < AUDIO_NUM_BANDS; band++) {
        const float energy = (ctx.audio != nullptr) ? ctx.audio->bandEnergy[band] : 0.0f;
        uint16_t barHeight = static_cast<uint16_t>(energy * PANEL_HEIGHT);
        if (barHeight > PANEL_HEIGHT) {
            barHeight = PANEL_HEIGHT;
        }

        const RGBW color = s_bandColors[band];
        const uint16_t xStart = static_cast<uint16_t>(band * columnsPerBand);
        uint16_t xEnd = static_cast<uint16_t>(xStart + columnsPerBand);
        if (band == (AUDIO_NUM_BANDS - 1U)) {
            xEnd = PANEL_WIDTH;
        }

        for (uint16_t x = xStart; x < xEnd; x++) {
            for (uint16_t barY = 0; barY < barHeight; barY++) {
                const uint16_t y = static_cast<uint16_t>(PANEL_HEIGHT - 1U - barY);
                const uint16_t index = mapXY(x, y);
                if (index < numPixels) {
                    const uint8_t intensity = static_cast<uint8_t>(
                        255U - ((static_cast<uint16_t>(barY) * 128U) / PANEL_HEIGHT)
                    );
                    buffer[index] = RGBW(
                        static_cast<uint8_t>((static_cast<uint16_t>(color.r) * intensity) / 255U),
                        static_cast<uint8_t>((static_cast<uint16_t>(color.g) * intensity) / 255U),
                        static_cast<uint8_t>((static_cast<uint16_t>(color.b) * intensity) / 255U),
                        0
                    );
                }
            }
        }
    }

    if (ctx.audio != nullptr && ctx.audio->beatDetected) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            const uint16_t index = mapXY(x, 0);
            if (index < numPixels) {
                buffer[index].w = 180;
            }
        }
    }
}

#endif

uint8_t s_sparkState[NUM_PIXELS];

void effectTwinkle(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    (void)ctx;

    for (uint16_t index = 0; index < numPixels; index++) {
        if (s_sparkState[index] > 4U) {
            s_sparkState[index] = static_cast<uint8_t>(s_sparkState[index] - 4U);
        } else {
            s_sparkState[index] = 0;
        }
    }

    for (uint8_t spark = 0; spark < 5U; spark++) {
        const uint16_t index = static_cast<uint16_t>(
            ((static_cast<uint16_t>(rng8()) << 8) | rng8()) % numPixels
        );
        s_sparkState[index] = 255;
    }

    for (uint16_t index = 0; index < numPixels; index++) {
        const uint8_t value = s_sparkState[index];
        buffer[index] = RGBW(
            static_cast<uint8_t>(value / 4U),
            static_cast<uint8_t>(value / 6U),
            0,
            value
        );
    }
}

void effectBreathing(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx) {
    const uint8_t phase = static_cast<uint8_t>(ctx.elapsedMs / 12U);
    const uint8_t value = sin8(phase);
    const RGBW color(
        static_cast<uint8_t>(value / 5U),
        static_cast<uint8_t>(value / 8U),
        0,
        value
    );

    for (uint16_t index = 0; index < numPixels; index++) {
        buffer[index] = color;
    }
}

const EffectFn s_effectTable[] = {
    effectSolid,
    effectRainbowCycle,
    effectRainbowWave,
    effectGradientSweep,
    effectFire,
#if AUDIO_ENABLED
    effectSoundReactive,
    effectSpectrum,
#else
    effectSolid,
    effectSolid,
#endif
    effectTwinkle,
    effectBreathing,
};

const char* const s_effectNames[] = {
    "Solid",
    "Rainbow Cycle",
    "Rainbow Wave",
    "Gradient Sweep",
    "Fire",
#if AUDIO_ENABLED
    "Sound Reactive",
    "Spectrum",
#else
    "Sound Reactive (off)",
    "Spectrum (off)",
#endif
    "Twinkle",
    "Breathing",
};

}  // namespace

EffectFn getEffect(EffectMode mode) {
    const uint8_t index = static_cast<uint8_t>(mode);
    if (index >= static_cast<uint8_t>(EffectMode::COUNT)) {
        return effectSolid;
    }
    return s_effectTable[index];
}

EffectMode nextMode(EffectMode current) {
    if (current >= EffectMode::COUNT) {
        return EffectMode::Solid;
    }

    uint8_t next = static_cast<uint8_t>(current) + 1U;
#if !AUDIO_ENABLED
    if (next == static_cast<uint8_t>(EffectMode::SoundReactive)) {
        next = static_cast<uint8_t>(EffectMode::Twinkle);
    }
#endif
    if (next >= static_cast<uint8_t>(EffectMode::COUNT)) {
        next = 0;
    }
    return static_cast<EffectMode>(next);
}

const char* effectName(EffectMode mode) {
    const uint8_t index = static_cast<uint8_t>(mode);
    if (index >= static_cast<uint8_t>(EffectMode::COUNT)) {
        return "Unknown";
    }
    return s_effectNames[index];
}
