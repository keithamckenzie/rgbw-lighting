#include <unity.h>
#include "matrix.h"

void setUp(void) {}
void tearDown(void) {}

// ====================================================================
// Universal tests — valid for all wiring patterns
// ====================================================================

void test_origin_top_left(void) {
    // (0,0) is always index 0 regardless of wiring pattern
    TEST_ASSERT_EQUAL_UINT16(0, mapXY(0, 0));
}

void test_x_out_of_bounds(void) {
    TEST_ASSERT_EQUAL_UINT16(NUM_PIXELS, mapXY(PANEL_WIDTH, 0));
}

void test_y_out_of_bounds(void) {
    TEST_ASSERT_EQUAL_UINT16(NUM_PIXELS, mapXY(0, PANEL_HEIGHT));
}

void test_both_out_of_bounds(void) {
    TEST_ASSERT_EQUAL_UINT16(NUM_PIXELS, mapXY(PANEL_WIDTH, PANEL_HEIGHT));
}

void test_all_indices_in_range(void) {
    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            uint16_t idx = mapXY(x, y);
            TEST_ASSERT_TRUE_MESSAGE(idx < NUM_PIXELS, "Index out of range");
        }
    }
}

void test_all_indices_unique(void) {
    static bool seen[NUM_PIXELS];
    for (uint16_t i = 0; i < NUM_PIXELS; i++) {
        seen[i] = false;
    }

    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            uint16_t idx = mapXY(x, y);
            if (idx >= NUM_PIXELS) {
                TEST_FAIL_MESSAGE("mapXY returned sentinel for in-range coordinate");
                return;
            }
            TEST_ASSERT_FALSE_MESSAGE(seen[idx], "Duplicate index found");
            seen[idx] = true;
        }
    }

    // Every index should be covered
    for (uint16_t i = 0; i < NUM_PIXELS; i++) {
        TEST_ASSERT_TRUE_MESSAGE(seen[i], "Index not covered by any coordinate");
    }
}

// ====================================================================
// Pattern-specific tests
// ====================================================================

#if WIRING_PATTERN == WIRING_SERPENTINE_H

void test_top_right(void) {
    TEST_ASSERT_EQUAL_UINT16(PANEL_WIDTH - 1, mapXY(PANEL_WIDTH - 1, 0));
}

void test_bottom_left(void) {
    uint16_t row = PANEL_HEIGHT - 1;
    uint16_t expected = (row & 1)
        ? row * PANEL_WIDTH + (PANEL_WIDTH - 1)
        : row * PANEL_WIDTH;
    TEST_ASSERT_EQUAL_UINT16(expected, mapXY(0, PANEL_HEIGHT - 1));
}

void test_bottom_right(void) {
    uint16_t row = PANEL_HEIGHT - 1;
    uint16_t expected = (row & 1)
        ? row * PANEL_WIDTH
        : row * PANEL_WIDTH + (PANEL_WIDTH - 1);
    TEST_ASSERT_EQUAL_UINT16(expected, mapXY(PANEL_WIDTH - 1, PANEL_HEIGHT - 1));
}

void test_even_row_sequential(void) {
    // Row 0 (even): left-to-right
    for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
        TEST_ASSERT_EQUAL_UINT16(x, mapXY(x, 0));
    }
}

void test_odd_row_reversed(void) {
#if PANEL_HEIGHT < 2
    TEST_IGNORE_MESSAGE("Requires PANEL_HEIGHT >= 2");
#else
    // Row 1 (odd): right-to-left
    for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
        uint16_t expected = 1 * PANEL_WIDTH + (PANEL_WIDTH - 1 - x);
        TEST_ASSERT_EQUAL_UINT16(expected, mapXY(x, 1));
    }
#endif
}

#elif WIRING_PATTERN == WIRING_PROGRESSIVE_H

void test_top_right(void) {
    TEST_ASSERT_EQUAL_UINT16(PANEL_WIDTH - 1, mapXY(PANEL_WIDTH - 1, 0));
}

void test_bottom_left(void) {
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(PANEL_HEIGHT - 1) * PANEL_WIDTH,
                             mapXY(0, PANEL_HEIGHT - 1));
}

void test_bottom_right(void) {
    TEST_ASSERT_EQUAL_UINT16(NUM_PIXELS - 1,
                             mapXY(PANEL_WIDTH - 1, PANEL_HEIGHT - 1));
}

void test_all_rows_sequential(void) {
    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        for (uint16_t x = 0; x < PANEL_WIDTH; x++) {
            TEST_ASSERT_EQUAL_UINT16(y * PANEL_WIDTH + x, mapXY(x, y));
        }
    }
}

#elif WIRING_PATTERN == WIRING_SERPENTINE_V

void test_top_right(void) {
    uint16_t col = PANEL_WIDTH - 1;
    uint16_t expected = (col & 1)
        ? col * PANEL_HEIGHT + (PANEL_HEIGHT - 1)
        : col * PANEL_HEIGHT;
    TEST_ASSERT_EQUAL_UINT16(expected, mapXY(PANEL_WIDTH - 1, 0));
}

void test_bottom_left(void) {
    TEST_ASSERT_EQUAL_UINT16(PANEL_HEIGHT - 1, mapXY(0, PANEL_HEIGHT - 1));
}

void test_bottom_right(void) {
    uint16_t col = PANEL_WIDTH - 1;
    uint16_t expected = (col & 1)
        ? col * PANEL_HEIGHT
        : col * PANEL_HEIGHT + (PANEL_HEIGHT - 1);
    TEST_ASSERT_EQUAL_UINT16(expected, mapXY(PANEL_WIDTH - 1, PANEL_HEIGHT - 1));
}

void test_even_col_sequential(void) {
    // Column 0 (even): top-to-bottom
    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        TEST_ASSERT_EQUAL_UINT16(y, mapXY(0, y));
    }
}

void test_odd_col_reversed(void) {
#if PANEL_WIDTH < 2
    TEST_IGNORE_MESSAGE("Requires PANEL_WIDTH >= 2");
#else
    // Column 1 (odd): bottom-to-top
    for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
        uint16_t expected = 1 * PANEL_HEIGHT + (PANEL_HEIGHT - 1 - y);
        TEST_ASSERT_EQUAL_UINT16(expected, mapXY(1, y));
    }
#endif
}

#endif

// ====================================================================
// Test runner
// ====================================================================

int main(void) {
    UNITY_BEGIN();

    // Universal
    RUN_TEST(test_origin_top_left);
    RUN_TEST(test_x_out_of_bounds);
    RUN_TEST(test_y_out_of_bounds);
    RUN_TEST(test_both_out_of_bounds);
    RUN_TEST(test_all_indices_in_range);
    RUN_TEST(test_all_indices_unique);

    // Pattern-specific
    RUN_TEST(test_top_right);
    RUN_TEST(test_bottom_left);
    RUN_TEST(test_bottom_right);

#if WIRING_PATTERN == WIRING_SERPENTINE_H
    RUN_TEST(test_even_row_sequential);
    RUN_TEST(test_odd_row_reversed);
#elif WIRING_PATTERN == WIRING_PROGRESSIVE_H
    RUN_TEST(test_all_rows_sequential);
#elif WIRING_PATTERN == WIRING_SERPENTINE_V
    RUN_TEST(test_even_col_sequential);
    RUN_TEST(test_odd_col_reversed);
#endif

    return UNITY_END();
}
