#include "control.h"

#include <type_traits>

#include "config.h"

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>
#else
#include <array>
#include <cstdio>
#include <mutex>

#define ESP_LOGI(tag, fmt, ...) do { std::printf("I (%s): " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define ESP_LOGW(tag, fmt, ...) do { std::printf("W (%s): " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#endif

namespace {

constexpr const char* TAG = "dj_booth.control";

static_assert(std::is_trivially_copyable<ControlCommand>::value,
              "ControlCommand must remain trivially copyable for queue transport");

#ifdef ESP32
StaticQueue_t s_queueStruct;
uint8_t s_queueStorage[CONTROL_QUEUE_LENGTH * sizeof(ControlCommand)];
QueueHandle_t s_queue = nullptr;
#else
std::array<ControlCommand, CONTROL_QUEUE_LENGTH> s_queueStorage{};
std::mutex s_queueMutex;
uint8_t s_queueHead = 0;
uint8_t s_queueTail = 0;
uint8_t s_queueCount = 0;
bool s_initialized = false;
#endif

void applyCommand(const ControlCommand& command, LightState& state) {
    switch (command.type) {
        case CommandType::SetPower:
            state.powerOn = command.value.powerOn;
            break;

        case CommandType::SetBrightness:
            state.brightness = command.value.brightness;
            break;

        case CommandType::SetColor:
            state.color = normalizeHSV(command.value.color);
            if (state.effectMode != EffectMode::Solid) {
                state.effectMode = EffectMode::Solid;
                state.modeChanged = true;
            }
            break;

        case CommandType::SetEffect:
            if (command.value.effect < EffectMode::COUNT && state.effectMode != command.value.effect) {
                state.effectMode = command.value.effect;
                state.modeChanged = true;
            }
            break;

        case CommandType::NextEffect: {
            const EffectMode next = nextMode(state.effectMode);
            if (next != state.effectMode) {
                state.effectMode = next;
                state.modeChanged = true;
            }
            break;
        }

        default:
            break;
    }
}

#ifndef ESP32
bool popCommand(ControlCommand& command) {
    std::lock_guard<std::mutex> lock(s_queueMutex);
    if (s_queueCount == 0U) {
        return false;
    }

    command = s_queueStorage[s_queueHead];
    s_queueHead = static_cast<uint8_t>((s_queueHead + 1U) % CONTROL_QUEUE_LENGTH);
    s_queueCount--;
    return true;
}
#endif

}  // namespace

ControlStatus controlBegin() {
#ifdef ESP32
    s_queue = xQueueCreateStatic(
        CONTROL_QUEUE_LENGTH,
        sizeof(ControlCommand),
        s_queueStorage,
        &s_queueStruct
    );

    if (s_queue == nullptr) {
        return ControlStatus::NotInitialized;
    }
#else
    std::lock_guard<std::mutex> lock(s_queueMutex);
    s_queueHead = 0;
    s_queueTail = 0;
    s_queueCount = 0;
    s_initialized = true;
#endif

    ESP_LOGI(TAG, "Control queue ready (%u slots)", static_cast<unsigned>(CONTROL_QUEUE_LENGTH));
    return ControlStatus::Ok;
}

ControlStatus controlEnqueue(const ControlCommand& command) {
#ifdef ESP32
    if (s_queue == nullptr) {
        return ControlStatus::NotInitialized;
    }

    if (xQueueSendToBack(s_queue, &command, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Dropping command from source %u", static_cast<unsigned>(command.source));
        return ControlStatus::QueueFull;
    }

    return ControlStatus::Ok;
#else
    std::lock_guard<std::mutex> lock(s_queueMutex);
    if (!s_initialized) {
        return ControlStatus::NotInitialized;
    }

    if (s_queueCount >= CONTROL_QUEUE_LENGTH) {
        ESP_LOGW(TAG, "Dropping command from source %u", static_cast<unsigned>(command.source));
        return ControlStatus::QueueFull;
    }

    s_queueStorage[s_queueTail] = command;
    s_queueTail = static_cast<uint8_t>((s_queueTail + 1U) % CONTROL_QUEUE_LENGTH);
    s_queueCount++;
    return ControlStatus::Ok;
#endif
}

uint8_t controlProcess(LightState& state) {
    uint8_t processed = 0;
    ControlCommand command{};

#ifdef ESP32
    if (s_queue == nullptr) {
        return 0;
    }

    while (xQueueReceive(s_queue, &command, 0) == pdTRUE) {
        applyCommand(command, state);
        processed++;
    }
#else
    while (popCommand(command)) {
        applyCommand(command, state);
        processed++;
    }
#endif

    return processed;
}
