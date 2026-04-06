#pragma once

#include <stdint.h>
#include "state.h"

enum class ControlSource : uint8_t {
    Matter = 0,
    WebUI,
    BLE,
    Physical,
    Internal
};

enum class CommandType : uint8_t {
    SetPower = 0,
    SetBrightness,
    SetColor,
    SetEffect,
    NextEffect
};

union ControlValue {
    bool powerOn;
    uint8_t brightness;
    HSV color;
    EffectMode effect;

    constexpr ControlValue() : brightness(0) {}
};

struct ControlCommand {
    CommandType type;
    ControlSource source;
    ControlValue value;
};

enum class ControlStatus : uint8_t {
    Ok = 0,
    NotInitialized,
    QueueFull
};

ControlStatus controlBegin();
ControlStatus controlEnqueue(const ControlCommand& command);
uint8_t controlProcess(LightState& state);

inline ControlCommand makePowerCommand(ControlSource source, bool powerOn) {
    ControlCommand command{};
    command.type = CommandType::SetPower;
    command.source = source;
    command.value.powerOn = powerOn;
    return command;
}

inline ControlCommand makeBrightnessCommand(ControlSource source, uint8_t brightness) {
    ControlCommand command{};
    command.type = CommandType::SetBrightness;
    command.source = source;
    command.value.brightness = brightness;
    return command;
}

inline ControlCommand makeColorCommand(ControlSource source, const HSV& color) {
    ControlCommand command{};
    command.type = CommandType::SetColor;
    command.source = source;
    command.value.color = color;
    return command;
}

inline ControlCommand makeEffectCommand(ControlSource source, EffectMode effect) {
    ControlCommand command{};
    command.type = CommandType::SetEffect;
    command.source = source;
    command.value.effect = effect;
    return command;
}

inline ControlCommand makeNextEffectCommand(ControlSource source) {
    ControlCommand command{};
    command.type = CommandType::NextEffect;
    command.source = source;
    return command;
}
