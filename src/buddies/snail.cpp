#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace snail {

// ─── SLEEP ───  ~12s cycle, 6 poses — tucked in shell, snoring slime bubbles
static void doSleep(uint32_t t) {
  static const char* const TUCK[5]    = { "            ", "     .--.   ", "    ( zz )  ", "   `-____-` ", "  ~~~~~~~~  " };
  static const char* const BREATHE[5] = { "            ", "     .--.   ", "    ( -- )  ", "   `-____-` ", "  ~~~~~~~~  " };
  static const char* const SNORE[5]   = { "      o     ", "     .--.   ", "    ( __ )  ", "   `-____-` ", "  ~~~~~~~~  " };
  static const char* const DEEP[5]    = { "      O     ", "     .--.   ", "    ( __ )  ", "   `-zzzz-` ", "  ~~~~~~~~  " };
  static const char* const DREAM[5]   = { "    . o O   ", "     .--.   ", "    ( UU )  ", "   `-____-` ", "  ~~~~~~~~  " };
  static const char* const TWITCH[5]  = { "            ", "     .--.   ", "    ( ^^ )  ", "   `-____-` ", " ~~~~~~~~~~ " };

  const char* const* P[6] = { TUCK, BREATHE, SNORE, DEEP, DREAM, TWITCH };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,2,1,
    0,1,0,1,
    3,3,3,3,
    4,4,4,4,
    1,5,1,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xD8FE);

  // Z particles drift up-left (snails are slow, gentle Z's)
  int p1 = (t)     % 12;
  int p2 = (t + 5) % 12;
  int p3 = (t + 9) % 12;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 8 - p1, BUDDY_Y_OVERLAY + 14 - p1);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 14 - p2, BUDDY_Y_OVERLAY + 10 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 4 - p3 / 2, BUDDY_Y_OVERLAY + 6 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses — antennae wiggle, eye peeks, slime trail
static void doIdle(uint32_t t) {
  static const char* const REST[5]    = { "  \\\\  /     ", "    .--.    ", "  _( oo )_  ", " (___@@___) ", "  ~~~~~~~~  " };
  static const char* const LOOK_L[5]  = { "  \\\\  /     ", "    .--.    ", "  _(<<  )_  ", " (___@@___) ", "  ~~~~~~~~  " };
  static const char* const LOOK_R[5]  = { "  \\\\  /     ", "    .--.    ", "  _(  >>)_  ", " (___@@___) ", "  ~~~~~~~~  " };
  static const char* const LOOK_U[5]  = { "  \\\\  /     ", "    .--.    ", "  _( ^^ )_  ", " (___@@___) ", "  ~~~~~~~~  " };
  static const char* const BLINK[5]   = { "  \\\\  /     ", "    .--.    ", "  _( -- )_  ", " (___@@___) ", "  ~~~~~~~~  " };
  static const char* const WIGGLE_L[5]= { "  /  //     ", "    .--.    ", "  _( oo )_  ", " (___@@___) ", "  ~~~~~~~~  " };
  static const char* const WIGGLE_R[5]= { "  \\\\  \\\\    ", "    .--.    ", "  _( oo )_  ", " (___@@___) ", "  ~~~~~~~~  " };
  static const char* const MUNCH_A[5] = { "  \\\\  /     ", "    .--.    ", "  _( oo )_  ", " (___ww___) ", "  ~~~~~~~~  " };
  static const char* const MUNCH_B[5] = { "  \\\\  /     ", "    .--.    ", "  _( oo )_  ", " (___WW___) ", "  ~~~~~~~~  " };
  static const char* const STRETCH[5] = { "   \\  /     ", "    .--.    ", "  _( oo )__ ", " (___@@____)", " ~~~~~~~~~~ " };

  const char* const* P[10] = { REST, LOOK_L, LOOK_R, LOOK_U, BLINK, WIGGLE_L, WIGGLE_R, MUNCH_A, MUNCH_B, STRETCH };
  static const uint8_t SEQ[] = {
    0,0,0,1,0,2,0,4,
    0,5,6,5,6,0,
    0,0,3,3,0,4,
    7,8,7,8,0,
    9,9,9,0,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xD8FE);

  // Slow slime drip below trail
  if ((t / 6) & 1) {
    buddySetColor(BUDDY_CYAN);
    buddySetCursor(BUDDY_X_CENTER - 10, BUDDY_Y_BASE + 32);
    buddyPrint(".");
  }
}

// ─── BUSY ───  ~10s cycle, 6 poses — shell spiral spinning, focused eyes, dot ticker
static void doBusy(uint32_t t) {
  static const char* const SPIRAL_A[5]= { "  \\\\  /     ", "    .o-.    ", "  _( vv )_  ", " (___--___) ", "  ~~~~~~~~  " };
  static const char* const SPIRAL_B[5]= { "  \\\\  /     ", "    .-o.    ", "  _( vv )_  ", " (___--___) ", "  ~~~~~~~~  " };
  static const char* const SPIRAL_C[5]= { "  \\\\  /     ", "    .--.    ", "  _( vv )o  ", " (___--___) ", "  ~~~~~~~~  " };
  static const char* const SPIRAL_D[5]= { "  \\\\  /     ", "    .--.    ", "  o( vv )_  ", " (___--___) ", "  ~~~~~~~~  " };
  static const char* const THINK[5]   = { " ? \\\\  /    ", "    .--.    ", "  _( ^^ )_  ", " (___..___) ", "  ~~~~~~~~  " };
  static const char* const EUREKA[5]  = { " * \\\\  / *  ", "    .--.    ", "  _( OO )_  ", " (___^^___) ", "  ~~~~~~~~  " };

  const char* const* P[6] = { SPIRAL_A, SPIRAL_B, SPIRAL_C, SPIRAL_D, THINK, EUREKA };
  static const uint8_t SEQ[] = {
    0,1,2,3, 0,1,2,3, 4,4, 0,1,2,3, 4,4, 5,0,1,2,3
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xD8FE);

  static const char* const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 24, BUDDY_Y_OVERLAY + 12);
  buddyPrint(DOTS[t % 6]);
}

// ─── ATTENTION ───  ~8s cycle, 6 poses — antennae periscope up, scanning
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "  ||  ||    ", "    .--.    ", "  _( OO )_  ", " (___OO___) ", "  ~~~~~~~~  " };
  static const char* const SCAN_L[5]  = { "  \\\\  \\\\    ", "    .--.    ", "  _(OO  )_  ", " (___OO___) ", "  ~~~~~~~~  " };
  static const char* const SCAN_R[5]  = { "  //  //    ", "    .--.    ", "  _(  OO)_  ", " (___OO___) ", "  ~~~~~~~~  " };
  static const char* const SCAN_U[5]  = { "  ||  ||    ", "    .--.    ", "  _( ^^ )_  ", " (___OO___) ", "  ~~~~~~~~  " };
  static const char* const TENSE[5]   = { "  ||  ||    ", "   /.--.\\   ", "  _( OO )_  ", "/(___OO___)\\", " /~~~~~~~~\\ " };
  static const char* const HUSH[5]    = { "  \\|  |/    ", "    .--.    ", "  _( oo )_  ", " (___..___) ", "  ~~~~~~~~  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, TENSE, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0xD8FE, xOff);

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

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses — shell hops, antennae fling, confetti rain
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "  \\\\  /     ", "    .--.    ", "  _( ^^ )_  ", " (___ww___) ", " /~~~~~~~~\\ " };
  static const char* const JUMP[5]    = { "  \\^  ^/    ", "   _.--._   ", "  ( ^  ^ )  ", " (___ww___) ", "  ~~~~~~~~  " };
  static const char* const PEAK[5]    = { "  \\(  )/    ", "   _.--._   ", "  ( **  ) ) ", " (___WW___) ", "  ~~~~~~~~  " };
  static const char* const SPIN_L[5]  = { "  /\\        ", "    .--.    ", "  _( <<)_   ", " (___ww___) ", "  ~~~~~~~~  " };
  static const char* const SPIN_R[5]  = { "        /\\  ", "    .--.    ", "   _(>> )_  ", " (___ww___) ", "  ~~~~~~~~  " };
  static const char* const POSE[5]    = { "  \\\\  // *  ", "  * .--.    ", "  _( ^^ )_  ", "/(___WW___)\\", "  ~~~~~~~~  " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xD8FE);

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

// ─── DIZZY ───  ~5.6s cycle, 5 poses — shell wobble, antennae droop, orbiting stars
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]  = { "  \\\\\\       ", "   .--.     ", "  ( @x )    ", " (___~v___) ", "  ~~~~~~~~  " };
  static const char* const TILT_R[5]  = { "       ///  ", "     .--.   ", "    ( x@ )  ", " (___v~___) ", "  ~~~~~~~~  " };
  static const char* const WOOZY[5]   = { "  \\~  ~/    ", "    .--.    ", "  _( @x )_  ", " (___~v___) ", "  ~~~~~~~~  " };
  static const char* const WOOZY2[5]  = { "  ~\\  /~    ", "    .--.    ", "  _( x@ )_  ", " (___v~___) ", "  ~~~~~~~~  " };
  static const char* const STUMBLE[5] = { "  \\v  v/    ", "    .--.    ", "  _( @@ )_  ", " (___--___) ", " /~_~~_~~\\  " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, STUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xD8FE, X_SHIFT[beat]);

  static const int8_t OX[] = { 0, 5, 7, 5, 0, -5, -7, -5 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 4) % 8;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 4 + OY[p1]);
  buddyPrint("*");
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 4 + OY[p2]);
  buddyPrint("*");
}

// ─── HEART ───  ~10s cycle, 5 poses — twined antennae, heart eyes, rising hearts
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "  \\\\  /     ", "    .--.    ", "  _( ^^ )_  ", " (___ww___) ", "  ~~~~~~~~  " };
  static const char* const BLUSH[5]   = { "  \\\\  /     ", "    .--.    ", "  _(#^^#)_  ", " (___ww___) ", "  ~~~~~~~~  " };
  static const char* const EYES_C[5]  = { "  \\\\  /     ", "    .--.    ", "  _(<3<3)_  ", " (___ww___) ", "  ~~~~~~~~  " };
  static const char* const TWINE[5]   = { "  \\\\//      ", "    .--.    ", "  _( @@ )_  ", " (___ww___) ", " /~~~~~~~~\\ " };
  static const char* const SIGH[5]    = { "  \\\\  /     ", "    .--.    ", "  _( -- )_  ", " (___^^___) ", "  ~~~~~~~~  " };

  const char* const* P[5] = { DREAMY, BLUSH, EYES_C, TWINE, SIGH };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xD8FE);

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

}  // namespace snail

extern const Species SNAIL_SPECIES = { "snail", 0xD8FE, { snail::doSleep, snail::doIdle, snail::doBusy, snail::doAttention, snail::doCelebrate, snail::doDizzy, snail::doHeart } };
