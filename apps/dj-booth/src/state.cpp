#include "state.h"
#include "config.h"

namespace {

uint16_t normalizeHue(uint16_t hue) {
    if (hue >= 360U) {
        return static_cast<uint16_t>(hue % 360U);
    }
    return hue;
}

}  // namespace

LightState makeDefaultLightState() {
    LightState state{};
    state.powerOn = (DEFAULT_POWER_ON != 0);
    state.brightness = DEFAULT_BRIGHTNESS;
    state.color = HSV{0, 255, 255};
    state.effectMode = EffectMode::Solid;
    state.modeChanged = true;
    return state;
}

void lightStateBeginFrame(LightState& state) {
    state.modeChanged = false;
}

HSV normalizeHSV(const HSV& color) {
    HSV normalized = color;
    normalized.h = normalizeHue(color.h);
    return normalized;
}

RGBW lightStateColorRGBW(const LightState& state) {
    return hsvToRGBW(normalizeHSV(state.color));
}
