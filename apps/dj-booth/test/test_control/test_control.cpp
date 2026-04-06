#include <unity.h>

#include "config.h"
#include "control.h"
#include "state.h"

void setUp(void) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ControlStatus::Ok), static_cast<uint8_t>(controlBegin()));
}

void tearDown(void) {}

void test_control_begin_returns_ok(void) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ControlStatus::Ok), static_cast<uint8_t>(controlBegin()));
}

void test_control_process_applies_set_power(void) {
    LightState state = makeDefaultLightState();
    lightStateBeginFrame(state);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makePowerCommand(ControlSource::WebUI, false)))
    );

    TEST_ASSERT_EQUAL_UINT8(1, controlProcess(state));
    TEST_ASSERT_FALSE(state.powerOn);
}

void test_control_process_applies_set_brightness(void) {
    LightState state = makeDefaultLightState();
    lightStateBeginFrame(state);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makeBrightnessCommand(ControlSource::WebUI, 42)))
    );

    TEST_ASSERT_EQUAL_UINT8(1, controlProcess(state));
    TEST_ASSERT_EQUAL_UINT8(42, state.brightness);
}

void test_control_process_applies_set_color_and_switches_to_solid(void) {
    LightState state = makeDefaultLightState();
    state.effectMode = EffectMode::Spectrum;
    lightStateBeginFrame(state);

    const HSV color{120, 200, 180};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makeColorCommand(ControlSource::WebUI, color)))
    );

    TEST_ASSERT_EQUAL_UINT8(1, controlProcess(state));
    TEST_ASSERT_EQUAL_UINT16(120, state.color.h);
    TEST_ASSERT_EQUAL_UINT8(200, state.color.s);
    TEST_ASSERT_EQUAL_UINT8(180, state.color.v);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EffectMode::Solid), static_cast<uint8_t>(state.effectMode));
    TEST_ASSERT_TRUE(state.modeChanged);
}

void test_control_process_applies_set_effect(void) {
    LightState state = makeDefaultLightState();
    lightStateBeginFrame(state);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makeEffectCommand(ControlSource::WebUI, EffectMode::Twinkle)))
    );

    TEST_ASSERT_EQUAL_UINT8(1, controlProcess(state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EffectMode::Twinkle), static_cast<uint8_t>(state.effectMode));
    TEST_ASSERT_TRUE(state.modeChanged);
}

void test_control_process_applies_next_effect(void) {
    LightState state = makeDefaultLightState();
    lightStateBeginFrame(state);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makeNextEffectCommand(ControlSource::WebUI)))
    );

    TEST_ASSERT_EQUAL_UINT8(1, controlProcess(state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EffectMode::RainbowCycle), static_cast<uint8_t>(state.effectMode));
    TEST_ASSERT_TRUE(state.modeChanged);
}

void test_control_queue_reports_overflow(void) {
    for (uint8_t index = 0; index < CONTROL_QUEUE_LENGTH; index++) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(ControlStatus::Ok),
            static_cast<uint8_t>(controlEnqueue(makeBrightnessCommand(ControlSource::WebUI, index)))
        );
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::QueueFull),
        static_cast<uint8_t>(controlEnqueue(makeBrightnessCommand(ControlSource::WebUI, 255)))
    );
}

void test_control_process_drains_all_queued_commands(void) {
    LightState state = makeDefaultLightState();
    lightStateBeginFrame(state);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makePowerCommand(ControlSource::WebUI, false)))
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makeBrightnessCommand(ControlSource::WebUI, 77)))
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlStatus::Ok),
        static_cast<uint8_t>(controlEnqueue(makeEffectCommand(ControlSource::WebUI, EffectMode::Fire)))
    );

    TEST_ASSERT_EQUAL_UINT8(3, controlProcess(state));
    TEST_ASSERT_FALSE(state.powerOn);
    TEST_ASSERT_EQUAL_UINT8(77, state.brightness);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EffectMode::Fire), static_cast<uint8_t>(state.effectMode));
    TEST_ASSERT_EQUAL_UINT8(0, controlProcess(state));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_control_begin_returns_ok);
    RUN_TEST(test_control_process_applies_set_power);
    RUN_TEST(test_control_process_applies_set_brightness);
    RUN_TEST(test_control_process_applies_set_color_and_switches_to_solid);
    RUN_TEST(test_control_process_applies_set_effect);
    RUN_TEST(test_control_process_applies_next_effect);
    RUN_TEST(test_control_queue_reports_overflow);
    RUN_TEST(test_control_process_drains_all_queued_commands);
    return UNITY_END();
}
