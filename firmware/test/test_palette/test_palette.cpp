#include <unity.h>
#include "buddy_palette.h"

void test_8bpp_endpoints() {
    uint16_t pal[PAL8_SIZE];
    buildPalette8(pal);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pal[0]);
    TEST_ASSERT_EQUAL_HEX16(0x07EC, pal[PAL8_GREEN_MAX]);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, pal[PAL8_WHITE]);
    TEST_ASSERT_EQUAL_HEX16(0xFA6D, pal[PAL8_RED]);
}
void test_8bpp_monotonic_green() {
    uint16_t pal[PAL8_SIZE];
    buildPalette8(pal);
    int prevG = -1;
    for (int i = 0; i <= PAL8_GREEN_MAX; ++i) {
        int g = (pal[i] >> 5) & 0x3F;
        TEST_ASSERT_TRUE(g >= prevG);
        prevG = g;
    }
}
void test_4bpp_layout() {
    uint16_t pal[PAL4_SIZE];
    buildPalette4(pal);
    TEST_ASSERT_EQUAL_INT(16, PAL4_SIZE);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pal[0]);
    TEST_ASSERT_EQUAL_HEX16(0x07EC, pal[PAL4_GREEN_MAX]);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, pal[PAL4_WHITE]);
    TEST_ASSERT_EQUAL_HEX16(0xFA6D, pal[PAL4_RED]);
}

void setUp() {} void tearDown() {}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_8bpp_endpoints);
    RUN_TEST(test_8bpp_monotonic_green);
    RUN_TEST(test_4bpp_layout);
    return UNITY_END();
}
