#include "buddy_coolS.h"
#include "buddy_geometry.h"
#include "buddy_anim_math.h"
#include "buddy_palette.h"
#include "buddy_idle_reveal.h"
#include "buddy_glyphs.h"
#include <cmath>
#include <cstring>

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

static const int16_t CX = 160, CY = 120;   // full-screen center

static void renderSleep(M5GFX_Sprite_t& spr, uint32_t t) {
  float breath  = sineCycle(t, 4000.0f);            // 0..1
  float scale   = 0.50f + 0.03f * breath;
  float opacity = 0.25f + 0.35f * breath;
  float op[COOL_S_COUNT];
  for (int i = 0; i < COOL_S_COUNT; ++i) op[i] = opacity;
  drawCoolS(spr, CX, CY, scale, 8.0f * scale / 0.55f, op, -1);
  // Drifting z + faint trailing z (every 4s).
  float zp = (float)(t % 4000) / 4000.0f;
  int16_t zx = (int16_t)(215 + zp * 25), zy = (int16_t)(90 - zp * 40);
  float zo = sinf(zp * (float)M_PI);
  drawGlyph(spr, zx, zy, "z", greenIdx(zo * 0.7f), 3);
  drawGlyph(spr, zx - 14, zy + 12, "z", greenIdx(zo * 0.4f), 2);
}

static void renderIdle(M5GFX_Sprite_t& spr, uint32_t t) {
  float p = (float)(t % 7000) / 7000.0f;
  float op[COOL_S_COUNT];
  idleRevealOpacities(p, op);
  drawCoolS(spr, CX, CY, 0.55f, 3.0f, op, -1);
}

static void renderBusy(M5GFX_Sprite_t& spr, uint32_t t) {
  float p = (float)(t % 1400) / 1400.0f;
  int16_t scanY = (int16_t)(p * 240);
  // Full figure, then erase below the scanline (top-to-bottom reveal).
  float op[COOL_S_COUNT];
  for (int i = 0; i < COOL_S_COUNT; ++i) op[i] = 0.95f;
  drawCoolS(spr, CX, CY, 0.55f, 3.0f, op, -1);
  spr.fillRect(0, scanY, 320, 240 - scanY, 0);          // mask below scanline
  spr.fillRect(0, scanY - 1, 320, 2, gGreenMax_idx());  // bright edge bar
  // Progress dots: 5, cycling every 200ms.
  int lit = (int)((t / 200) % 5);
  for (int i = 0; i < 5; ++i) {
    int ci = (lit >= i) ? greenIdx(0.9f) : greenIdx(0.15f);
    spr.fillRect(140 + i * 8, 222, 5, 5, ci);
  }
}

void buddyCoolSTick(M5GFX_Sprite_t& spr, PersonaState state, uint32_t tMs,
                    const BuddyOverlay& overlay) {
  spr.fillScreen(0);
  switch (state) {
    case PS_SLEEP: renderSleep(spr, tMs); break;
    case PS_IDLE:  renderIdle(spr, tMs);  break;
    case PS_BUSY:  renderBusy(spr, tMs);  break;
    default:       renderIdle(spr, tMs);  break;  // others added in Task 9
  }
}
