#include "power.h"
#include <Arduino.h>
#include <esp_sleep.h>

// LEDC: channel 0, 5 kHz base, 8-bit resolution. Backlight pin GPIO 21.
// (audio.cpp uses channel 2; led.cpp uses no LEDC.)
static const int BL_PIN  = 21;
static const int BL_CHAN = 0;
static const int BL_FREQ = 5000;
static const int BL_RES  = 8;

bool screenOff = false;

// Track last non-zero brightness so powerScreen(true) restores it. Default 80%.
static int s_lastPct = 80;

void powerBegin() {
    ledcSetup(BL_CHAN, BL_FREQ, BL_RES);
    ledcAttachPin(BL_PIN, BL_CHAN);
    powerBacklight(s_lastPct);
    screenOff = false;
}

void powerBacklight(int pct) {
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    if (pct > 0)   s_lastPct = pct;
    uint8_t duty = (uint8_t)((pct * 255) / 100);
    ledcWrite(BL_CHAN, duty);
}

void powerScreen(bool on) {
    if (on) {
        ledcWrite(BL_CHAN, (uint8_t)((s_lastPct * 255) / 100));
        screenOff = false;
    } else {
        ledcWrite(BL_CHAN, 0);
        screenOff = true;
    }
}

void powerSleep() {
    ledcWrite(BL_CHAN, 0);
    esp_deep_sleep_start();
}

PowerStatus powerStatus() {
    return PowerStatus{100, 5000, 0, true};
}
