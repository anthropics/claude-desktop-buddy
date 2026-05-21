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
void characterSetPeek(bool peek);
namespace lgfx { inline namespace v1 { class LGFXBase; } }
void characterRenderTo(lgfx::v1::LGFXBase* tgt, int cx, int cy);

const Palette& characterPalette();
