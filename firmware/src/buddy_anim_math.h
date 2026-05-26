#pragma once
#include <cstdint>
#include <cmath>

// clamp01 — clamp x into [0,1].
inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// sineCycle — 0.5 + 0.5*sin over one period (ms). Returns [0,1], 0.5 at t=0.
inline float sineCycle(float tMs, float periodMs) {
  return 0.5f + 0.5f * std::sin(2.0f * (float)M_PI * tMs / periodMs);
}

// rampIndex8 maps to the PAL8 layout (0..59 green ramp; White=60, red=61).
// rampIndex4 maps to the PAL4 layout (0..13 green ramp; White=14, red=15).
// NOTE: the runtime renderer in buddy_coolS.cpp uses the depth-aware greenIdx()
// helper instead — do NOT call rampIndex8/rampIndex4 directly in render code.
// These helpers exist for unit tests and must not be removed.
inline int rampIndex8(float o) { return (int)lroundf(clamp01(o) * 59.0f); }
inline int rampIndex4(float o) { return (int)lroundf(clamp01(o) * 13.0f); }
