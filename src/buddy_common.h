#pragma once
#include <stdint.h>

// One include site for the M5 SDK. Switches by build target so the rest
// of the code can use BuddySprite/BuddyCanvas without #ifdefs.
#ifdef BUDDY_TARGET_STICKS3
  #include <M5Unified.h>
  using BuddySprite = M5Canvas;     // sprite-like offscreen buffer
  using BuddyCanvas = LovyanGFX;    // common rendering base for sprite + display
#else
  #include <M5StickCPlus.h>
  using BuddySprite = TFT_eSprite;
  using BuddyCanvas = TFT_eSPI;
#endif

// Shared constants and helpers for buddy species files.
// Each species file (src/buddies/<name>.cpp) includes this header
// and defines its 7 state functions.

// Geometry — shared layout for all species
extern const int BUDDY_X_CENTER;
extern const int BUDDY_CANVAS_W;
extern const int BUDDY_Y_BASE;
extern const int BUDDY_Y_OVERLAY;
extern const int BUDDY_CHAR_W;
extern const int BUDDY_CHAR_H;

// Common colors species can use freely
extern const uint16_t BUDDY_BG;
extern const uint16_t BUDDY_HEART;
extern const uint16_t BUDDY_DIM;
extern const uint16_t BUDDY_YEL;
extern const uint16_t BUDDY_WHITE;
extern const uint16_t BUDDY_CYAN;
extern const uint16_t BUDDY_GREEN;
extern const uint16_t BUDDY_PURPLE;
extern const uint16_t BUDDY_RED;
extern const uint16_t BUDDY_BLUE;

// Print one line centered around BUDDY_X_CENTER, optionally x-offset.
void buddyPrintLine(const char* line, int yPx, uint16_t color, int xOff = 0);

// Print N-line sprite block. yOffset is added to BUDDY_Y_BASE for the top row.
void buddyPrintSprite(const char* const* lines, uint8_t nLines, int yOffset, uint16_t color, int xOff = 0);

// Set sprite text color directly + cursor (for ad-hoc particle drawing).
void buddySetCursor(int x, int y);
void buddySetColor(uint16_t fg);
void buddyPrint(const char* s);
