#pragma once

#include <stdint.h>
#include "rgbw.h"

struct AudioState;

struct EffectContext {
    uint32_t frameCount;
    uint32_t elapsedMs;
    const HSV* userColor;
    const AudioState* audio;
};

typedef void (*EffectFn)(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx);

enum class EffectMode : uint8_t {
    Solid = 0,
    RainbowCycle,
    RainbowWave,
    GradientSweep,
    Fire,
    SoundReactive,
    Spectrum,
    Twinkle,
    Breathing,
    COUNT
};

EffectFn getEffect(EffectMode mode);
EffectMode nextMode(EffectMode current);
const char* effectName(EffectMode mode);
