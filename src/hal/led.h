#pragma once
#include <stdint.h>

// Onboard RGB LED HAL for CYD (ESP32-2432S028R).
// Pins: R=GPIO4, G=GPIO16, B=GPIO17. Active-LOW (digitalWrite LOW = lit).
// ledSet treats any non-zero channel value as fully ON (no PWM).

void ledBegin();
void ledSet(uint8_t r, uint8_t g, uint8_t b);
