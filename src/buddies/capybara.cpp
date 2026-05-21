#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace capybara {

// ─── SLEEP ───  ~12s cycle, 6 poses
static void doSleep(uint32_t t) {
  static const char* const FLAT[5]    = { "            ", "            ", "    .--.    ", "  _( -- )_  ", " (___zz___) " };
  static const char* const BREATHE[5] = { "            ", "    .--.    ", "  _( -- )_  ", " (___..___) ", "  ~~~~~~~~  " };
  static const char* const SNORE[5]   = { "            ", "    .--.    ", "  _( __ )_  ", " (___oO___) ", "  ~~~~~~~~  " };
  static const char* const SIDE[5]    = { "            ", "            ", "  .---___   ", " (--   --)= ", "  `~~~~~~`  " };
  static const char* const SIDE_Z[5]  = { "            ", "            ", "  .---___   ", " (-- ZZZ-)= ", "  `~~~~~~`  " };
  static const char* const YAWN[5]    = { "            ", "    .--.    ", "  _( ^^ )_  ", " (___O____) ", "  ~~~~~~~~  " };

  const char* const* P[6] = { FLAT, BREATHE, SNORE, SIDE, SIDE_Z, YAWN };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,2,1,
    0,1,0,1,
    3,4,3,4,3,4,
    3,3,
    1,5,1,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC2A6);

  // Z particles drift up-right
  int p1 = (t)     % 10;
  int p2 = (t + 4) % 10;
  int p3 = (t + 7) % 10;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 20 + p1, BUDDY_Y_OVERLAY + 18 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 26 + p2, BUDDY_Y_OVERLAY + 14 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 16 + p3 / 2, BUDDY_Y_OVERLAY + 10 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses
static void doIdle(uint32_t t) {
  static const char* const REST[5]    = { "            ", "  n______n  ", " ( o    o ) ", " (   oo   ) ", "  `------'  " };
  static const char* const LOOK_L[5]  = { "            ", "  n______n  ", " (o     o ) ", " (   oo   ) ", "  `------'  " };
  static const char* const LOOK_R[5]  = { "            ", "  n______n  ", " ( o     o) ", " (   oo   ) ", "  `------'  " };
  static const char* const LOOK_U[5]  = { "            ", "  n______n  ", " ( ^    ^ ) ", " (   oo   ) ", "  `------'  " };
  static const char* const BLINK[5]   = { "            ", "  n______n  ", " ( -    - ) ", " (   oo   ) ", "  `------'  " };
  static const char* const EAR_TW[5]  = { "            ", "  ^______n  ", " ( o    o ) ", " (   oo   ) ", "  `------'  " };
  static const char* const CHEW_A[5]  = { "            ", "  n______n  ", " ( o    o ) ", " (   ww   ) ", "  `------'  " };
  static const char* const CHEW_B[5]  = { "            ", "  n______n  ", " ( o    o ) ", " (   WW   ) ", "  `------'  " };
  static const char* const YAWN[5]    = { "            ", "  n______n  ", " ( -    - ) ", " (   OO   ) ", "  `------'  " };
  static const char* const STRETCH[5] = { "            ", " /n______n\\ ", "/( o    o )\\", " (   oo   ) ", "  `------'  " };

  const char* const* P[10] = { REST, LOOK_L, LOOK_R, LOOK_U, BLINK, EAR_TW, CHEW_A, CHEW_B, YAWN, STRETCH };
  static const uint8_t SEQ[] = {
    0,0,0,1,0,2,0,4,
    0,5,0,0,
    6,7,6,7,
    0,0,3,3,0,4,
    8,8,0,0,
    9,9,0,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC2A6);
}

// ─── BUSY ───  ~10s cycle, 6 poses + dot ticker
static void doBusy(uint32_t t) {
  static const char* const TYPE_A[5]  = { "            ", "  n______n  ", " ( v    v ) ", " (   --   ) ", " /`------'\\ " };
  static const char* const TYPE_B[5]  = { "            ", "  n______n  ", " ( v    v ) ", " (   __   ) ", " \\`------'/ " };
  static const char* const THINK[5]   = { "      ?     ", "  n______n  ", " ( ^    ^ ) ", " (   ..   ) ", "  `------'  " };
  static const char* const SIP[5]     = { "    [_]     ", "  n_____|n  ", " ( o    o|) ", " (   --   ) ", "  `------'  " };
  static const char* const EUREKA[5]  = { "      *     ", "  n______n  ", " ( O    O ) ", " (   ^^   ) ", " /`------'\\ " };
  static const char* const RELIEF[5]  = { "    ~~~     ", "  n______n  ", " ( -    - ) ", " (   __   ) ", "  `------'  " };

  const char* const* P[6] = { TYPE_A, TYPE_B, THINK, SIP, EUREKA, RELIEF };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC2A6);

  static const char* const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 14);
  buddyPrint(DOTS[t % 6]);
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "    ^  ^    ", " /^_____^\\  ", "( O      O )", " (   O    ) ", "  `------'  " };
  static const char* const SCAN_L[5]  = { "    ^  ^    ", " /^_____^\\  ", "(O       O )", " (   O    ) ", "  `------'  " };
  static const char* const SCAN_R[5]  = { "    ^  ^    ", " /^_____^\\  ", "( O       O)", " (   O    ) ", "  `------'  " };
  static const char* const SCAN_U[5]  = { "    ^  ^    ", " /^_____^\\  ", "( ^      ^ )", " (   O    ) ", "  `------'  " };
  static const char* const TENSE[5]   = { "    ^  ^    ", "/^^_____^^\\ ", "( O      O )", " (   O    ) ", " /`------'\\ " };
  static const char* const HUSH[5]    = { "    ^  ^    ", " /^_____^\\  ", "( o      o )", " (   .    ) ", "  `------'  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, TENSE, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0xC2A6, xOff);

  if ((t / 2) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER - 4, BUDDY_Y_OVERLAY);
    buddyPrint("!");
  }
  if ((t / 3) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER + 4, BUDDY_Y_OVERLAY + 4);
    buddyPrint("!");
  }
}

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + confetti rain
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "            ", "  n______n  ", " ( ^    ^ ) ", " (   ww   ) ", " /`------'\\ " };
  static const char* const JUMP[5]    = { "  \\(    )/  ", "   n____n   ", " ( ^    ^ ) ", " (   ww   ) ", "  `------'  " };
  static const char* const PEAK[5]    = { "  \\^    ^/  ", "   n____n   ", " ( ^    ^ ) ", " (   WW   ) ", "  `------'  " };
  static const char* const SPIN_L[5]  = { "            ", "  n______n  ", "( <    < ) /", " (   ww   ) ", "  `------'  " };
  static const char* const SPIN_R[5]  = { "            ", "  n______n  ", "\\( >    > )", " (   ww   ) ", "  `------'  " };
  static const char* const POSE[5]    = { "    \\__/    ", "  n______n  ", " ( ^    ^ ) ", "/(   WW   )\\", "  `------'  " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xC2A6);

  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_HEART, BUDDY_CYAN, BUDDY_WHITE, BUDDY_GREEN };
  for (int i = 0; i < 6; i++) {
    int phase = (t * 2 + i * 11) % 22;
    int x = BUDDY_X_CENTER - 36 + i * 14;
    int y = BUDDY_Y_OVERLAY - 6 + phase;
    if (y > BUDDY_Y_BASE + 20 || y < 0) continue;
    buddySetColor(cols[i % 5]);
    buddySetCursor(x, y);
    buddyPrint((i + (int)(t/2)) & 1 ? "*" : ".");
  }
}

// ─── DIZZY ───  ~5.6s cycle, 5 poses + orbiting stars
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]  = { "            ", " n______n   ", "( @    @ )  ", " (   ~~   ) ", "  `------'  " };
  static const char* const TILT_R[5]  = { "            ", "   n______n ", "  ( @    @ )", " (   ~~   ) ", "  `------'  " };
  static const char* const WOOZY[5]   = { "            ", "  n______n  ", " ( x    @ ) ", " (   ~v   ) ", "  `------'  " };
  static const char* const WOOZY2[5]  = { "            ", "  n______n  ", " ( @    x ) ", " (   v~   ) ", "  `------'  " };
  static const char* const STUMBLE[5] = { "            ", "  n______n  ", " ( @    @ ) ", " (   --   ) ", " /`-_---_'\\ " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, STUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC2A6, X_SHIFT[beat]);

  static const int8_t OX[] = { 0, 5, 7, 5, 0, -5, -7, -5 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 4) % 8;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 6 + OY[p1]);
  buddyPrint("*");
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 6 + OY[p2]);
  buddyPrint("*");
}

// ─── HEART ───  ~10s cycle, 5 poses + rising heart stream
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "            ", "  n______n  ", " ( ^    ^ ) ", " (   ww   ) ", "  `------'  " };
  static const char* const BLUSH[5]   = { "            ", "  n______n  ", " (#^    ^#) ", " (   ww   ) ", "  `------'  " };
  static const char* const EYES_C[5]  = { "            ", "  n______n  ", " ( <3  <3 ) ", " (   ww   ) ", "  `------'  " };
  static const char* const TWIRL[5]   = { "            ", "  n______n  ", " ( @    @ ) ", " (   ww   ) ", " /`------'\\ " };
  static const char* const SIGH[5]    = { "            ", "  n______n  ", " ( -    - ) ", " (   ^^   ) ", "  `------'  " };

  const char* const* P[5] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xC2A6);

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

}  // namespace capybara

extern const Species CAPYBARA_SPECIES = {
  "capybara",
  0xC2A6,
  { capybara::doSleep, capybara::doIdle, capybara::doBusy, capybara::doAttention,
    capybara::doCelebrate, capybara::doDizzy, capybara::doHeart }
};
