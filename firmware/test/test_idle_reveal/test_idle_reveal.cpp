#include <unity.h>
#include "buddy_idle_reveal.h"
#include "buddy_geometry.h"

void test_start_all_hidden() {
    float op[COOL_S_COUNT];
    idleRevealOpacities(0.0f, op);
    for (int i = 0; i < COOL_S_COUNT; ++i) TEST_ASSERT_FLOAT_WITHIN(1e-3, 0.0f, op[i]);
}
void test_midphase1_some_verticals_on() {
    float op[COOL_S_COUNT];
    idleRevealOpacities(0.2f, op);
    TEST_ASSERT_FLOAT_WITHIN(1e-3, 1.0f, op[PRIM_topL]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3, 0.0f, op[PRIM_waistA]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3, 0.0f, op[PRIM_apexTop]);
}
void test_phase3_full_figure() {
    float op[COOL_S_COUNT];
    idleRevealOpacities(0.84f, op);
    for (int i = 0; i < COOL_S_COUNT; ++i) TEST_ASSERT_TRUE(op[i] > 0.9f);
}
void test_final_fade() {
    float op[COOL_S_COUNT];
    idleRevealOpacities(1.0f, op);
    for (int i = 0; i < COOL_S_COUNT; ++i) TEST_ASSERT_FLOAT_WITHIN(1e-3, 0.0f, op[i]);
}

void setUp() {} void tearDown() {}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_start_all_hidden);
    RUN_TEST(test_midphase1_some_verticals_on);
    RUN_TEST(test_phase3_full_figure);
    RUN_TEST(test_final_fade);
    return UNITY_END();
}
