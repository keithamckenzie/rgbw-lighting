#include <unity.h>

#include "state.h"

void setUp(void) {}
void tearDown(void) {}

void test_make_default_light_state_uses_config_defaults(void) {
    const LightState state = makeDefaultLightState();

    TEST_ASSERT_TRUE(state.powerOn);
    TEST_ASSERT_EQUAL_UINT8(160, state.brightness);
    TEST_ASSERT_EQUAL_UINT16(0, state.color.h);
    TEST_ASSERT_EQUAL_UINT8(255, state.color.s);
    TEST_ASSERT_EQUAL_UINT8(255, state.color.v);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EffectMode::Solid), static_cast<uint8_t>(state.effectMode));
    TEST_ASSERT_TRUE(state.modeChanged);
}

void test_normalize_hsv_wraps_hue_values(void) {
    TEST_ASSERT_EQUAL_UINT16(0, normalizeHSV(HSV{720, 100, 200}).h);
    TEST_ASSERT_EQUAL_UINT16(1, normalizeHSV(HSV{361, 100, 200}).h);
    TEST_ASSERT_EQUAL_UINT16(180, normalizeHSV(HSV{180, 100, 200}).h);
}

void test_light_state_begin_frame_clears_mode_changed(void) {
    LightState state = makeDefaultLightState();
    state.modeChanged = true;

    lightStateBeginFrame(state);

    TEST_ASSERT_FALSE(state.modeChanged);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_make_default_light_state_uses_config_defaults);
    RUN_TEST(test_normalize_hsv_wraps_hue_values);
    RUN_TEST(test_light_state_begin_frame_clears_mode_changed);
    return UNITY_END();
}
