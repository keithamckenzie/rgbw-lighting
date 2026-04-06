#include "config.h"
#include "control.h"
#include "audio.h"
#include "effects.h"
#include "matrix.h"
#include "matter_ep.h"
#include "state.h"
#include "web_ui.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>
#include <WiFi.h>

#include "led_strip.h"

namespace {

constexpr const char* TAG = "dj_booth.main";

#if STRIP_TYPE == STRIP_TYPE_SK6812
LEDStrip<StripType::SK6812_RGBW> strip(PIN_LED_DATA, NUM_PIXELS);
#else
LEDStrip<StripType::WS2815B_RGB> strip(PIN_LED_DATA, NUM_PIXELS);
#endif

RGBW pixelBuffer[NUM_PIXELS];
LightState lightState = makeDefaultLightState();
AudioState audioState{};
uint32_t frameCount = 0;
uint32_t startMs = 0;
uint32_t lastFrameMs = 0;
bool s_stripBlanked = false;
bool s_matterStartPending = true;

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(100);

    const ControlStatus status = controlBegin();
    if (status != ControlStatus::Ok) {
        ESP_LOGW(TAG, "Control queue init failed: %u", static_cast<unsigned>(status));
    }

    audioResetState(audioState);
    audioBegin();

    strip.begin();
    strip.clear();
    strip.show();
    s_stripBlanked = true;

    startMs = millis();
    lastFrameMs = startMs;

    webBegin(lightState, audioState);
    if (WiFi.status() == WL_CONNECTED) {
        matterBegin(lightState);
        if (matterStarted()) {
            s_matterStartPending = false;
        }
    }

    ESP_LOGI(
        TAG,
        "DJ Booth panel ready: %ux%u (%u pixels)",
        static_cast<unsigned>(PANEL_WIDTH),
        static_cast<unsigned>(PANEL_HEIGHT),
        static_cast<unsigned>(NUM_PIXELS)
    );
    ESP_LOGI(TAG, "Effect: %s", effectName(lightState.effectMode));
}

void loop() {
    webLoop(false);

#ifndef NATIVE_BUILD
    if (s_matterStartPending && !matterStarted() && WiFi.status() == WL_CONNECTED) {
        matterBegin(lightState);
        if (matterStarted()) {
            s_matterStartPending = false;
        }
    }
#endif

    const uint32_t nowMs = millis();
    if ((nowMs - lastFrameMs) < FRAME_MS) {
        return;
    }
    lastFrameMs = nowMs;

    lightStateBeginFrame(lightState);
    controlProcess(lightState);
    matterSync(lightState);
    audioUpdate(audioState, nowMs);

    if (lightState.modeChanged) {
        ESP_LOGI(TAG, "Effect: %s", effectName(lightState.effectMode));
    }

    webLoop();

    strip.setBrightness(lightState.brightness);

    if (!lightState.powerOn) {
        if (!s_stripBlanked) {
            strip.clear();
            strip.show();
            s_stripBlanked = true;
        }
        return;
    }

    s_stripBlanked = false;

    EffectContext ctx{};
    ctx.frameCount = frameCount;
    ctx.elapsedMs = nowMs - startMs;
    ctx.userColor = &lightState.color;
    ctx.audio = &audioState;

    const EffectFn effect = getEffect(lightState.effectMode);
    effect(pixelBuffer, NUM_PIXELS, ctx);

    for (uint16_t index = 0; index < NUM_PIXELS; index++) {
        strip.setPixel(index, pixelBuffer[index]);
    }

    strip.show();
    frameCount++;
}

#elif !defined(PIO_UNIT_TESTING)

int main() {
    LightState state = makeDefaultLightState();
    AudioState audioState{};
    (void)controlBegin();
    audioResetState(audioState);
    audioBegin();
    webBegin(state, audioState);
    webLoop(false);
    audioUpdate(audioState, 0);
    (void)controlProcess(state);
    matterBegin(state);
    matterSync(state);
    webLoop();
    (void)mapXY(0, 0);
    return 0;
}

#endif
