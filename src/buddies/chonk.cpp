#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace chonk {

// ─── SLEEP ───  ~12s cycle, 6 poses (heavy belly breathing)
static void doSleep(uint32_t t) {
  static const char* const CURL[5]    = { "            ", "            ", "  /\\____/\\  ", " ( -    - ) ", " (___zz___) " };
  static const char* const INHALE[5]  = { "            ", "  /\\____/\\  ", " ( -    - ) ", " (   --   ) ", " (________) " };
  static const char* const EXHALE[5]  = { "            ", "  /\\____/\\  ", " ( -    - ) ", "(    --    )", "(__________)" };
  static const char* const SNORE[5]   = { "            ", "  /\\____/\\  ", " ( -    - ) ", "(    OO    )", "(__________)" };
  static const char* const JIGGLE[5]  = { "            ", "  /\\____/\\  ", " ( -    - ) ", "(   ~~~~   )", " ~~~~~~~~~~ " };
  static const char* const DROOL[5]   = { "            ", "  /\\____/\\  ", " ( -    u ) ", " (   __   ) ", " (___..___) " };

  const char* const* P[6] = { CURL, INHALE, EXHALE, SNORE, INHALE, JIGGLE };
  static const uint8_t SEQ[] = {
    0,0,0,1,2,1,2,
    1,2,1,2,
    3,3,1,2,
    1,2,1,2,
    5,4,5,4,
    0,0,
    1,2,3,2
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  // Belly jiggle on EXHALE/JIGGLE poses
  int yOff = (pose == 5) ? ((t & 1) ? 1 : 0) : 0;
  buddyPrintSprite(P[pose], 5, yOff, 0xFD20);

  // Z particles drift up-right, lazy and heavy
  int p1 = (t)     % 12;
  int p2 = (t + 5) % 12;
  int p3 = (t + 9) % 12;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 18 + p1, BUDDY_Y_OVERLAY + 20 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 24 + p2, BUDDY_Y_OVERLAY + 16 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 30 + p3 / 2, BUDDY_Y_OVERLAY + 12 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~16s cycle, 10 poses (round mystery, ears swivel)
static void doIdle(uint32_t t) {
  static const char* const REST[5]    = { "            ", "  /\\____/\\  ", " ( o    o ) ", " (   ..   ) ", "  `------'  " };
  static const char* const LOOK_L[5]  = { "            ", "  /\\____/\\  ", " (o     o ) ", " (   ..   ) ", "  `------'  " };
  static const char* const LOOK_R[5]  = { "            ", "  /\\____/\\  ", " ( o     o) ", " (   ..   ) ", "  `------'  " };
  static const char* const LOOK_U[5]  = { "            ", "  /\\____/\\  ", " ( '    ' ) ", " (   ..   ) ", "  `------'  " };
  static const char* const BLINK[5]   = { "            ", "  /\\____/\\  ", " ( -    - ) ", " (   ..   ) ", "  `------'  " };
  static const char* const EAR_L[5]   = { "            ", "  /|____/\\  ", " ( o    o ) ", " (   ..   ) ", "  `------'  " };
  static const char* const EAR_R[5]   = { "            ", "  /\\____|\\  ", " ( o    o ) ", " (   ..   ) ", "  `------'  " };
  static const char* const JIG_A[5]   = { "            ", "  /\\____/\\  ", " ( o    o ) ", "(    ..    )", " (________) " };
  static const char* const JIG_B[5]   = { "            ", "  /\\____/\\  ", " ( o    o ) ", " (   ..   ) ", "(__________)" };
  static const char* const SNIFF[5]   = { "            ", "  /\\____/\\  ", " ( o    o ) ", " (   oo   ) ", "  `------'  " };

  const char* const* P[10] = { REST, LOOK_L, LOOK_R, LOOK_U, BLINK, EAR_L, EAR_R, JIG_A, JIG_B, SNIFF };
  static const uint8_t SEQ[] = {
    0,0,0,4,0,1,1,0,
    2,2,0,4,
    5,0,6,0,
    0,3,3,0,
    7,8,7,8,
    0,0,4,0,
    9,9,0,0,
    0,5,6,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFD20);
}

// ─── BUSY ───  ~10s cycle, 6 poses + cog ticker
static void doBusy(uint32_t t) {
  static const char* const PONDER[5]  = { "      ?     ", "  /\\____/\\  ", " ( o    o ) ", " (   --   ) ", "  `------'  " };
  static const char* const CALC_A[5]  = { "            ", "  /\\____/\\  ", " ( v    v ) ", " (   ::   ) ", " /`------'\\ " };
  static const char* const CALC_B[5]  = { "            ", "  /\\____/\\  ", " ( v    v ) ", " (   ;;   ) ", " \\`------'/ " };
  static const char* const SCRIBE[5]  = { "    ___     ", "  /\\___/\\/  ", " ( o    o-- ", " (   --   ) ", "  `------'  " };
  static const char* const AHA[5]     = { "      *     ", "  /\\____/\\  ", " ( O    O ) ", " (   ^^   ) ", " /`------'\\ " };
  static const char* const SIGH[5]    = { "    ~~~     ", "  /\\____/\\  ", " ( -    - ) ", " (   __   ) ", "  `------'  " };

  const char* const* P[6] = { PONDER, CALC_A, CALC_B, SCRIBE, AHA, SIGH };
  static const uint8_t SEQ[] = {
    1,2,1,2,1,2, 0,0, 1,2,1,2, 3,3,3, 4,4, 1,2,1,2, 5,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFD20);

  // Cog/dot ticker spinning beside head
  static const char* const COGS[] = { "+  ", "x  ", "*  ", "x  " };
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 24, BUDDY_Y_OVERLAY + 12);
  buddyPrint(COGS[t % 4]);
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 30, BUDDY_Y_OVERLAY + 18);
  buddyPrint(COGS[(t + 2) % 4]);
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse (heavy alert wobble)
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "    ^  ^    ", "  /^____^\\  ", " ( O    O ) ", " (   o    ) ", "  `------'  " };
  static const char* const SCAN_L[5]  = { "    ^  ^    ", "  /^____^\\  ", " (O     O ) ", " (   o    ) ", "  `------'  " };
  static const char* const SCAN_R[5]  = { "    ^  ^    ", "  /^____^\\  ", " ( O     O) ", " (   o    ) ", "  `------'  " };
  static const char* const PERK[5]    = { "    /\\/\\    ", "  /^____^\\  ", " ( ^    ^ ) ", " (   o    ) ", "  `------'  " };
  static const char* const TENSE[5]   = { "    ^  ^    ", " /^^____^^\\ ", " ( O    O ) ", " (   o    ) ", " /`------'\\ " };
  static const char* const GULP[5]    = { "    ^  ^    ", "  /^____^\\  ", " ( o    o ) ", " (   O    ) ", "  `------'  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, PERK, TENSE, GULP };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 2 : -2) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0xFD20, xOff);

  if ((t / 2) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER - 6, BUDDY_Y_OVERLAY);
    buddyPrint("!");
  }
  if ((t / 3) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER + 6, BUDDY_Y_OVERLAY + 4);
    buddyPrint("!");
  }
  if ((t / 4) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER - 14, BUDDY_Y_OVERLAY + 6);
    buddyPrint("!");
  }
}

// ─── CELEBRATE ───  ~6s cycle, 6 poses + confetti rain (heavy belly bounce)
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "            ", "  /\\____/\\  ", " ( ^    ^ ) ", "(    WW    )", "(__________)" };
  static const char* const JUMP[5]    = { "  \\(    )/  ", "   /____\\   ", " ( ^    ^ ) ", " (   WW   ) ", "  `------'  " };
  static const char* const PEAK[5]    = { "  \\^    ^/  ", "   /____\\   ", " ( ^    ^ ) ", " (   OO   ) ", "  `------'  " };
  static const char* const SPIN_L[5]  = { "            ", "  /\\____/\\  ", "( <    < ) /", " (   ww   ) ", "  `------'  " };
  static const char* const SPIN_R[5]  = { "            ", "  /\\____/\\  ", "\\( >    > )", " (   ww   ) ", "  `------'  " };
  static const char* const POSE[5]    = { "    \\__/    ", "  /\\____/\\  ", " ( ^    ^ ) ", "/(   WW   )\\", "  `------'  " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-7,-3,0, 0,0,0,0, 0,-3,-7,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xFD20);

  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_HEART, BUDDY_CYAN, BUDDY_WHITE, BUDDY_GREEN, BUDDY_PURPLE };
  for (int i = 0; i < 7; i++) {
    int phase = (t * 2 + i * 9) % 24;
    int x = BUDDY_X_CENTER - 42 + i * 14;
    int y = BUDDY_Y_OVERLAY - 8 + phase;
    if (y > BUDDY_Y_BASE + 22 || y < 0) continue;
    buddySetColor(cols[i % 6]);
    buddySetCursor(x, y);
    buddyPrint((i + (int)(t/2)) & 1 ? "*" : "o");
  }
}

// ─── DIZZY ───  ~6s cycle, 5 poses + orbiting stars (big body lurches)
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]  = { "            ", " /\\____/\\   ", "( @    @ )  ", " (   ~~   ) ", "  `------'  " };
  static const char* const TILT_R[5]  = { "            ", "   /\\____/\\ ", "  ( @    @ )", " (   ~~   ) ", "  `------'  " };
  static const char* const WOOZY[5]   = { "            ", "  /\\____/\\  ", " ( x    @ ) ", " (   ~v   ) ", "  `------'  " };
  static const char* const WOOZY2[5]  = { "            ", "  /\\____/\\  ", " ( @    x ) ", " (   v~   ) ", "  `------'  " };
  static const char* const TUMBLE[5]  = { "            ", "  /\\____/\\  ", " ( @    @ ) ", "(    --    )", " /`-_---_'\\ " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, TUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -4,4,-4,4, 0,0, -4,4,-4,4, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFD20, X_SHIFT[beat]);

  // Stars orbiting in a wider ellipse around the chonk's head
  static const int8_t OX[] = { 0, 6, 9, 6, 0, -6, -9, -6 };
  static const int8_t OY[] = { -6, -4, 0, 4, 6, 4, 0, -4 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 3) % 8;
  uint8_t p3 = (t + 5) % 8;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 6 + OY[p1]);
  buddyPrint("*");
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 6 + OY[p2]);
  buddyPrint("*");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + OX[p3] - 2, BUDDY_Y_OVERLAY + 6 + OY[p3]);
  buddyPrint("+");
}

// ─── HEART ───  ~10s cycle, 6 poses + rising heart stream (blushy chonk)
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "            ", "  /\\____/\\  ", " ( ^    ^ ) ", " (   ww   ) ", "  `------'  " };
  static const char* const BLUSH[5]   = { "            ", "  /\\____/\\  ", " (#^    ^#) ", " (   ww   ) ", "  `------'  " };
  static const char* const EYES_C[5]  = { "            ", "  /\\____/\\  ", " ( <3  <3 ) ", " (   ww   ) ", "  `------'  " };
  static const char* const TWIRL[5]   = { "            ", "  /\\____/\\  ", " ( @    @ ) ", "(    ww    )", " /`------'\\ " };
  static const char* const SIGH[5]    = { "            ", "  /\\____/\\  ", " ( -    - ) ", " (   ^^   ) ", "  `------'  " };
  static const char* const HUG[5]     = { "  v      v  ", "  /\\____/\\  ", " ( ^    ^ ) ", "/(   ww   )\\", "  `------'  " };

  const char* const* P[6] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH, HUG };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 5,5,0, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, 0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xFD20);

  buddySetColor(BUDDY_HEART);
  for (int i = 0; i < 6; i++) {
    int phase = (t + i * 3) % 18;
    int y = BUDDY_Y_OVERLAY + 16 - phase;
    if (y < -2 || y > BUDDY_Y_BASE) continue;
    int x = BUDDY_X_CENTER - 22 + i * 8 + ((phase / 3) & 1) * 2 - 1;
    buddySetCursor(x, y);
    buddyPrint("v");
  }
}

}  // namespace chonk

extern const Species CHONK_SPECIES = { "chonk", 0xFD20, { chonk::doSleep, chonk::doIdle, chonk::doBusy, chonk::doAttention, chonk::doCelebrate, chonk::doDizzy, chonk::doHeart } };
