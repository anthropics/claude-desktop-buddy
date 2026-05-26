#pragma once
#include <M5Unified.h>
#include "buddy_state.h"

using M5GFX_Sprite_t = LGFX_Sprite;

// Palette depth the active sprite was created with (selected at boot).
enum BuddyDepth { DEPTH_8 = 0, DEPTH_4 };

// Overlay data the buddy renderer draws in the side margins (full-screen layout).
struct BuddyOverlay {
  int  running, waiting, total;
  char statusMsg[64];
  bool autoOn;          // AUTO ON banner
  int  autoCount;
  bool autoToast;       // transient "Auto: tool (N)" toast active
  char autoToolMsg[40];
  bool debugActive;     // show small DBG tag, suppress counts
};

// Initialize palette indices for the chosen depth. Call once after createSprite.
void buddyInit(BuddyDepth depth);

void drawCoolS(M5GFX_Sprite_t& spr, int16_t cx, int16_t cy, float scale,
               float width, const float opacity[12], int forceIdx);

// Render one frame of `state` at time tMs into `spr` (does NOT push).
void buddyCoolSTick(M5GFX_Sprite_t& spr, PersonaState state, uint32_t tMs,
                    const BuddyOverlay& overlay);
