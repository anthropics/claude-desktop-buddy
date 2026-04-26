#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace axolotl {

// Base silhouette: }~ ... ~{ are the feathery gills, ( .--. ) is the smile,
// (_/  \_) are the little feet. Always smiling, always bubbling.

// ─── SLEEP ───  ~12s cycle, 6 poses + bubble stream
static void doSleep(uint32_t t) {
  static const char* const FLOAT[5]   = { "            ", "}~(______)~{", "}~( -  - )~{", "  ( .__. )  ", "  (_/  \\_)  " };
  static const char* const BREATHE[5] = { "            ", "}~(______)~{", "}~( _  _ )~{", "  ( .__. )  ", "  (_/  \\_)  " };
  static const char* const DEEP[5]    = { "            ", "~}(______){~", "~}( _  _ ){~", "  ( ____ )  ", "  ~_/  \\_~  " };
  static const char* const CURL[5]    = { "            ", "            ", "}~(.____.)~{", " }~(- __ -)~{", "  (__//__)  " };
  static const char* const SIDE[5]    = { "            ", "            ", "  }~~~~~_   ", " }~( -- -)= ", "  (__----)  " };
  static const char* const SNORE[5]   = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( oOOo )  ", "  (_/  \\_)  " };

  const char* const* P[6] = { FLOAT, BREATHE, DEEP, CURL, SIDE, SNORE };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,2,1,
    0,1,0,1,
    3,3,4,4,3,4,
    3,3,
    1,5,1,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFB1E);

  // Bubbles drift up
  int p1 = (t)     % 12;
  int p2 = (t + 5) % 12;
  int p3 = (t + 9) % 12;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 18 + (p1 % 3), BUDDY_Y_OVERLAY + 20 - p1 * 2);
  buddyPrint("o");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 26 - (p2 % 4), BUDDY_Y_OVERLAY + 18 - p2);
  buddyPrint("O");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 14 + (p3 % 5), BUDDY_Y_OVERLAY + 12 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses + lazy bubbles
static void doIdle(uint32_t t) {
  static const char* const REST[5]    = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char* const LOOK_L[5]  = { "            ", "}~(______)~{", "}~(o   o )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char* const LOOK_R[5]  = { "            ", "}~(______)~{", "}~( o   o)~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char* const LOOK_U[5]  = { "            ", "}~(______)~{", "}~( ^  ^ )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char* const BLINK[5]   = { "            ", "}~(______)~{", "}~( -  - )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char* const GILL_L[5]  = { "            ", "}}~(_____)~{", "}}~(o  o )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char* const GILL_R[5]  = { "            ", "}~(_____)~{{", "}~(o  o )~{{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char* const CHEW_A[5]  = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( wwww )  ", "  (_/  \\_)  " };
  static const char* const CHEW_B[5]  = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( WWWW )  ", "  (_/  \\_)  " };
  static const char* const WIGGLE[5]  = { "            ", "~}(______){~", "~}( o  o ){~", "  ( .--. )  ", "  ~_/  \\_~  " };

  const char* const* P[10] = { REST, LOOK_L, LOOK_R, LOOK_U, BLINK, GILL_L, GILL_R, CHEW_A, CHEW_B, WIGGLE };
  static const uint8_t SEQ[] = {
    0,0,0,1,0,2,0,4,
    0,5,0,6,0,
    7,8,7,8,
    0,0,3,3,0,4,
    9,9,0,0,
    1,2,1,2,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFB1E);

  // single lazy bubble
  int p = (t / 2) % 14;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 24, BUDDY_Y_OVERLAY + 16 - p);
  buddyPrint(p & 1 ? "o" : "O");
}

// ─── BUSY ───  ~10s cycle, 6 poses + dot ticker
static void doBusy(uint32_t t) {
  static const char* const TYPE_A[5]  = { "            ", "}~(______)~{", "}~( v  v )~{", "  (  --  )  ", " /(_/  \\_)\\ " };
  static const char* const TYPE_B[5]  = { "            ", "}~(______)~{", "}~( v  v )~{", "  (  __  )  ", " \\(_/  \\_)/ " };
  static const char* const THINK[5]   = { "      ?     ", "}~(______)~{", "}~( ^  ^ )~{", "  (  ..  )  ", "  (_/  \\_)  " };
  static const char* const SCRIBBLE[5]= { "      /     ", "}~(_____)~{ ", "}~( o  o )~{", "  ( .--. ) /", "  (_/  \\_)  " };
  static const char* const EUREKA[5]  = { "      *     ", "}~(______)~{", "}~( O  O )~{", "  (  ^^  )  ", " /(_/  \\_)\\ " };
  static const char* const RELIEF[5]  = { "    ~~~     ", "}~(______)~{", "}~( -  - )~{", "  (  __  )  ", "  (_/  \\_)  " };

  const char* const* P[6] = { TYPE_A, TYPE_B, THINK, SCRIBBLE, EUREKA, RELIEF };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFB1E);

  static const char* const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 14);
  buddyPrint(DOTS[t % 6]);

  // tiny bubbles
  int b = (t * 2) % 10;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER - 28, BUDDY_Y_OVERLAY + 18 - b);
  buddyPrint("o");
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "    ^  ^    ", "}}~(______)~{{", "}}~( O  O )~{{", "  (  O   )  ", "  (_/  \\_)  " };
  static const char* const SCAN_L[5]  = { "    ^  ^    ", "}}~(______)~{{", "}}~(O    O)~{{", "  (  O   )  ", "  (_/  \\_)  " };
  static const char* const SCAN_R[5]  = { "    ^  ^    ", "}}~(______)~{{", "}}~(O    O)~{{", "  (   O  )  ", "  (_/  \\_)  " };
  static const char* const SCAN_U[5]  = { "    ^  ^    ", "}}~(______)~{{", "}}~( ^  ^ )~{{", "  (  O   )  ", "  (_/  \\_)  " };
  static const char* const TENSE[5]   = { "    ^  ^    ", "}}}~(____)~{{{", "}}}~( O  O)~{{{", "  (  O   )  ", " /(_/  \\_)\\ " };
  static const char* const HUSH[5]    = { "    ^  ^    ", "}~(______)~{", "}~( o  o )~{", "  (  .   )  ", "  (_/  \\_)  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, TENSE, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0xFB1E, xOff);

  if ((t / 2) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER - 4, BUDDY_Y_OVERLAY);
    buddyPrint("!");
  }
  if ((t / 3) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER + 4, BUDDY_Y_OVERLAY + 4);
    buddyPrint("!");
  }
}

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + confetti rain
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "            ", "}~(______)~{", "}~( ^  ^ )~{", "  (  ww  )  ", " /(_/  \\_)\\ " };
  static const char* const JUMP[5]    = { "  \\(    )/  ", "}~(______)~{", "}~( ^  ^ )~{", "  (  ww  )  ", "  (_/  \\_)  " };
  static const char* const PEAK[5]    = { "  \\^    ^/  ", "}~(______)~{", "}~( ^  ^ )~{", "  (  WW  )  ", "  (_/  \\_)  " };
  static const char* const SPIN_L[5]  = { "            ", "}~(______)~{", "}~(<    <)~{", "  (  ww  ) /", "  (_/  \\_)  " };
  static const char* const SPIN_R[5]  = { "            ", "}~(______)~{", "}~(>    >)~{", "\\ (  ww  )  ", "  (_/  \\_)  " };
  static const char* const POSE[5]    = { "    \\__/    ", "}~(______)~{", "}~( ^  ^ )~{", "/ (  WW  ) \\", "  (_/  \\_)  " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xFB1E);

  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_HEART, BUDDY_CYAN, BUDDY_WHITE, BUDDY_GREEN, BUDDY_PURPLE };
  for (int i = 0; i < 7; i++) {
    int phase = (t * 2 + i * 9) % 24;
    int x = BUDDY_X_CENTER - 36 + i * 12;
    int y = BUDDY_Y_OVERLAY - 6 + phase;
    if (y > BUDDY_Y_BASE + 20 || y < 0) continue;
    buddySetColor(cols[i % 6]);
    buddySetCursor(x, y);
    buddyPrint((i + (int)(t/2)) & 1 ? "*" : "o");
  }
}

// ─── DIZZY ───  ~6s cycle, 5 poses + orbiting stars
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]  = { "            ", "}~(______)~{ ", "}~( @  @ )~{ ", "  ( ~~~~ )  ", "  (_/  \\_)  " };
  static const char* const TILT_R[5]  = { "            ", " }~(______)~{", " }~( @  @ )~{", "  ( ~~~~ )  ", "  (_/  \\_)  " };
  static const char* const WOOZY[5]   = { "            ", "}~(______)~{", "}~( x  @ )~{", "  ( ~vv~ )  ", "  (_/  \\_)  " };
  static const char* const WOOZY2[5]  = { "            ", "}~(______)~{", "}~( @  x )~{", "  ( vv~~ )  ", "  (_/  \\_)  " };
  static const char* const STUMBLE[5] = { "            ", "~}(______){~", "~}( @  @ ){~", "  (  --  )  ", " /~_/  \\_~\\ " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, STUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFB1E, X_SHIFT[beat]);

  static const int8_t OX[] = { 0, 5, 7, 5, 0, -5, -7, -5 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 4) % 8;
  uint8_t p3 = (t + 2) % 8;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 6 + OY[p1]);
  buddyPrint("*");
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 6 + OY[p2]);
  buddyPrint("*");
  buddySetColor(BUDDY_HEART);
  buddySetCursor(BUDDY_X_CENTER + OX[p3] - 2, BUDDY_Y_OVERLAY + 6 + OY[p3]);
  buddyPrint("o");
}

// ─── HEART ───  ~10s cycle, 6 poses + rising heart stream
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "            ", "}~(______)~{", "}~( ^  ^ )~{", "  ( .vv. )  ", "  (_/  \\_)  " };
  static const char* const BLUSH[5]   = { "            ", "}~(______)~{", "}~(#^  ^#)~{", "  ( .vv. )  ", "  (_/  \\_)  " };
  static const char* const EYES_C[5]  = { "            ", "}~(______)~{", "}~(<3  <3)~{", "  ( .vv. )  ", "  (_/  \\_)  " };
  static const char* const TWIRL[5]   = { "            ", "~}(______){~", "~}( @  @ ){~", "  ( .vv. )  ", " /(_/  \\_)\\ " };
  static const char* const SIGH[5]    = { "            ", "}~(______)~{", "}~( -  - )~{", "  ( ^^^^ )  ", "  (_/  \\_)  " };
  static const char* const WINK[5]    = { "            ", "}~(______)~{", "}~( ^  - )~{", "  ( .vv. )  ", "  (_/  \\_)  " };

  const char* const* P[6] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH, WINK };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 5,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xFB1E);

  buddySetColor(BUDDY_HEART);
  for (int i = 0; i < 5; i++) {
    int phase = (t + i * 4) % 16;
    int y = BUDDY_Y_OVERLAY + 16 - phase;
    if (y < -2 || y > BUDDY_Y_BASE) continue;
    int x = BUDDY_X_CENTER - 20 + i * 8 + ((phase / 3) & 1) * 2 - 1;
    buddySetCursor(x, y);
    buddyPrint("v");
  }

  // a couple of pink bubbles too
  int b = (t + 3) % 14;
  buddySetColor(BUDDY_HEART);
  buddySetCursor(BUDDY_X_CENTER + 26, BUDDY_Y_OVERLAY + 16 - b);
  buddyPrint(b & 1 ? "o" : "O");
}

}  // namespace axolotl

extern const Species AXOLOTL_SPECIES = {
  "axolotl",
  0xFB1E,
  { axolotl::doSleep, axolotl::doIdle, axolotl::doBusy, axolotl::doAttention,
    axolotl::doCelebrate, axolotl::doDizzy, axolotl::doHeart }
};
