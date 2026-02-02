#pragma once

#include <stdint.h>
#include "config.h"
#include "rgbw.h"

struct AudioState;  // forward declaration

struct EffectContext {
    uint32_t frameCount;
    uint32_t elapsedMs;
    uint8_t  brightness;    // 0-255, from pot
    const AudioState* audio;
};

// Effect function signature
typedef void (*EffectFn)(RGBW* buffer, uint16_t numPixels, const EffectContext& ctx);

// Effect mode enum — values are stable regardless of AUDIO_ENABLED
enum class EffectMode : uint8_t {
    Solid = 0,
    RainbowCycle,
    RainbowWave,
    GradientSweep,
    Fire,
    SoundReactive,  // always present for stable enum values
    Spectrum,       // per-pixel frequency band visualization
    Twinkle,
    Breathing,
    COUNT   // must be last
};

// Get the effect function for a given mode
EffectFn getEffect(EffectMode mode);

// Advance to next mode, wrapping around
EffectMode nextMode(EffectMode current);

// Get effect name for serial logging
const char* effectName(EffectMode mode);
