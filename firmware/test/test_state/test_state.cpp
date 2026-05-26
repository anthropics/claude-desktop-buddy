#include <unity.h>
#include "buddy_state.h"

void test_sleep_when_disconnected() {
    PersonaInputs in = {false, 0, 0, 1000, false, PS_IDLE};
    TEST_ASSERT_EQUAL_INT(PS_SLEEP, derivePersonaState(in));
}
void test_heart_beats_busy() {
    PersonaInputs in = {true, 3, 2000, 1000, false, PS_IDLE};
    TEST_ASSERT_EQUAL_INT(PS_HEART, derivePersonaState(in));
}
void test_busy_when_running() {
    PersonaInputs in = {true, 2, 0, 1000, false, PS_IDLE};
    TEST_ASSERT_EQUAL_INT(PS_BUSY, derivePersonaState(in));
}
void test_idle_when_connected_quiet() {
    PersonaInputs in = {true, 0, 0, 1000, false, PS_IDLE};
    TEST_ASSERT_EQUAL_INT(PS_IDLE, derivePersonaState(in));
}
void test_heart_expires() {
    PersonaInputs in = {true, 0, 500, 1000, false, PS_IDLE};
    TEST_ASSERT_EQUAL_INT(PS_IDLE, derivePersonaState(in));
}
void test_debug_overrides_everything() {
    PersonaInputs in = {false, 0, 0, 1000, true, PS_DIZZY};
    TEST_ASSERT_EQUAL_INT(PS_DIZZY, derivePersonaState(in));
}

void setUp() {} void tearDown() {}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sleep_when_disconnected);
    RUN_TEST(test_heart_beats_busy);
    RUN_TEST(test_busy_when_running);
    RUN_TEST(test_idle_when_connected_quiet);
    RUN_TEST(test_heart_expires);
    RUN_TEST(test_debug_overrides_everything);
    return UNITY_END();
}
