#pragma once
#include <stdint.h>

struct PowerStatus {
    uint8_t  pct;   // battery percentage, always 100 on CYD (USB only)
    uint16_t mV;    // millivolts, always 5000
    int16_t  mA;    // current draw (signed); always 0 -- we don't measure
    bool     usb;   // always true on CYD
};

extern bool screenOff;

void powerBegin();
void powerBacklight(int pct);
void powerScreen(bool on);
void powerSleep();
PowerStatus powerStatus();
