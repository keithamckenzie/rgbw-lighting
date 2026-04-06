#include <unity.h>

#include "matrix.h"

void setUp(void) {}
void tearDown(void) {}

void test_origin_maps_to_first_pixel(void) {
    TEST_ASSERT_EQUAL_UINT16(0, mapXY(0, 0));
}

void test_top_row_maps_left_to_right(void) {
    TEST_ASSERT_EQUAL_UINT16(25, mapXY(25, 0));
}

void test_second_row_reverses_for_serpentine_layout(void) {
    TEST_ASSERT_EQUAL_UINT16(51, mapXY(0, 1));
    TEST_ASSERT_EQUAL_UINT16(26, mapXY(25, 1));
}

void test_bottom_row_corner_cases_match_26_by_32_grid(void) {
    // Row 31 is odd → serpentine reversed: x=0 maps to end, x=25 maps to start
    TEST_ASSERT_EQUAL_UINT16(831, mapXY(0, 31));
    TEST_ASSERT_EQUAL_UINT16(806, mapXY(25, 31));
}

void test_out_of_bounds_returns_sentinel(void) {
    TEST_ASSERT_EQUAL_UINT16(NUM_PIXELS, mapXY(26, 0));
    TEST_ASSERT_EQUAL_UINT16(NUM_PIXELS, mapXY(0, 32));
    TEST_ASSERT_EQUAL_UINT16(NUM_PIXELS, mapXY(26, 32));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_origin_maps_to_first_pixel);
    RUN_TEST(test_top_row_maps_left_to_right);
    RUN_TEST(test_second_row_reverses_for_serpentine_layout);
    RUN_TEST(test_bottom_row_corner_cases_match_26_by_32_grid);
    RUN_TEST(test_out_of_bounds_returns_sentinel);
    return UNITY_END();
}
