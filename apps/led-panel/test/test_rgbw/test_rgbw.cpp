#include <unity.h>
#include "rgbw.h"

void setUp(void) {}
void tearDown(void) {}

// ====================================================================
// scaleBrightness() tests
// ====================================================================

void test_scaleBrightness_full(void) {
    // Regression: brightness=255 must return exact input values
    RGBW result = scaleBrightness(RGBW(255, 255, 255, 255), 255);
    TEST_ASSERT_EQUAL_UINT8(255, result.r);
    TEST_ASSERT_EQUAL_UINT8(255, result.g);
    TEST_ASSERT_EQUAL_UINT8(255, result.b);
    TEST_ASSERT_EQUAL_UINT8(255, result.w);
}

void test_scaleBrightness_zero(void) {
    RGBW result = scaleBrightness(RGBW(255, 128, 64, 32), 0);
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_scaleBrightness_half(void) {
    RGBW result = scaleBrightness(RGBW(200, 100, 50, 25), 128);
    // (200 * 129) >> 8 = 100, (100 * 129) >> 8 = 50, etc.
    TEST_ASSERT_EQUAL_UINT8(100, result.r);
    TEST_ASSERT_EQUAL_UINT8(50, result.g);
    TEST_ASSERT_EQUAL_UINT8(25, result.b);
    TEST_ASSERT_EQUAL_UINT8(12, result.w);
}

void test_scaleBrightness_black(void) {
    RGBW result = scaleBrightness(RGBW(0, 0, 0, 0), 255);
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_scaleBrightness_one(void) {
    // brightness=1: (255 * 2) >> 8 = 1
    RGBW result = scaleBrightness(RGBW(255, 255, 255, 255), 1);
    TEST_ASSERT_EQUAL_UINT8(1, result.r);
    TEST_ASSERT_EQUAL_UINT8(1, result.g);
    TEST_ASSERT_EQUAL_UINT8(1, result.b);
    TEST_ASSERT_EQUAL_UINT8(1, result.w);
}

void test_scaleBrightness_preserves_ratio(void) {
    // Channels with 2:1 ratio should stay roughly 2:1
    RGBW result = scaleBrightness(RGBW(200, 100, 0, 0), 200);
    TEST_ASSERT_TRUE(result.r > result.g);
    // Allow +-1 for rounding
    TEST_ASSERT_INT_WITHIN(1, result.g * 2, result.r);
}

// ====================================================================
// hsvToRGBW() tests
// ====================================================================

void test_hsvToRGBW_pure_red(void) {
    HSV hsv = {0, 255, 255};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_EQUAL_UINT8(255, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_hsvToRGBW_pure_green(void) {
    HSV hsv = {120, 255, 255};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(255, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_hsvToRGBW_pure_blue(void) {
    HSV hsv = {240, 255, 255};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(255, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_hsvToRGBW_white_on_w_channel(void) {
    // s=0 means pure white — should go to W channel
    HSV hsv = {0, 0, 200};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(200, result.w);
}

void test_hsvToRGBW_black(void) {
    HSV hsv = {0, 255, 0};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_hsvToRGBW_partial_saturation(void) {
    // Partial saturation should produce non-zero W
    HSV hsv = {0, 128, 255};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_TRUE(result.w > 0);
    TEST_ASSERT_TRUE(result.r > 0);
}

void test_hsvToRGBW_yellow(void) {
    // Hue=60 is yellow (R+G)
    HSV hsv = {60, 255, 255};
    RGBW result = hsvToRGBW(hsv);
    TEST_ASSERT_TRUE(result.r > 0 || result.w > 0);
    TEST_ASSERT_TRUE(result.g > 0 || result.w > 0);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
}

void test_hsvToRGBW_hue_360_is_region5(void) {
    // Hue 360 falls into default case (region 5: 300-360)
    // which is the same region as hue 300 (magenta)
    HSV hsv = {360, 255, 255};
    RGBW result = hsvToRGBW(hsv);
    // region=6 -> default -> r=v, g=p, b=q
    // At hue=360, remainder = (360 - 360) * 255 / 60 = 0
    // So q = v * (255 - (s * 0) >> 8) >> 8 = v * 255 >> 8 = 254
    TEST_ASSERT_EQUAL_UINT8(255, result.r);
    TEST_ASSERT_TRUE(result.b > 0);  // Non-zero blue in magenta region
}

// ====================================================================
// lerpRGBW() tests
// ====================================================================

void test_lerpRGBW_t0_returns_a(void) {
    RGBW a(100, 50, 25, 12);
    RGBW b(200, 150, 125, 112);
    RGBW result = lerpRGBW(a, b, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(a.r, result.r);
    TEST_ASSERT_EQUAL_UINT8(a.g, result.g);
    TEST_ASSERT_EQUAL_UINT8(a.b, result.b);
    TEST_ASSERT_EQUAL_UINT8(a.w, result.w);
}

void test_lerpRGBW_t1_returns_b(void) {
    RGBW a(100, 50, 25, 12);
    RGBW b(200, 150, 125, 112);
    RGBW result = lerpRGBW(a, b, 1.0f);
    TEST_ASSERT_EQUAL_UINT8(b.r, result.r);
    TEST_ASSERT_EQUAL_UINT8(b.g, result.g);
    TEST_ASSERT_EQUAL_UINT8(b.b, result.b);
    TEST_ASSERT_EQUAL_UINT8(b.w, result.w);
}

void test_lerpRGBW_midpoint(void) {
    RGBW a(0, 0, 0, 0);
    RGBW b(200, 100, 50, 250);
    RGBW result = lerpRGBW(a, b, 0.5f);
    TEST_ASSERT_EQUAL_UINT8(100, result.r);
    TEST_ASSERT_EQUAL_UINT8(50, result.g);
    TEST_ASSERT_EQUAL_UINT8(25, result.b);
    TEST_ASSERT_EQUAL_UINT8(125, result.w);
}

void test_lerpRGBW_clamp_below(void) {
    RGBW a(100, 50, 25, 12);
    RGBW b(200, 150, 125, 112);
    RGBW result = lerpRGBW(a, b, -0.5f);
    TEST_ASSERT_EQUAL_UINT8(a.r, result.r);
    TEST_ASSERT_EQUAL_UINT8(a.g, result.g);
    TEST_ASSERT_EQUAL_UINT8(a.b, result.b);
    TEST_ASSERT_EQUAL_UINT8(a.w, result.w);
}

void test_lerpRGBW_clamp_above(void) {
    RGBW a(100, 50, 25, 12);
    RGBW b(200, 150, 125, 112);
    RGBW result = lerpRGBW(a, b, 1.5f);
    TEST_ASSERT_EQUAL_UINT8(b.r, result.r);
    TEST_ASSERT_EQUAL_UINT8(b.g, result.g);
    TEST_ASSERT_EQUAL_UINT8(b.b, result.b);
    TEST_ASSERT_EQUAL_UINT8(b.w, result.w);
}

void test_lerpRGBW_same_color(void) {
    RGBW c(42, 42, 42, 42);
    RGBW result = lerpRGBW(c, c, 0.5f);
    TEST_ASSERT_EQUAL_UINT8(42, result.r);
    TEST_ASSERT_EQUAL_UINT8(42, result.g);
    TEST_ASSERT_EQUAL_UINT8(42, result.b);
    TEST_ASSERT_EQUAL_UINT8(42, result.w);
}

// ====================================================================
// rgbwToRgb() tests
// ====================================================================

void test_rgbwToRgb_white_fold(void) {
    // White channel folds into RGB
    RGBW result = rgbwToRgb(RGBW(100, 50, 25, 50));
    TEST_ASSERT_EQUAL_UINT8(150, result.r);
    TEST_ASSERT_EQUAL_UINT8(100, result.g);
    TEST_ASSERT_EQUAL_UINT8(75, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_rgbwToRgb_clamp_at_255(void) {
    RGBW result = rgbwToRgb(RGBW(200, 200, 200, 200));
    TEST_ASSERT_EQUAL_UINT8(255, result.r);
    TEST_ASSERT_EQUAL_UINT8(255, result.g);
    TEST_ASSERT_EQUAL_UINT8(255, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_rgbwToRgb_pure_white(void) {
    RGBW result = rgbwToRgb(RGBW(0, 0, 0, 255));
    TEST_ASSERT_EQUAL_UINT8(255, result.r);
    TEST_ASSERT_EQUAL_UINT8(255, result.g);
    TEST_ASSERT_EQUAL_UINT8(255, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_rgbwToRgb_no_white(void) {
    RGBW result = rgbwToRgb(RGBW(100, 50, 25, 0));
    TEST_ASSERT_EQUAL_UINT8(100, result.r);
    TEST_ASSERT_EQUAL_UINT8(50, result.g);
    TEST_ASSERT_EQUAL_UINT8(25, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

void test_rgbwToRgb_black(void) {
    RGBW result = rgbwToRgb(RGBW(0, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(0, result.r);
    TEST_ASSERT_EQUAL_UINT8(0, result.g);
    TEST_ASSERT_EQUAL_UINT8(0, result.b);
    TEST_ASSERT_EQUAL_UINT8(0, result.w);
}

// ====================================================================
// applyGamma() tests
// ====================================================================

void test_applyGamma_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, applyGamma((uint8_t)0));
}

void test_applyGamma_max(void) {
    TEST_ASSERT_EQUAL_UINT8(255, applyGamma((uint8_t)255));
}

void test_applyGamma_midpoint_less_than_linear(void) {
    // Gamma 2.2 compresses midtones: gamma(128) should be ~56, well below 128
    uint8_t result = applyGamma((uint8_t)128);
    TEST_ASSERT_TRUE(result < 128);
    TEST_ASSERT_INT_WITHIN(5, 56, result);
}

void test_applyGamma_monotonic(void) {
    // Output must be non-decreasing across the full range
    uint8_t prev = applyGamma((uint8_t)0);
    for (uint16_t i = 1; i < 256; i++) {
        uint8_t curr = applyGamma((uint8_t)i);
        TEST_ASSERT_TRUE_MESSAGE(curr >= prev, "Gamma table is not monotonic");
        prev = curr;
    }
}

void test_applyGamma_rgbw_consistency(void) {
    // RGBW overload should match per-channel results
    RGBW input(100, 200, 50, 128);
    RGBW result = applyGamma(input);
    TEST_ASSERT_EQUAL_UINT8(applyGamma((uint8_t)100), result.r);
    TEST_ASSERT_EQUAL_UINT8(applyGamma((uint8_t)200), result.g);
    TEST_ASSERT_EQUAL_UINT8(applyGamma((uint8_t)50), result.b);
    TEST_ASSERT_EQUAL_UINT8(applyGamma((uint8_t)128), result.w);
}

// ====================================================================
// Test runner
// ====================================================================

int main(void) {
    UNITY_BEGIN();

    // scaleBrightness (6 tests)
    RUN_TEST(test_scaleBrightness_full);
    RUN_TEST(test_scaleBrightness_zero);
    RUN_TEST(test_scaleBrightness_half);
    RUN_TEST(test_scaleBrightness_black);
    RUN_TEST(test_scaleBrightness_one);
    RUN_TEST(test_scaleBrightness_preserves_ratio);

    // hsvToRGBW (8 tests)
    RUN_TEST(test_hsvToRGBW_pure_red);
    RUN_TEST(test_hsvToRGBW_pure_green);
    RUN_TEST(test_hsvToRGBW_pure_blue);
    RUN_TEST(test_hsvToRGBW_white_on_w_channel);
    RUN_TEST(test_hsvToRGBW_black);
    RUN_TEST(test_hsvToRGBW_partial_saturation);
    RUN_TEST(test_hsvToRGBW_yellow);
    RUN_TEST(test_hsvToRGBW_hue_360_is_region5);

    // lerpRGBW (6 tests)
    RUN_TEST(test_lerpRGBW_t0_returns_a);
    RUN_TEST(test_lerpRGBW_t1_returns_b);
    RUN_TEST(test_lerpRGBW_midpoint);
    RUN_TEST(test_lerpRGBW_clamp_below);
    RUN_TEST(test_lerpRGBW_clamp_above);
    RUN_TEST(test_lerpRGBW_same_color);

    // rgbwToRgb (5 tests)
    RUN_TEST(test_rgbwToRgb_white_fold);
    RUN_TEST(test_rgbwToRgb_clamp_at_255);
    RUN_TEST(test_rgbwToRgb_pure_white);
    RUN_TEST(test_rgbwToRgb_no_white);
    RUN_TEST(test_rgbwToRgb_black);

    // applyGamma (5 tests)
    RUN_TEST(test_applyGamma_zero);
    RUN_TEST(test_applyGamma_max);
    RUN_TEST(test_applyGamma_midpoint_less_than_linear);
    RUN_TEST(test_applyGamma_monotonic);
    RUN_TEST(test_applyGamma_rgbw_consistency);

    return UNITY_END();
}
