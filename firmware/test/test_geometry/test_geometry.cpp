#include <unity.h>
#include "buddy_geometry.h"

void test_primitive_count() {
    TEST_ASSERT_EQUAL_INT(12, COOL_S_COUNT);
}

void test_known_primitives() {
    TEST_ASSERT_EQUAL_INT(20,  COOL_S[PRIM_topL].a.x);
    TEST_ASSERT_EQUAL_INT(60,  COOL_S[PRIM_topL].a.y);
    TEST_ASSERT_EQUAL_INT(20,  COOL_S[PRIM_topL].b.x);
    TEST_ASSERT_EQUAL_INT(170, COOL_S[PRIM_topL].b.y);
    TEST_ASSERT_EQUAL_INT(180, COOL_S[PRIM_stubR].a.x);
    TEST_ASSERT_EQUAL_INT(140, COOL_S[PRIM_stubR].b.x);
    TEST_ASSERT_EQUAL_INT(200, COOL_S[PRIM_stubR].b.y);
    TEST_ASSERT_EQUAL_INT(60,  COOL_S[PRIM_stubL].b.x);
}

void test_apex_polylines() {
    TEST_ASSERT_EQUAL_INT(100, COOL_S[PRIM_apexTop].mid.x);
    TEST_ASSERT_EQUAL_INT(0,   COOL_S[PRIM_apexTop].mid.y);
    TEST_ASSERT_EQUAL_INT(1,   COOL_S[PRIM_apexTop].isPolyline);
    TEST_ASSERT_EQUAL_INT(400, COOL_S[PRIM_apexBot].mid.y);
    TEST_ASSERT_EQUAL_INT(0,   COOL_S[PRIM_topL].isPolyline);
}

void test_mapPoint() {
    int16_t x, y;
    mapPoint(20, 60, 160, 120, 0.55f, x, y);
    TEST_ASSERT_INT_WITHIN(1, 116, x);
    TEST_ASSERT_INT_WITHIN(1, 43,  y);
    mapPoint(180, 340, 160, 120, 0.55f, x, y);
    TEST_ASSERT_INT_WITHIN(1, 204, x);
    TEST_ASSERT_INT_WITHIN(1, 197, y);
}

void setUp() {} void tearDown() {}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_primitive_count);
    RUN_TEST(test_known_primitives);
    RUN_TEST(test_apex_polylines);
    RUN_TEST(test_mapPoint);
    return UNITY_END();
}
