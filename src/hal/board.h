#pragma once

// Board HAL — CYD ESP32-2432S028R. Display on VSPI (default SPI), touch
// on HSPI (separate bus). Wired per PORT.md §3 pin map; build_flags in
// platformio.ini supply the TFT_eSPI pin constants, so the library can
// stay shared/unmodified across projects.

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

extern TFT_eSPI tft;
extern XPT2046_Touchscreen ts;

void boardBegin();
void boardLoop();
