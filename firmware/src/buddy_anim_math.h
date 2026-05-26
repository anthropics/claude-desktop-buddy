#pragma once
#include <cstdint>
#include <cmath>

// clamp01 — clamp x into [0,1].
inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// sineCycle — 0.5 + 0.5*sin over one period (ms). Returns [0,1], 0.5 at t=0.
inline float sineCycle(float tMs, float periodMs) {
  return 0.5f + 0.5f * std::sin(2.0f * (float)M_PI * tMs / periodMs);
}

// Map opacity o in [0,1] to an 8bpp green-ramp index (0..59). White=60, red=61.
inline int rampIndex8(float o) { return (int)lroundf(clamp01(o) * 59.0f); }

// Map opacity o in [0,1] to a 4bpp green-ramp index (0..13). White=14, red=15.
inline int rampIndex4(float o) { return (int)lroundf(clamp01(o) * 13.0f); }
