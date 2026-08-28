#pragma once
#include <stdint.h>

struct Palette {
  uint16_t body, bg, text, textDim, ink;
};

// Call after M5.begin() and spr.createSprite(). Mounts LittleFS, reads
// /characters/<name>/manifest.json, parses colors, caches GIF paths.
bool characterInit(const char* name);
bool characterLoaded();

// 0..6: sleep, idle, busy, attention, celebrate, dizzy, heart.
// Closes current GIF, opens the one for this state. No-op if same state.
void characterSetState(uint8_t state);

// Advances timing; if it's time for the next frame, decodes it into the
// sprite. Call every loop iteration. Does nothing if not loaded.
void characterTick();
void characterInvalidate();
void characterClose();   // close GIF + clear loaded flag; FS stays mounted   // full clear + reopen current — call when an overlay closes

// Peek mode renders the GIF at half scale, centered in the info-panel
// header strip; off renders full-size centered in the upper home area.
// Adaptive to actual canvas height — no padding required in source art.
// Peek levels. The pet shares the screen with something else and renders
// smaller, but how much smaller depends on who it is sharing with: the info
// and pet pages repaint from y=70, the clock face from y=98, so the clock can
// afford a noticeably larger pet than a single shared setting allowed.
enum : uint8_t { PEEK_OFF = 0, PEEK_HALF = 1, PEEK_3Q = 2 };
// Returns true when the level actually changed, which also means it has
// already invalidated -- callers must not invalidate a second time, since
// every invalidate reopens the GIF.
bool characterSetPeek(uint8_t level);
#include "compat.h"
void characterRenderTo(TFT_eSPI* tgt, int cx, int cy);

const Palette& characterPalette();
