#include <Arduino.h>
#include "config.h"
#include "rgbw.h"
#include "led_strip.h"
#include "matrix.h"
#include "effects.h"
#include "input.h"
#include "audio.h"

// --- Strip instantiation (compile-time type selection) ---
#if STRIP_TYPE == 0
static LEDStrip<StripType::SK6812_RGBW> strip(PIN_LED_DATA, NUM_PIXELS);
#else
static LEDStrip<StripType::WS2815B_RGB> strip(PIN_LED_DATA, NUM_PIXELS);
#endif

// --- Pixel buffer (effects write here, then copied to strip) ---
static RGBW pixelBuffer[NUM_PIXELS];

// --- State ---
static InputState inputState;
static AudioState audioState;
static uint32_t frameCount = 0;
static uint32_t startMs = 0;
static uint32_t lastFrameMs = 0;

void setup() {
    Serial.begin(115200);

    // Allow serial to settle
    delay(100);

    Serial.println(F("--- LED Panel ---"));
    Serial.print(F("Panel: "));
    Serial.print(PANEL_WIDTH);
    Serial.print(F("x"));
    Serial.print(PANEL_HEIGHT);
    Serial.print(F(" = "));
    Serial.print(NUM_PIXELS);
    Serial.println(F(" pixels"));
    Serial.print(F("Strip type: "));
#if STRIP_TYPE == 0
    Serial.println(F("SK6812 RGBW"));
#else
    Serial.println(F("WS2815B RGB"));
#endif
    Serial.print(F("Target FPS: "));
    Serial.println(TARGET_FPS);

    strip.begin();
    strip.clear();
    strip.show();

    inputBegin(128);
    audioBegin();

    inputState.powerOn = true;
    inputState.brightness = 128;
    inputState.currentMode = EffectMode::Solid;
    inputState.modeChanged = false;

    audioState.energy = 0.0f;
    audioState.beatDetected = false;
    audioState.bpm = 0.0f;

    startMs = millis();
    lastFrameMs = startMs;

#ifdef ESP32
    Serial.print(F("Free heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
#elif defined(ESP8266)
    Serial.print(F("Free heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
#endif

    Serial.print(F("Effect: "));
    Serial.println(effectName(inputState.currentMode));
}

void loop() {
    uint32_t nowMs = millis();

    // --- Frame rate gate ---
    if (nowMs - lastFrameMs < FRAME_MS) {
        return;
    }
    lastFrameMs = nowMs;

    // --- Input ---
    inputUpdate(inputState, nowMs);

    // --- Audio (no-op on ESP8266) ---
    audioUpdate(audioState, nowMs);

    // --- Apply brightness from pot ---
    strip.setBrightness(inputState.brightness);

    // --- Power off: clear and return ---
    if (!inputState.powerOn) {
        strip.clear();
        strip.show();
        return;
    }

    // --- Log mode changes ---
    if (inputState.modeChanged) {
        Serial.print(F("Effect: "));
        Serial.println(effectName(inputState.currentMode));
    }

    // --- Build effect context ---
    EffectContext ctx;
    ctx.frameCount = frameCount;
    ctx.elapsedMs = nowMs - startMs;
    ctx.brightness = inputState.brightness;
    ctx.audio = &audioState;

    // --- Run current effect into pixel buffer ---
    EffectFn fn = getEffect(inputState.currentMode);
    fn(pixelBuffer, NUM_PIXELS, ctx);

    // --- Copy pixel buffer to strip ---
    for (uint16_t i = 0; i < NUM_PIXELS; i++) {
        strip.setPixel(i, pixelBuffer[i]);
    }

    strip.show();
    frameCount++;
}
