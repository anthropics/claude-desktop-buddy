#pragma once
#include <cstdint>
#include <cmath>

static const int PAL8_GREEN_MAX = 59;
static const int PAL8_WHITE     = 60;
static const int PAL8_RED       = 61;
static const int PAL8_SIZE      = 62;

static const int PAL4_GREEN_MAX = 13;
static const int PAL4_WHITE     = 14;
static const int PAL4_RED       = 15;
static const int PAL4_SIZE      = 16;

inline uint16_t rgb565(int r, int g, int b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

inline uint16_t greenRamp(float f) {
  if (f < 0) f = 0; if (f > 1) f = 1;
  return rgb565(0, (int)lroundf(255.0f * f), (int)lroundf(102.0f * f));
}

inline void buildPalette8(uint16_t pal[PAL8_SIZE]) {
  for (int i = 0; i <= PAL8_GREEN_MAX; ++i)
    pal[i] = greenRamp((float)i / PAL8_GREEN_MAX);
  pal[PAL8_WHITE] = 0xFFFF;
  pal[PAL8_RED]   = rgb565(0xFF, 0x4D, 0x6D);
}

inline void buildPalette4(uint16_t pal[PAL4_SIZE]) {
  for (int i = 0; i <= PAL4_GREEN_MAX; ++i)
    pal[i] = greenRamp((float)i / PAL4_GREEN_MAX);
  pal[PAL4_WHITE] = 0xFFFF;
  pal[PAL4_RED]   = rgb565(0xFF, 0x4D, 0x6D);
}
