#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace octopus {

// ─── SLEEP ───  ~12s cycle, 6 poses
static void doSleep(uint32_t t) {
  static const char* const CURL[5]    = { "            ", "            ", "   .----.   ", "  ( -- -- ) ", "  (~~zz~~)  " };
  static const char* const BREATHE[5] = { "            ", "   .----.   ", "  ( -- -- ) ", "  (______)  ", "  ~~~~~~~~  " };
  static const char* const DEEP[5]    = { "            ", "   .----.   ", "  ( __ __ ) ", "  (______)  ", "  ~~~~~~~~  " };
  static const char* const DRIFT_L[5] = { "            ", "  .----.    ", " ( -- -- )  ", " (______)   ", " ~/~/~/~/   " };
  static const char* const DRIFT_R[5] = { "            ", "    .----.  ", "   ( -- -- )", "   (______) ", "   /~/~/~/~ " };
  static const char* const SNORE[5]   = { "            ", "   .----.   ", "  ( oO oO ) ", "  (__o___)  ", "  ~~~~~~~~  " };

  const char* const* P[6] = { CURL, BREATHE, DEEP, DRIFT_L, DRIFT_R, SNORE };
  static const uint8_t SEQ[] = {
    0,0,1,1,0,0,2,2,
    1,1,0,0,
    3,3,4,4,3,4,
    1,1,2,2,
    0,5,1,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xA01F);

  // Z particles drift up
  int p1 = (t)     % 10;
  int p2 = (t + 4) % 10;
  int p3 = (t + 7) % 10;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 18 + p1, BUDDY_Y_OVERLAY + 16 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 24 + p2, BUDDY_Y_OVERLAY + 12 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 14 + p3 / 2, BUDDY_Y_OVERLAY + 8 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses
static void doIdle(uint32_t t) {
  static const char* const REST[5]    = { "            ", "   .----.   ", "  ( o  o )  ", "  (______)  ", "  /\\/\\/\\/\\  " };
  static const char* const WAVE_A[5]  = { "            ", "   .----.   ", "  ( o  o )  ", "  (______)  ", "  \\/\\/\\/\\/  " };
  static const char* const LOOK_L[5]  = { "            ", "   .----.   ", "  (o   o )  ", "  (______)  ", "  /\\/\\/\\/\\  " };
  static const char* const LOOK_R[5]  = { "            ", "   .----.   ", "  ( o   o)  ", "  (______)  ", "  \\/\\/\\/\\/  " };
  static const char* const BLINK[5]   = { "            ", "   .----.   ", "  ( -  - )  ", "  (______)  ", "  /\\/\\/\\/\\  " };
  static const char* const CURL_T[5]  = { "            ", "   .----.   ", "  ( o  o )  ", "  (______)  ", "  /)/\\/\\(\\  " };
  static const char* const WIGGLE[5]  = { "            ", "   .----.   ", "  ( ^  ^ )  ", "  (______)  ", "  )(/\\/\\)(  " };
  static const char* const FLOAT_U[5] = { "   .----.   ", "  ( o  o )  ", "  (______)  ", "  /\\/\\/\\/\\  ", "            " };
  static const char* const GRIN[5]    = { "            ", "   .----.   ", "  ( ^  ^ )  ", "  (\\__/\\)  ", "  /\\/\\/\\/\\  " };
  static const char* const STRETCH[5] = { "            ", "  /.----.\\  ", " /( o  o )\\ ", " \\(______)/ ", " //\\/\\/\\/\\\\" };

  const char* const* P[10] = { REST, WAVE_A, LOOK_L, LOOK_R, BLINK, CURL_T, WIGGLE, FLOAT_U, GRIN, STRETCH };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,2,1,3,
    0,1,4,0,
    5,6,5,6,
    0,1,0,4,1,0,
    7,7,0,1,
    8,8,0,1,
    9,9,0,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xA01F);
}

// ─── BUSY ───  ~10s cycle, 6 poses + bubble ticker
static void doBusy(uint32_t t) {
  static const char* const TYPE_A[5]  = { "            ", "   .----.   ", "  ( v  v )  ", "  (__--__)  ", "  /)\\/\\/(\\  " };
  static const char* const TYPE_B[5]  = { "            ", "   .----.   ", "  ( v  v )  ", "  (__==__)  ", "  (\\/\\/\\/)  " };
  static const char* const THINK[5]   = { "      ?     ", "   .----.   ", "  ( ^  ^ )  ", "  (__..__)  ", "  /\\/\\/\\/\\  " };
  static const char* const SCRIBE[5]  = { "    [_]     ", "   .---|.   ", "  ( o  o|)  ", "  (__--__)  ", "  /\\(\\/\\/\\  " };
  static const char* const EUREKA[5]  = { "      *     ", "   .----.   ", "  ( O  O )  ", "  (__^^__)  ", " //\\/\\/\\\\\\ " };
  static const char* const RELIEF[5]  = { "    ~~~     ", "   .----.   ", "  ( -  - )  ", "  (__--__)  ", "  /\\/\\/\\/\\  " };

  const char* const* P[6] = { TYPE_A, TYPE_B, THINK, SCRIBE, EUREKA, RELIEF };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xA01F);

  static const char* const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 14);
  buddyPrint(DOTS[t % 6]);

  // Tiny bubbles drift up
  int b = (t * 2) % 18;
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER - 30, BUDDY_Y_OVERLAY + 18 - b);
  buddyPrint("o");
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "    ^  ^    ", "   .----.   ", "  ( O  O )  ", "  (__O___)  ", "  /\\/\\/\\/\\  " };
  static const char* const SCAN_L[5]  = { "    ^  ^    ", "   .----.   ", "  (O   O )  ", "  (__O___)  ", "  /\\/\\/\\/\\  " };
  static const char* const SCAN_R[5]  = { "    ^  ^    ", "   .----.   ", "  ( O   O)  ", "  (__O___)  ", "  \\/\\/\\/\\/  " };
  static const char* const SCAN_U[5]  = { "    ^  ^    ", "   .----.   ", "  ( ^  ^ )  ", "  (__O___)  ", "  /\\/\\/\\/\\  " };
  static const char* const TENSE[5]   = { "    ^  ^    ", "  /.----.\\  ", " ( O    O ) ", "  (__O___)  ", " //\\/\\/\\\\\\ " };
  static const char* const HUSH[5]    = { "    ^  ^    ", "   .----.   ", "  ( o  o )  ", "  (__.___)  ", "  /\\/\\/\\/\\  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, TENSE, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0xA01F, xOff);

  if ((t / 2) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER - 6, BUDDY_Y_OVERLAY);
    buddyPrint("!");
  }
  if ((t / 3) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER + 6, BUDDY_Y_OVERLAY + 4);
    buddyPrint("!");
  }
}

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + confetti rain
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "            ", "   .----.   ", "  ( ^  ^ )  ", "  (__ww__)  ", " //\\/\\/\\\\\\ " };
  static const char* const JUMP[5]    = { "  \\/    \\/  ", "   .----.   ", "  ( ^  ^ )  ", "  (__ww__)  ", "  )(\\/\\/)( " };
  static const char* const PEAK[5]    = { "  \\^    ^/  ", "   .----.   ", "  ( ^  ^ )  ", "  (__WW__)  ", "  ((    ))  " };
  static const char* const SPIN_L[5]  = { "            ", "   .----.   ", " (( <  < )) ", "  (__ww__)  ", " /)/\\/\\(\\  " };
  static const char* const SPIN_R[5]  = { "            ", "   .----.   ", " (( >  > )) ", "  (__ww__)  ", " (\\/\\/\\/)/ " };
  static const char* const POSE[5]    = { "    \\__/    ", "   .----.   ", " /( ^  ^ )\\ ", " \\(__WW__)/ ", " //\\/\\/\\\\\\ " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xA01F);

  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_HEART, BUDDY_CYAN, BUDDY_WHITE, BUDDY_GREEN };
  for (int i = 0; i < 6; i++) {
    int phase = (t * 2 + i * 11) % 22;
    int x = BUDDY_X_CENTER - 36 + i * 14;
    int y = BUDDY_Y_OVERLAY - 6 + phase;
    if (y > BUDDY_Y_BASE + 20 || y < 0) continue;
    buddySetColor(cols[i % 5]);
    buddySetCursor(x, y);
    buddyPrint((i + (int)(t/2)) & 1 ? "*" : "o");
  }
}

// ─── DIZZY ───  ~5.6s cycle, 5 poses + orbiting stars + ink cloud
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]  = { "            ", "  .----.    ", " ( @  @ )   ", " (__~~__)   ", " /\\/\\/\\/\\   " };
  static const char* const TILT_R[5]  = { "            ", "    .----.  ", "   ( @  @ ) ", "   (__~~__) ", "   \\/\\/\\/\\/ " };
  static const char* const WOOZY[5]   = { "            ", "   .----.   ", "  ( x  @ )  ", "  (__~v__)  ", "  /\\)/\\(\\  " };
  static const char* const WOOZY2[5]  = { "            ", "   .----.   ", "  ( @  x )  ", "  (__v~__)  ", "  (\\/\\(/\\/  " };
  static const char* const STUMBLE[5] = { "            ", "   .----.   ", "  ( @  @ )  ", "  (__--__)  ", " /)\\_/\\_(\\ " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, STUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xA01F, X_SHIFT[beat]);

  // Orbiting stars
  static const int8_t OX[] = { 0, 5, 7, 5, 0, -5, -7, -5 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 4) % 8;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 4 + OY[p1]);
  buddyPrint("*");
  buddySetColor(BUDDY_PURPLE);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 4 + OY[p2]);
  buddyPrint("*");

  // Ink cloud puff drifts down occasionally
  if ((t / 8) & 1) {
    int puff = (t % 8);
    buddySetColor(BUDDY_DIM);
    buddySetCursor(BUDDY_X_CENTER - 24 - puff, BUDDY_Y_OVERLAY + 10 + puff);
    buddyPrint("o");
    buddySetCursor(BUDDY_X_CENTER + 24 + puff, BUDDY_Y_OVERLAY + 10 + puff);
    buddyPrint("o");
  }
}

// ─── HEART ───  ~10s cycle, 6 poses + rising heart stream
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "            ", "   .----.   ", "  ( ^  ^ )  ", "  (__ww__)  ", "  /\\/\\/\\/\\  " };
  static const char* const BLUSH[5]   = { "            ", "   .----.   ", "  (#^  ^#)  ", "  (__ww__)  ", "  /\\/\\/\\/\\  " };
  static const char* const EYES_C[5]  = { "            ", "   .----.   ", "  (<3  <3)  ", "  (__ww__)  ", "  \\/\\/\\/\\/  " };
  static const char* const TWIRL[5]   = { "            ", "   .----.   ", "  ( @  @ )  ", "  (__ww__)  ", " //\\/\\/\\\\\\ " };
  static const char* const SIGH[5]    = { "    ~~~     ", "   .----.   ", "  ( -  - )  ", "  (__^^__)  ", "  /\\/\\/\\/\\  " };
  static const char* const HUG[5]     = { "            ", "  /.----.\\  ", " /(#^  ^#)\\ ", " \\(__ww__)/ ", "  )(\\/\\/)(  " };

  const char* const* P[6] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH, HUG };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 5,5,1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, 0,-1,0,-1 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xA01F);

  buddySetColor(BUDDY_HEART);
  for (int i = 0; i < 5; i++) {
    int phase = (t + i * 4) % 16;
    int y = BUDDY_Y_OVERLAY + 16 - phase;
    if (y < -2 || y > BUDDY_Y_BASE) continue;
    int x = BUDDY_X_CENTER - 20 + i * 8 + ((phase / 3) & 1) * 2 - 1;
    buddySetCursor(x, y);
    buddyPrint("v");
  }
}

}  // namespace octopus

extern const Species OCTOPUS_SPECIES = {
  "octopus",
  0xA01F,
  { octopus::doSleep, octopus::doIdle, octopus::doBusy, octopus::doAttention,
    octopus::doCelebrate, octopus::doDizzy, octopus::doHeart }
};
