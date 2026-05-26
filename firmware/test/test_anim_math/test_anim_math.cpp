#include <unity.h>
#include "buddy_anim_math.h"

void test_clamp01_bounds() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, clamp01(-0.5f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, clamp01(1.5f));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, clamp01(0.25f));
}

void test_sineCycle_range_and_phase() {
    // sineCycle(t, period) = 0.5 + 0.5*sin(2*pi*t/period): in [0,1], 0.5 at t=0.
    TEST_ASSERT_FLOAT_WITHIN(1e-4, 0.5f, sineCycle(0.0f, 4000.0f));
    float v = sineCycle(1000.0f, 4000.0f);   // quarter cycle -> peak 1.0
    TEST_ASSERT_FLOAT_WITHIN(1e-3, 1.0f, v);
}

void test_rampIndex8() {
    // 8bpp ramp: green indices 0..59, white=60, red=61.
    TEST_ASSERT_EQUAL_INT(0,  rampIndex8(0.0f));
    TEST_ASSERT_EQUAL_INT(59, rampIndex8(1.0f));
    TEST_ASSERT_EQUAL_INT(30, rampIndex8(0.5f)); // round(0.5*59)=30
    TEST_ASSERT_EQUAL_INT(0,  rampIndex8(-1.0f));
    TEST_ASSERT_EQUAL_INT(59, rampIndex8(2.0f));
}

void test_rampIndex4() {
    // 4bpp ramp: green indices 0..13, white=14, red=15.
    TEST_ASSERT_EQUAL_INT(0,  rampIndex4(0.0f));
    TEST_ASSERT_EQUAL_INT(13, rampIndex4(1.0f));
    TEST_ASSERT_EQUAL_INT(7,  rampIndex4(0.5f)); // round(0.5*13)=7
}

void setUp() {} void tearDown() {}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_clamp01_bounds);
    RUN_TEST(test_sineCycle_range_and_phase);
    RUN_TEST(test_rampIndex8);
    RUN_TEST(test_rampIndex4);
    return UNITY_END();
}
