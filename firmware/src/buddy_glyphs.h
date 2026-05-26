#pragma once
#include <M5Unified.h>

using M5GFX_Sprite_t = LGFX_Sprite;

// Draw a single glyph char centered at (x,y) using the built-in font, scaled.
// colorIdx is a palette index. textsize ~ size/8 (built-in font is 6x8).
inline void drawGlyph(M5GFX_Sprite_t& spr, int16_t x, int16_t y,
                      const char* ch, int colorIdx, uint8_t textsize) {
  spr.setTextSize(textsize);
  spr.setTextColor(colorIdx);
  spr.setTextDatum(middle_center);
  spr.drawString(ch, x, y);
}
