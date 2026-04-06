#include "web_ui.h"

#ifdef NATIVE_BUILD

void webBegin(LightState& lightState, AudioState& audioState) {
    (void)lightState;
    (void)audioState;
}

void webLoop(bool allowPush) {
    (void)allowPush;
}

#else

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_log.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "control.h"
#include "effects.h"
#include "secrets.h"
#include "wifi_manager.h"

namespace {

constexpr const char* TAG = "dj_booth.web";
constexpr uint16_t kServerPort = 80;
constexpr uint32_t kWifiTimeoutMs = 10000;
constexpr uint32_t kSpectrumIntervalMs = 100;
constexpr uint32_t kKeepAliveIntervalMs = 15000;
constexpr size_t kRequestBodySize = 96;
constexpr size_t kStateJsonSize = 160;
constexpr size_t kEffectsJsonSize = 384;
constexpr size_t kSpectrumJsonSize = 256;
constexpr size_t kSseFrameSize = 384;

struct StateSnapshot {
    bool powerOn;
    uint8_t brightness;
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint8_t effectIndex;
};

// Known limitation: the synchronous WebServer runs on the main loop task and can
// introduce minor render stutter while requests are handled. The roadmap is to
// migrate this app to ESPAsyncWebServer instead of growing more sync behavior.
WebServer s_server(kServerPort);
WiFiManager s_wifiManager;
LightState* s_lightState = nullptr;
AudioState* s_audioState = nullptr;
WiFiClient s_sseClient;
bool s_serverStarted = false;
bool s_filesystemReady = false;
bool s_sseClientActive = false;
bool s_pendingStatePush = false;
bool s_lastStateValid = false;
StateSnapshot s_lastState{};
uint32_t s_lastSpectrumMs = 0;
uint32_t s_lastKeepAliveMs = 0;

bool isJsonValueTerminator(char ch) {
    switch (ch) {
        case '\0':
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case ',':
        case ']':
        case '}':
            return true;
        default:
            return false;
    }
}

const char* statusText(int code) {
    switch (code) {
        case 200:
            return "OK";
        case 204:
            return "No Content";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 500:
            return "Internal Server Error";
        case 503:
            return "Service Unavailable";
        default:
            return "OK";
    }
}

StateSnapshot captureState() {
    StateSnapshot snapshot{};
    if (s_lightState == nullptr) {
        return snapshot;
    }

    snapshot.powerOn = s_lightState->powerOn;
    snapshot.brightness = s_lightState->brightness;
    snapshot.hue = s_lightState->color.h;
    snapshot.saturation = s_lightState->color.s;
    snapshot.value = s_lightState->color.v;
    snapshot.effectIndex = static_cast<uint8_t>(s_lightState->effectMode);
    return snapshot;
}

bool statesEqual(const StateSnapshot& lhs, const StateSnapshot& rhs) {
    return lhs.powerOn == rhs.powerOn &&
           lhs.brightness == rhs.brightness &&
           lhs.hue == rhs.hue &&
           lhs.saturation == rhs.saturation &&
           lhs.value == rhs.value &&
           lhs.effectIndex == rhs.effectIndex;
}

void disconnectSseClient() {
    if (s_sseClientActive) {
        s_sseClient.stop();
    }
    s_sseClientActive = false;
}

bool sseClientConnected() {
    if (!s_sseClientActive) {
        return false;
    }

    if (!s_sseClient.connected()) {
        disconnectSseClient();
        return false;
    }

    return true;
}

void sendEmptyResponse(int code) {
    WiFiClient client = s_server.client();
    client.printf(
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n",
        code,
        statusText(code)
    );
}

void sendBodyResponse(int code, const char* contentType, const char* body) {
    const size_t length = (body == nullptr) ? 0U : std::strlen(body);
    WiFiClient client = s_server.client();
    client.printf(
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        code,
        statusText(code),
        contentType,
        static_cast<unsigned>(length)
    );

    if (length > 0U) {
        client.write(reinterpret_cast<const uint8_t*>(body), length);
    }
}

void sendErrorResponse(int code, const char* message) {
    char body[96];
    const int written = std::snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    if (written < 0 || written >= static_cast<int>(sizeof(body))) {
        sendBodyResponse(code, "application/json", "{\"error\":\"overflow\"}");
        return;
    }
    sendBodyResponse(code, "application/json", body);
}

bool copyRequestBody(char* buffer, size_t bufferSize) {
    if (!s_server.hasArg("plain")) {
        return false;
    }

    // WebServer::arg() returns Arduino String by value; this sync stack offers no
    // zero-allocation request-body API today. Keep the exception contained here
    // until the planned ESPAsyncWebServer migration removes the String copy.
    const String& body = s_server.arg("plain");
    const size_t length = static_cast<size_t>(body.length());
    if (length == 0U || (length + 1U) > bufferSize) {
        return false;
    }

    std::memcpy(buffer, body.c_str(), length + 1U);
    return true;
}

const char* findJsonValue(const char* body, const char* key) {
    const char* position = std::strstr(body, key);
    if (position == nullptr) {
        return nullptr;
    }

    position += std::strlen(key);
    while (*position == ' ' || *position == '\t' || *position == '\r' ||
           *position == '\n' || *position == ':') {
        position++;
    }
    return position;
}

bool parseJsonLong(const char* body, const char* key, long minimum, long maximum, long& valueOut) {
    const char* valueStart = findJsonValue(body, key);
    if (valueStart == nullptr) {
        return false;
    }

    char* valueEnd = nullptr;
    const long value = std::strtol(valueStart, &valueEnd, 10);
    if (valueEnd == valueStart || !isJsonValueTerminator(*valueEnd) || value < minimum ||
        value > maximum) {
        return false;
    }

    valueOut = value;
    return true;
}

bool parseJsonBool(const char* body, const char* key, bool& valueOut) {
    const char* valueStart = findJsonValue(body, key);
    if (valueStart == nullptr) {
        return false;
    }

    if (std::strncmp(valueStart, "true", 4) == 0) {
        if (!isJsonValueTerminator(valueStart[4])) {
            return false;
        }
        valueOut = true;
        return true;
    }

    if (std::strncmp(valueStart, "false", 5) == 0) {
        if (!isJsonValueTerminator(valueStart[5])) {
            return false;
        }
        valueOut = false;
        return true;
    }

    return false;
}

bool buildStateJson(char* buffer, size_t bufferSize) {
    if (s_lightState == nullptr) {
        return false;
    }

    const int written = std::snprintf(
        buffer,
        bufferSize,
        "{\"power\":%s,\"brightness\":%u,\"hue\":%u,\"sat\":%u,\"val\":%u,"
        "\"effect\":\"%s\",\"effectIndex\":%u}",
        s_lightState->powerOn ? "true" : "false",
        static_cast<unsigned>(s_lightState->brightness),
        static_cast<unsigned>(s_lightState->color.h),
        static_cast<unsigned>(s_lightState->color.s),
        static_cast<unsigned>(s_lightState->color.v),
        effectName(s_lightState->effectMode),
        static_cast<unsigned>(static_cast<uint8_t>(s_lightState->effectMode))
    );

    return written > 0 && written < static_cast<int>(bufferSize);
}

bool buildEffectsJson(char* buffer, size_t bufferSize) {
    if (bufferSize == 0U) {
        return false;
    }

    size_t used = 0;
    int written = std::snprintf(buffer, bufferSize, "[");
    if (written <= 0 || written >= static_cast<int>(bufferSize)) {
        return false;
    }
    used = static_cast<size_t>(written);

    const uint8_t effectCount = static_cast<uint8_t>(EffectMode::COUNT);
    for (uint8_t index = 0; index < effectCount; index++) {
        written = std::snprintf(
            buffer + used,
            bufferSize - used,
            "%s{\"index\":%u,\"name\":\"%s\"}",
            index == 0U ? "" : ",",
            static_cast<unsigned>(index),
            effectName(static_cast<EffectMode>(index))
        );
        if (written <= 0 || static_cast<size_t>(written) >= (bufferSize - used)) {
            return false;
        }
        used += static_cast<size_t>(written);
    }

    written = std::snprintf(buffer + used, bufferSize - used, "]");
    return written > 0 && static_cast<size_t>(written) < (bufferSize - used);
}

bool buildSpectrumJson(char* buffer, size_t bufferSize) {
    if (s_audioState == nullptr || bufferSize == 0U) {
        return false;
    }

    size_t used = 0;
    int written = std::snprintf(buffer, bufferSize, "{\"bands\":[");
    if (written <= 0 || written >= static_cast<int>(bufferSize)) {
        return false;
    }
    used = static_cast<size_t>(written);

    for (uint8_t index = 0; index < AUDIO_NUM_BANDS; index++) {
        written = std::snprintf(
            buffer + used,
            bufferSize - used,
            "%s%.3f",
            index == 0U ? "" : ",",
            static_cast<double>(s_audioState->bandEnergy[index])
        );
        if (written <= 0 || static_cast<size_t>(written) >= (bufferSize - used)) {
            return false;
        }
        used += static_cast<size_t>(written);
    }

    written = std::snprintf(
        buffer + used,
        bufferSize - used,
        "],\"beat\":%s,\"bpm\":%.1f}",
        s_audioState->beatDetected ? "true" : "false",
        static_cast<double>(s_audioState->bpm)
    );
    return written > 0 && static_cast<size_t>(written) < (bufferSize - used);
}

bool sendSseEvent(const char* eventName, const char* jsonPayload) {
    if (!sseClientConnected()) {
        return false;
    }

    char frame[kSseFrameSize];
    const int frameLength = std::snprintf(
        frame,
        sizeof(frame),
        "event: %s\n"
        "data: %s\n\n",
        eventName,
        jsonPayload
    );

    if (frameLength <= 0 || frameLength >= static_cast<int>(sizeof(frame))) {
        disconnectSseClient();
        return false;
    }

    if (s_sseClient.availableForWrite() < static_cast<size_t>(frameLength)) {
        ESP_LOGW(TAG, "Dropping SSE client due to backpressure");
        disconnectSseClient();
        return false;
    }

    const size_t written = s_sseClient.write(
        reinterpret_cast<const uint8_t*>(frame),
        static_cast<size_t>(frameLength)
    );
    if (written != static_cast<size_t>(frameLength)) {
        disconnectSseClient();
        return false;
    }

    return true;
}

void sendSseKeepAlive() {
    if (!sseClientConnected()) {
        return;
    }

    static constexpr char kPingFrame[] = ": ping\n\n";
    if (s_sseClient.availableForWrite() < sizeof(kPingFrame) - 1U) {
        ESP_LOGW(TAG, "Dropping SSE client due to keepalive backpressure");
        disconnectSseClient();
        return;
    }

    const size_t written = s_sseClient.write(
        reinterpret_cast<const uint8_t*>(kPingFrame),
        sizeof(kPingFrame) - 1U
    );
    if (written != (sizeof(kPingFrame) - 1U)) {
        disconnectSseClient();
    }
}

bool enqueueCommand(const ControlCommand& command) {
    const ControlStatus status = controlEnqueue(command);
    if (status == ControlStatus::Ok) {
        s_pendingStatePush = true;
        sendEmptyResponse(204);
        return true;
    }

    if (status == ControlStatus::QueueFull) {
        sendErrorResponse(503, "queue_full");
        return false;
    }

    sendErrorResponse(503, "not_ready");
    return false;
}

void handleRoot() {
    if (!s_filesystemReady) {
        sendErrorResponse(503, "filesystem_unavailable");
        return;
    }

    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        sendErrorResponse(404, "missing_index");
        return;
    }

    WiFiClient client = s_server.client();
    client.printf(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        static_cast<unsigned>(file.size())
    );

    uint8_t chunk[512];
    while (file.available()) {
        const size_t readCount = file.read(chunk, sizeof(chunk));
        if (readCount == 0U) {
            break;
        }
        client.write(chunk, readCount);
    }

    file.close();
}

void handleStateGet() {
    char json[kStateJsonSize];
    if (!buildStateJson(json, sizeof(json))) {
        sendErrorResponse(500, "state_unavailable");
        return;
    }

    sendBodyResponse(200, "application/json", json);
}

void handleEffectsGet() {
    char json[kEffectsJsonSize];
    if (!buildEffectsJson(json, sizeof(json))) {
        sendErrorResponse(500, "effects_unavailable");
        return;
    }

    sendBodyResponse(200, "application/json", json);
}

void handlePowerPost() {
    char body[kRequestBodySize];
    bool powerOn = false;
    if (!copyRequestBody(body, sizeof(body)) || !parseJsonBool(body, "\"on\"", powerOn)) {
        sendErrorResponse(400, "invalid_power");
        return;
    }

    enqueueCommand(makePowerCommand(ControlSource::WebUI, powerOn));
}

void handleBrightnessPost() {
    char body[kRequestBodySize];
    long brightness = 0;
    if (!copyRequestBody(body, sizeof(body)) ||
        !parseJsonLong(body, "\"value\"", 0, 255, brightness)) {
        sendErrorResponse(400, "invalid_brightness");
        return;
    }

    enqueueCommand(
        makeBrightnessCommand(ControlSource::WebUI, static_cast<uint8_t>(brightness))
    );
}

void handleColorPost() {
    char body[kRequestBodySize];
    long hue = 0;
    long saturation = 0;
    long value = 0;
    if (!copyRequestBody(body, sizeof(body)) ||
        !parseJsonLong(body, "\"h\"", 0, 360, hue) ||
        !parseJsonLong(body, "\"s\"", 0, 255, saturation) ||
        !parseJsonLong(body, "\"v\"", 0, 255, value)) {
        sendErrorResponse(400, "invalid_color");
        return;
    }

    HSV color{};
    color.h = static_cast<uint16_t>(hue);
    color.s = static_cast<uint8_t>(saturation);
    color.v = static_cast<uint8_t>(value);

    enqueueCommand(makeColorCommand(ControlSource::WebUI, color));
}

void handleEffectPost() {
    char body[kRequestBodySize];
    long effectIndex = 0;
    if (!copyRequestBody(body, sizeof(body)) ||
        !parseJsonLong(
            body,
            "\"index\"",
            0,
            static_cast<long>(static_cast<uint8_t>(EffectMode::COUNT) - 1U),
            effectIndex
        )) {
        sendErrorResponse(400, "invalid_effect");
        return;
    }

    enqueueCommand(
        makeEffectCommand(
            ControlSource::WebUI,
            static_cast<EffectMode>(static_cast<uint8_t>(effectIndex))
        )
    );
}

void handleNextEffectPost() {
    enqueueCommand(makeNextEffectCommand(ControlSource::WebUI));
}

void handleEventsGet() {
    if (s_sseClientActive) {
        disconnectSseClient();
    }

    s_sseClient = s_server.client();
    s_sseClient.setNoDelay(true);
    s_sseClient.printf(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
    );
    s_sseClientActive = true;
    s_pendingStatePush = true;
    s_lastSpectrumMs = 0;
    s_lastKeepAliveMs = millis();
    s_lastStateValid = false;
}

void handleNotFound() {
    sendErrorResponse(404, "not_found");
}

void registerRoutes() {
    s_server.on("/", HTTP_GET, handleRoot);
    s_server.on("/api/effects", HTTP_GET, handleEffectsGet);
    s_server.on("/api/state", HTTP_GET, handleStateGet);
    s_server.on("/api/power", HTTP_POST, handlePowerPost);
    s_server.on("/api/brightness", HTTP_POST, handleBrightnessPost);
    s_server.on("/api/color", HTTP_POST, handleColorPost);
    s_server.on("/api/effect", HTTP_POST, handleEffectPost);
    s_server.on("/api/effect/next", HTTP_POST, handleNextEffectPost);
    s_server.on("/api/events", HTTP_GET, handleEventsGet);
    s_server.onNotFound(handleNotFound);
}

void maybePushState() {
    if (!sseClientConnected()) {
        return;
    }

    const StateSnapshot snapshot = captureState();
    const bool changed = !s_lastStateValid || !statesEqual(snapshot, s_lastState);
    if (!s_pendingStatePush && !changed && (s_lightState == nullptr || !s_lightState->modeChanged)) {
        return;
    }

    char json[kStateJsonSize];
    if (!buildStateJson(json, sizeof(json))) {
        return;
    }

    if (sendSseEvent("state", json)) {
        s_lastState = snapshot;
        s_lastStateValid = true;
        s_pendingStatePush = false;
    }
}

void maybePushSpectrum(uint32_t nowMs) {
    if (!sseClientConnected() || (nowMs - s_lastSpectrumMs) < kSpectrumIntervalMs) {
        return;
    }

    char json[kSpectrumJsonSize];
    if (!buildSpectrumJson(json, sizeof(json))) {
        return;
    }

    if (sendSseEvent("spectrum", json)) {
        s_lastSpectrumMs = nowMs;
    }
}

void maybeSendKeepAlive(uint32_t nowMs) {
    if (!sseClientConnected() || (nowMs - s_lastKeepAliveMs) < kKeepAliveIntervalMs) {
        return;
    }

    sendSseKeepAlive();
    if (s_sseClientActive) {
        s_lastKeepAliveMs = nowMs;
    }
}

}  // namespace

void webBegin(LightState& lightState, AudioState& audioState) {
    s_lightState = &lightState;
    s_audioState = &audioState;
    s_pendingStatePush = true;
    s_lastStateValid = false;
    s_lastSpectrumMs = 0;
    s_lastKeepAliveMs = millis();

    s_filesystemReady = LittleFS.begin();
    if (!s_filesystemReady) {
        ESP_LOGW(TAG, "LittleFS mount failed");
    }

    if (!s_wifiManager.connect(WIFI_SSID, WIFI_PASSWORD, kWifiTimeoutMs)) {
        ESP_LOGW(TAG, "WiFi connect timed out");
    } else {
        ESP_LOGI(TAG, "WiFi connected: %s", s_wifiManager.getIP().c_str());
    }

    registerRoutes();
    s_server.begin();
    s_serverStarted = true;
    ESP_LOGI(TAG, "Web server listening on port %u", static_cast<unsigned>(kServerPort));
}

void webLoop(bool allowPush) {
    if (!s_serverStarted) {
        return;
    }

    s_server.handleClient();

    if (!allowPush) {
        return;
    }

    const uint32_t nowMs = millis();
    maybePushState();
    maybePushSpectrum(nowMs);
    maybeSendKeepAlive(nowMs);
}

#endif
