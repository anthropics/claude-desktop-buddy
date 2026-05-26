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

static void renderAttention(M5GFX_Sprite_t& spr, uint32_t t) {
  bool flash = ((t / 110) % 2) == 0;
  int  ci    = flash ? gWhite : -1;          // -1 -> green via opacity
  float opacity = flash ? 1.0f : 0.75f;
  float scale = 0.56f + 0.025f * sinf(t / 120.0f);
  int16_t sx = (int16_t)(sinf(t / 30.0f) * 1.6f);
  int16_t sy = (int16_t)(cosf(t / 35.0f) * 1.2f);
  float op[COOL_S_COUNT];
  for (int i = 0; i < COOL_S_COUNT; ++i) op[i] = opacity;
  drawCoolS(spr, CX + sx, CY + sy, scale, 4.0f, op, ci);
  // Pulsing ! in four corners (alternating phase).
  const int16_t cxs[4] = {22, 298, 22, 298};
  const int16_t cys[4] = {36, 36, 216, 216};
  const float   ph [4] = {0, 0.5f, 0.5f, 0};
  for (int i = 0; i < 4; ++i) {
    bool on = ((int)(t / 150 + ph[i] * 2) % 2) == 0;
    drawGlyph(spr, cxs[i], cys[i], "!", on ? gWhite : greenIdx(0.3f), 3);
  }
}

static void renderCelebrate(M5GFX_Sprite_t& spr, uint32_t t) {
  float bp  = (float)(t % 800) / 800.0f;
  float hop = sinf(bp * (float)M_PI);            // 0..1..0
  int16_t yOff = (int16_t)(-hop * 18);
  float scale = 0.55f + 0.04f * hop;
  // (Rotational sway from states.jsx is omitted: drawWideLine has no rotate;
  //  bounce + confetti carry the celebrate read. Documented simplification.)
  float op[COOL_S_COUNT];
  for (int i = 0; i < COOL_S_COUNT; ++i) op[i] = 1.0f;
  drawCoolS(spr, CX, CY + yOff, scale, 4.0f, op, -1);
  // Radial confetti: 16 glyphs flying outward, looping.
  const char* glyphs[4] = {"*", "+", "x", "."};
  for (int i = 0; i < 16; ++i) {
    float angle = (float)i / 16.0f * 2.0f * (float)M_PI;
    float phase = (float)((t + i * 100) % 1600) / 1600.0f;
    float radius = 30 + phase * 130;
    int16_t px = (int16_t)(CX + cosf(angle) * radius);
    int16_t py = (int16_t)(CY + sinf(angle) * radius * 0.7f);
    float o = sinf(phase * (float)M_PI);
    drawGlyph(spr, px, py, glyphs[i % 4], greenIdx(o * 0.85f), 2);
  }
  if (hop > 0.55f)
    drawGlyph(spr, CX, 36, "LVL UP", greenIdx((hop - 0.55f) / 0.45f), 3);
}

static void renderDizzy(M5GFX_Sprite_t& spr, uint32_t t) {
  float wobble = sinf(t / 200.0f) * 8.0f;
  float glitch = (((t / 800) % 4) == 0) ? sinf(t / 20.0f) * 3.0f : 0.0f;
  float sX = 0.55f + sinf(t / 300.0f) * 0.05f;
  // Phantom S at 32% (drawn first, dimmer).
  float ph[COOL_S_COUNT]; for (int i=0;i<COOL_S_COUNT;++i) ph[i]=0.32f;
  drawCoolS(spr, (int16_t)(CX - wobble * 0.7f), CY, sX, 3.0f, ph, -1);
  // Main S.
  float op[COOL_S_COUNT]; for (int i=0;i<COOL_S_COUNT;++i) op[i]=1.0f;
  drawCoolS(spr, (int16_t)(CX + wobble + glitch), CY, sX, 3.0f, op, -1);
  // Four orbiting ? glyphs.
  for (int i = 0; i < 4; ++i) {
    float a = t / 600.0f + i * (float)M_PI / 2.0f;
    int16_t px = (int16_t)(CX + cosf(a) * 80);
    int16_t py = (int16_t)(CY + sinf(a) * 50);
    drawGlyph(spr, px, py, "?", greenIdx(0.85f), 2);
  }
}

static void renderHeart(M5GFX_Sprite_t& spr, uint32_t t) {
  float p = (float)(t % 1200) / 1200.0f;
  // Two narrow gaussians at p=0.10 and p=0.30.
  float k = expf(-powf((p - 0.10f) * 14.0f, 2)) +
            expf(-powf((p - 0.30f) * 14.0f, 2)) * 0.7f;
  float scale = 0.55f + k * 0.06f;
  bool  useRed = k > 0.15f;
  float op[COOL_S_COUNT]; for (int i=0;i<COOL_S_COUNT;++i) op[i]=1.0f;
  drawCoolS(spr, CX, CY, scale, 3.0f + k * 4.0f, op, useRed ? gRed : -1);
  // 5 'v' hearts floating up (red).
  for (int i = 0; i < 5; ++i) {
    float phf = (float)((t + i * 480) % 2400) / 2400.0f;
    int16_t px = (int16_t)(80 + i * 40 + sinf(phf * 4 + i) * 8);
    int16_t py = (int16_t)(220 - phf * 200);
    // Hearts fade with sin(phf*pi); red has no ramp, so skip when too dim.
    if (sinf(phf * (float)M_PI) > 0.25f)
      drawGlyph(spr, px, py, "v", gRed, 2);
  }
}

static void drawOverlay(M5GFX_Sprite_t& spr, const BuddyOverlay& o) {
  if (o.debugActive) {
    drawGlyph(spr, 300, 12, "DBG", greenIdx(0.7f), 1);
    return;                       // counts suppressed in debug
  }
  spr.setTextDatum(top_left);
  spr.setTextColor(greenIdx(1.0f));
  // Left margin: running (top), waiting (below). Right margin: total.
  spr.setTextSize(2);
  spr.setCursor(6, 40);  spr.printf("%dR", o.running);
  spr.setCursor(6, 90);  spr.printf("%dW", o.waiting);
  spr.setCursor(258, 40); spr.printf("%d", o.total);
  // Status msg: bottom strip, truncated to fit 320px (~52 chars at size 1).
  if (o.statusMsg[0]) {
    spr.setTextSize(1);
    char buf[54]; strncpy(buf, o.statusMsg, 53); buf[53] = 0;
    spr.setTextColor(greenIdx(0.5f));
    spr.setCursor(6, 230); spr.print(buf);
  }
  // AUTO banner (top), drawn over everything when on.
  if (o.autoOn) {
    spr.fillRect(0, 0, 320, 16, gRed);
    spr.setTextColor(gWhite); spr.setTextSize(1);
    spr.setCursor(6, 4); spr.printf("AUTO ON  %d", o.autoCount);
  }
  // Transient auto-fired toast (bottom).
  if (o.autoToast) {
    spr.setTextColor(greenIdx(1.0f)); spr.setTextSize(1);
    spr.setCursor(120, 230); spr.printf("Auto: %s (%d)", o.autoToolMsg, o.autoCount);
  }
}

void buddyCoolSTick(M5GFX_Sprite_t& spr, PersonaState state, uint32_t tMs,
                    const BuddyOverlay& overlay) {
  spr.fillScreen(0);
  switch (state) {
    case PS_SLEEP:     renderSleep(spr, tMs);     break;
    case PS_IDLE:      renderIdle(spr, tMs);      break;
    case PS_BUSY:      renderBusy(spr, tMs);      break;
    case PS_ATTENTION: renderAttention(spr, tMs); break;
    case PS_CELEBRATE: renderCelebrate(spr, tMs); break;
    case PS_DIZZY:     renderDizzy(spr, tMs);     break;
    case PS_HEART:     renderHeart(spr, tMs);     break;
    default:           renderIdle(spr, tMs);      break;
  }
  drawOverlay(spr, overlay);
}
