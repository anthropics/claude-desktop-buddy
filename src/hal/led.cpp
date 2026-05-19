#include "led.h"
#include <Arduino.h>

// CYD onboard RGB LED pins (active-LOW, direct GPIO, no PWM).
static const uint8_t LED_R_PIN = 4;
static const uint8_t LED_G_PIN = 16;
static const uint8_t LED_B_PIN = 17;

void ledBegin() {
    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    // Drive HIGH (off) before any other code can observe the LED to
    // avoid the "all colours on at boot" flash from floating inputs.
    ledSet(0, 0, 0);
}

void ledSet(uint8_t r, uint8_t g, uint8_t b) {
    // Active-LOW: LOW lights the channel, HIGH turns it off.
    digitalWrite(LED_R_PIN, r ? LOW : HIGH);
    digitalWrite(LED_G_PIN, g ? LOW : HIGH);
    digitalWrite(LED_B_PIN, b ? LOW : HIGH);
}
