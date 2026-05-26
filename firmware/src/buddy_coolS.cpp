#include "buddy_coolS.h"
#include "buddy_geometry.h"
#include "buddy_anim_math.h"
#include "buddy_palette.h"
#include "buddy_idle_reveal.h"
#include <cmath>

// Active palette index lookups (set by buddyInit for the chosen depth).
static int gGreenMax = PAL8_GREEN_MAX;
static int gWhite     = PAL8_WHITE;
static int gRed       = PAL8_RED;

void buddyInit(BuddyDepth depth) {
  if (depth == DEPTH_8) { gGreenMax = PAL8_GREEN_MAX; gWhite = PAL8_WHITE; gRed = PAL8_RED; }
  else                  { gGreenMax = PAL4_GREEN_MAX; gWhite = PAL4_WHITE; gRed = PAL4_RED; }
}

// Green palette index for opacity o in [0,1] (depth-aware).
static inline int greenIdx(float o) { return (int)lroundf(clamp01(o) * gGreenMax); }

static inline int gGreenMax_idx() { return gGreenMax; }

// Draw one primitive as a wide line / two-segment polyline at palette index `ci`.
static void drawPrim(M5GFX_Sprite_t& spr, const Primitive& pr,
                     int16_t cx, int16_t cy, float scale, int ci, float width) {
  int16_t ax, ay, bx, by;
  mapPoint(pr.a.x, pr.a.y, cx, cy, scale, ax, ay);
  mapPoint(pr.b.x, pr.b.y, cx, cy, scale, bx, by);
  if (pr.isPolyline) {
    int16_t mx, my;
    mapPoint(pr.mid.x, pr.mid.y, cx, cy, scale, mx, my);
    spr.drawWideLine(ax, ay, mx, my, width, ci);
    spr.drawWideLine(mx, my, bx, by, width, ci);
  } else {
    spr.drawWideLine(ax, ay, bx, by, width, ci);
  }
}

// Draw the whole figure with per-primitive opacity (0..1). colorIdx selects the
// base color (greenIdx for green states; gWhite / gRed for flashes/heart).
// If forceIdx >= 0, all primitives use that exact palette index (white/red).
void drawCoolS(M5GFX_Sprite_t& spr, int16_t cx, int16_t cy, float scale,
               float width, const float opacity[COOL_S_COUNT], int forceIdx) {
  for (int i = 0; i < COOL_S_COUNT; ++i) {
    if (opacity[i] <= 0.0f) continue;
    int ci = (forceIdx >= 0) ? forceIdx : greenIdx(opacity[i]);
    drawPrim(spr, COOL_S[i], cx, cy, scale, ci, width);
  }
}

void buddyCoolSTick(M5GFX_Sprite_t& spr, PersonaState, uint32_t,
                    const BuddyOverlay&) {
  spr.fillScreen(0);   // black (palette index 0)
}
