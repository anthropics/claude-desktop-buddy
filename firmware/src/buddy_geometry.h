#pragma once
#include <cstdint>
#include <cmath>

struct GPoint { int16_t x, y; };

struct Primitive {
  GPoint  a, mid, b;
  uint8_t isPolyline;  // 0 = line (mid unused), 1 = apex polyline
};

enum PrimitiveKey {
  PRIM_topL = 0, PRIM_topM, PRIM_topR, PRIM_botL, PRIM_botM, PRIM_botR,
  PRIM_waistA, PRIM_waistB, PRIM_stubR, PRIM_stubL,
  PRIM_apexTop, PRIM_apexBot,
};
static const int COOL_S_COUNT = 12;

static const Primitive COOL_S[COOL_S_COUNT] = {
  /* topL    */ {{ 20, 60}, {0,0}, { 20,170}, 0},
  /* topM    */ {{100, 60}, {0,0}, {100,170}, 0},
  /* topR    */ {{180, 60}, {0,0}, {180,170}, 0},
  /* botL    */ {{ 20,230}, {0,0}, { 20,340}, 0},
  /* botM    */ {{100,230}, {0,0}, {100,340}, 0},
  /* botR    */ {{180,230}, {0,0}, {180,340}, 0},
  /* waistA  */ {{ 20,170}, {0,0}, {100,230}, 0},
  /* waistB  */ {{100,170}, {0,0}, {180,230}, 0},
  /* stubR   */ {{180,170}, {0,0}, {140,200}, 0},
  /* stubL   */ {{ 20,230}, {0,0}, { 60,200}, 0},
  /* apexTop */ {{ 20, 60}, {100,  0}, {180, 60}, 1},
  /* apexBot */ {{ 20,340}, {100,400}, {180,340}, 1},
};

inline void mapPoint(int16_t px, int16_t py, int16_t cx, int16_t cy,
                     float scale, int16_t& outX, int16_t& outY) {
  outX = (int16_t)lroundf(cx + scale * (px - 100));
  outY = (int16_t)lroundf(cy + scale * (py - 200));
}
