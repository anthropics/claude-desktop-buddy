#pragma once
#include "buddy_geometry.h"
#include "buddy_anim_math.h"

inline void idleRevealOpacities(float p, float op[COOL_S_COUNT]) {
  for (int i = 0; i < COOL_S_COUNT; ++i) op[i] = 0.0f;

  auto reveal = [&](const int* keys, int n, float fromP, float toP) {
    float k = clamp01((p - fromP) / (toP - fromP));
    for (int i = 0; i < n; ++i) {
      float slot = (float)i / n, slotEnd = (float)(i + 1) / n;
      op[keys[i]] = clamp01((k - slot) / (slotEnd - slot));
    }
  };
  static const int verticals[6] = {PRIM_topL, PRIM_topM, PRIM_topR,
                                   PRIM_botL, PRIM_botM, PRIM_botR};
  static const int diagonals[4] = {PRIM_waistA, PRIM_waistB, PRIM_stubR, PRIM_stubL};
  static const int apexes[2]    = {PRIM_apexTop, PRIM_apexBot};

  if (p < 0.4f) {
    reveal(verticals, 6, 0.0f, 0.4f);
  } else if (p < 0.7f) {
    for (int i = 0; i < 6; ++i) op[verticals[i]] = 1.0f;
    reveal(diagonals, 4, 0.4f, 0.7f);
  } else if (p < 0.85f) {
    for (int i = 0; i < 6; ++i) op[verticals[i]] = 1.0f;
    for (int i = 0; i < 4; ++i) op[diagonals[i]] = 1.0f;
    reveal(apexes, 2, 0.7f, 0.80f);
  } else {
    float fade = clamp01((p - 0.92f) / 0.08f);
    for (int i = 0; i < COOL_S_COUNT; ++i) op[i] = 1.0f - fade;
  }
}
