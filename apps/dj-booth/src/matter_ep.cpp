#include "matter_ep.h"

#ifdef NATIVE_BUILD

void matterBegin(LightState& state) {
    (void)state;
}

bool matterStarted() {
    return false;
}

void matterSync(const LightState& state) {
    (void)state;
}

#else

#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include <array>

#include "control.h"

#if defined(CONFIG_ESP_MATTER_ENABLE_DATA_MODEL)
#include <Matter.h>

#if __has_include(<MatterEndpoints/MatterEnhancedColorLight.h>)
    #include <MatterEndpoints/MatterEnhancedColorLight.h>
namespace {
using MatterLightEndpoint = MatterEnhancedColorLight;
constexpr bool kUsingEnhancedColorLight = true;
}
#elif __has_include(<MatterEndpoints/MatterColorLight.h>)
    #include <MatterEndpoints/MatterColorLight.h>
namespace {
using MatterLightEndpoint = MatterColorLight;
constexpr bool kUsingEnhancedColorLight = false;
}
#else
    #error "No supported Matter color light endpoint is available"
#endif
#endif

namespace {

constexpr const char* TAG = "dj_booth.matter";
constexpr uint8_t kMatterSvMax = 254;
constexpr uint16_t kMatterHueMax = 254;
constexpr size_t kEchoHistoryLength = 8;

#if defined(CONFIG_ESP_MATTER_ENABLE_DATA_MODEL)
struct MatterStateSnapshot {
    bool powerOn;
    uint8_t brightness;
    HSV color;
};

MatterLightEndpoint s_light;
MatterStateSnapshot s_lastSynced{};
bool s_started = false;
bool s_lastSyncedValid = false;
portMUX_TYPE s_echoHistoryMux = portMUX_INITIALIZER_UNLOCKED;

enum EchoFieldMask : uint8_t {
    kEchoFieldPower = 1U << 0,
    kEchoFieldBrightness = 1U << 1,
    kEchoFieldColor = 1U << 2,
    kEchoFieldAll = kEchoFieldPower | kEchoFieldBrightness | kEchoFieldColor,
};

struct PendingEcho {
    MatterStateSnapshot snapshot;
    uint8_t pendingMask;
    bool valid;
};

std::array<PendingEcho, kEchoHistoryLength> s_echoHistory{};
size_t s_nextEchoSlot = 0;

uint8_t scaleToMatter(uint8_t projectValue) {
    return static_cast<uint8_t>((static_cast<uint16_t>(projectValue) * kMatterSvMax + 127U) / 255U);
}

uint8_t scaleFromMatter(uint8_t matterValue) {
    return static_cast<uint8_t>((static_cast<uint16_t>(matterValue) * 255U + (kMatterSvMax / 2U)) / kMatterSvMax);
}

uint8_t projectToMatterSV(uint8_t projectValue) {
    return scaleToMatter(projectValue);
}

uint8_t matterToProjectSV(uint8_t matterValue) {
    return scaleFromMatter(matterValue);
}

uint16_t projectToMatterHue(uint16_t projectHue) {
    const uint16_t normalizedHue = static_cast<uint16_t>(projectHue % 360U);
    return static_cast<uint16_t>((static_cast<uint32_t>(normalizedHue) * kMatterHueMax + 180U) / 360U);
}

uint16_t matterToProjectHue(uint16_t matterHue) {
    return static_cast<uint16_t>((static_cast<uint32_t>(matterHue) * 360U + (kMatterHueMax / 2U)) / kMatterHueMax);
}

espHsvColor_t projectToMatterColor(const HSV& color) {
    espHsvColor_t matterColor{};
    matterColor.h = projectToMatterHue(color.h);
    matterColor.s = projectToMatterSV(color.s);
    matterColor.v = projectToMatterSV(color.v);
    return matterColor;
}

HSV matterToProjectColor(const espHsvColor_t& matterColor) {
    HSV projectColor{};
    projectColor.h = matterToProjectHue(matterColor.h);
    projectColor.s = matterToProjectSV(matterColor.s);
    projectColor.v = matterToProjectSV(matterColor.v);
    return projectColor;
}

MatterStateSnapshot captureMatterState(const LightState& state) {
    MatterStateSnapshot snapshot{};
    snapshot.powerOn = state.powerOn;
    snapshot.brightness = state.brightness;
    snapshot.color = normalizeHSV(state.color);
    return snapshot;
}

bool statesEqual(const MatterStateSnapshot& lhs, const MatterStateSnapshot& rhs) {
    return lhs.powerOn == rhs.powerOn &&
           lhs.brightness == rhs.brightness &&
           lhs.color.h == rhs.color.h &&
           lhs.color.s == rhs.color.s &&
           lhs.color.v == rhs.color.v;
}

bool colorsEqual(const HSV& lhs, const HSV& rhs) {
    return lhs.h == rhs.h && lhs.s == rhs.s && lhs.v == rhs.v;
}

void clearEchoHistory() {
    portENTER_CRITICAL(&s_echoHistoryMux);
    for (PendingEcho& entry : s_echoHistory) {
        entry.valid = false;
        entry.pendingMask = 0;
    }
    s_nextEchoSlot = 0;
    portEXIT_CRITICAL(&s_echoHistoryMux);
}

void rememberPendingEcho(const MatterStateSnapshot& snapshot) {
    portENTER_CRITICAL(&s_echoHistoryMux);
    PendingEcho& entry = s_echoHistory[s_nextEchoSlot];
    entry.snapshot = snapshot;
    entry.pendingMask = kEchoFieldAll;
    entry.valid = true;
    s_nextEchoSlot = (s_nextEchoSlot + 1U) % s_echoHistory.size();
    portEXIT_CRITICAL(&s_echoHistoryMux);
}

void forgetPendingEcho(const MatterStateSnapshot& snapshot) {
    portENTER_CRITICAL(&s_echoHistoryMux);
    size_t slot = s_nextEchoSlot;
    for (size_t count = 0; count < s_echoHistory.size(); count++) {
        slot = (slot == 0U) ? (s_echoHistory.size() - 1U) : (slot - 1U);
        PendingEcho& entry = s_echoHistory[slot];
        if (!entry.valid || !statesEqual(entry.snapshot, snapshot)) {
            continue;
        }

        entry.valid = false;
        entry.pendingMask = 0;
        break;
    }
    portEXIT_CRITICAL(&s_echoHistoryMux);
}

bool consumePendingEchoPower(bool powerOn) {
    bool suppressed = false;

    portENTER_CRITICAL(&s_echoHistoryMux);
    size_t slot = s_nextEchoSlot;
    for (size_t count = 0; count < s_echoHistory.size(); count++) {
        slot = (slot == 0U) ? (s_echoHistory.size() - 1U) : (slot - 1U);
        PendingEcho& entry = s_echoHistory[slot];
        if (!entry.valid ||
            (entry.pendingMask & kEchoFieldPower) == 0U ||
            entry.snapshot.powerOn != powerOn) {
            continue;
        }

        entry.pendingMask &= static_cast<uint8_t>(~kEchoFieldPower);
        entry.valid = entry.pendingMask != 0U;
        suppressed = true;
        break;
    }
    portEXIT_CRITICAL(&s_echoHistoryMux);

    return suppressed;
}

bool consumePendingEchoBrightness(uint8_t brightness) {
    bool suppressed = false;

    portENTER_CRITICAL(&s_echoHistoryMux);
    size_t slot = s_nextEchoSlot;
    for (size_t count = 0; count < s_echoHistory.size(); count++) {
        slot = (slot == 0U) ? (s_echoHistory.size() - 1U) : (slot - 1U);
        PendingEcho& entry = s_echoHistory[slot];
        if (!entry.valid ||
            (entry.pendingMask & kEchoFieldBrightness) == 0U ||
            entry.snapshot.brightness != brightness) {
            continue;
        }

        entry.pendingMask &= static_cast<uint8_t>(~kEchoFieldBrightness);
        entry.valid = entry.pendingMask != 0U;
        suppressed = true;
        break;
    }
    portEXIT_CRITICAL(&s_echoHistoryMux);

    return suppressed;
}

bool consumePendingEchoColor(const HSV& color) {
    bool suppressed = false;

    portENTER_CRITICAL(&s_echoHistoryMux);
    size_t slot = s_nextEchoSlot;
    for (size_t count = 0; count < s_echoHistory.size(); count++) {
        slot = (slot == 0U) ? (s_echoHistory.size() - 1U) : (slot - 1U);
        PendingEcho& entry = s_echoHistory[slot];
        if (!entry.valid ||
            (entry.pendingMask & kEchoFieldColor) == 0U ||
            !colorsEqual(entry.snapshot.color, color)) {
            continue;
        }

        entry.pendingMask &= static_cast<uint8_t>(~kEchoFieldColor);
        entry.valid = entry.pendingMask != 0U;
        suppressed = true;
        break;
    }
    portEXIT_CRITICAL(&s_echoHistoryMux);

    return suppressed;
}

bool enqueueMatterCommand(const ControlCommand& command) {
    const ControlStatus status = controlEnqueue(command);
    if (status == ControlStatus::Ok) {
        return true;
    }

    ESP_LOGW(TAG, "Matter command enqueue failed: %u", static_cast<unsigned>(status));
    return false;
}

void logCommissioningState() {
    if (Matter.isDeviceCommissioned()) {
        ESP_LOGI(TAG, "Matter commissioned and ready");
        return;
    }

    ESP_LOGI(TAG, "Matter waiting for commissioning");
    ESP_LOGI(TAG, "Manual pairing code: %s", Matter.getManualPairingCode().c_str());
    ESP_LOGI(TAG, "QR code URL: %s", Matter.getOnboardingQRCodeUrl().c_str());
}

#endif

}  // namespace

void matterBegin(LightState& state) {
#if !defined(CONFIG_ESP_MATTER_ENABLE_DATA_MODEL)
    (void)state;
    ESP_LOGW(TAG, "Matter data model is disabled; skipping endpoint setup");
#else
    if (s_started) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW(TAG, "WiFi is not connected; skipping Matter startup");
        return;
    }

    const MatterStateSnapshot initialState = captureMatterState(state);
    const espHsvColor_t initialColor = projectToMatterColor(initialState.color);

    bool endpointReady = false;
    if constexpr (kUsingEnhancedColorLight) {
        endpointReady = s_light.begin(
            initialState.powerOn,
            initialColor,
            projectToMatterSV(initialState.brightness)
        );
    } else {
        endpointReady = s_light.begin(initialState.powerOn, initialColor);
        if (endpointReady) {
            endpointReady = s_light.setBrightness(projectToMatterSV(initialState.brightness));
        }
    }

    if (!endpointReady) {
        ESP_LOGW(TAG, "Matter endpoint init failed");
        return;
    }

    clearEchoHistory();

    s_light.onChangeOnOff([](bool powerOn) {
        if (consumePendingEchoPower(powerOn)) {
            return true;
        }
        return enqueueMatterCommand(makePowerCommand(ControlSource::Matter, powerOn));
    });
    s_light.onChangeBrightness([](uint8_t brightness) {
        const uint8_t projectBrightness = matterToProjectSV(brightness);
        if (consumePendingEchoBrightness(projectBrightness)) {
            return true;
        }
        return enqueueMatterCommand(
            makeBrightnessCommand(ControlSource::Matter, projectBrightness)
        );
    });
    s_light.onChangeColorHSV([](espHsvColor_t matterColor) {
        const HSV color = normalizeHSV(matterToProjectColor(matterColor));
        if (consumePendingEchoColor(color)) {
            return true;
        }
        return enqueueMatterCommand(makeColorCommand(ControlSource::Matter, color));
    });

    Matter.begin();

    s_started = true;
    s_lastSynced = initialState;
    s_lastSyncedValid = false;

    logCommissioningState();
#endif
}

bool matterStarted() {
#if !defined(CONFIG_ESP_MATTER_ENABLE_DATA_MODEL)
    return false;
#else
    return s_started;
#endif
}

void matterSync(const LightState& state) {
#if !defined(CONFIG_ESP_MATTER_ENABLE_DATA_MODEL)
    (void)state;
#else
    if (!s_started) {
        return;
    }

    const MatterStateSnapshot snapshot = captureMatterState(state);
    if (s_lastSyncedValid && statesEqual(snapshot, s_lastSynced)) {
        return;
    }

    const espHsvColor_t matterColor = projectToMatterColor(snapshot.color);
    const uint8_t matterBrightness = projectToMatterSV(snapshot.brightness);
    rememberPendingEcho(snapshot);

    const bool colorOk = s_light.setColorHSV(matterColor);
    const bool brightnessOk = s_light.setBrightness(matterBrightness);
    const bool powerOk = s_light.setOnOff(snapshot.powerOn);

    if (!(colorOk && brightnessOk && powerOk)) {
        forgetPendingEcho(snapshot);
        ESP_LOGW(TAG, "Matter sync failed");
        return;
    }

    s_lastSynced = snapshot;
    s_lastSyncedValid = true;
#endif
}

#endif
