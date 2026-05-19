#pragma once
#include <stdint.h>

// Audio HAL for CYD (ESP32-2432S028R).
// GPIO 26 (DAC2) driven as LEDC tone output. Mirrors M5.Beep.tone() semantics:
// start tone at freq_hz, auto-stop after duration_ms (non-blocking).
// audioLoop() must be called from the main loop to honour auto-stop.

void audioBegin();
void boardTone(uint16_t freq_hz, uint16_t duration_ms);
void audioLoop();
