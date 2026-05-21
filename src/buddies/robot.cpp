#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace robot {

// ─── SLEEP ───  ~12s cycle, 6 poses, robot in low-power mode
static void doSleep(uint32_t t) {
  static const char* const PWR_DN[5]  = { "            ", "   .[__].   ", "  [ -    - ]", "  [ ____ ]  ", "  `------'  " };
  static const char* const DIM_A[5]   = { "            ", "   .[..].   ", "  [ .    . ]", "  [ ____ ]  ", "  `------'  " };
  static const char* const DIM_B[5]   = { "            ", "   .[  ].   ", "  [        ]", "  [ ____ ]  ", "  `------'  " };
  static const char* const PING[5]    = { "            ", "   .[||].   ", "  [ -    - ]", "  [ z__z ]  ", "  `------'  " };
  static const char* const DREAM[5]   = { "    .[*].   ", "   .[||].   ", "  [ -    - ]", "  [ zzzz ]  ", "  `------'  " };
  static const char* const REBOOT[5]  = { "            ", "   .[..].   ", "  [ o    - ]", "  [ ____ ]  ", "  `------'  " };

  const char* const* P[6] = { PWR_DN, DIM_A, DIM_B, PING, DREAM, REBOOT };
  static const uint8_t SEQ[] = {
    0,1,2,1,0,1,2,1,
    0,0,3,3,
    4,4,4,3,
    0,1,2,1,0,
    5,0,1,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC618);

  // Z particles drift up-right (low power beeps)
  int p1 = (t)     % 10;
  int p2 = (t + 4) % 10;
  int p3 = (t + 7) % 10;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 20 + p1, BUDDY_Y_OVERLAY + 18 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 26 + p2, BUDDY_Y_OVERLAY + 14 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 16 + p3 / 2, BUDDY_Y_OVERLAY + 10 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses with stiff mechanical motion
static void doIdle(uint32_t t) {
  static const char* const REST[5]    = { "            ", "   .[||].   ", "  [ o    o ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const SCAN_L[5]  = { "            ", "   .[||].   ", "  [o     o ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const SCAN_R[5]  = { "            ", "   .[||].   ", "  [ o     o]", "  [ ==== ]  ", "  `------'  " };
  static const char* const BLINK[5]   = { "            ", "   .[||].   ", "  [ -    - ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const ANT_L[5]   = { "            ", "   .[\\\\].   ", "  [ o    o ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const ANT_R[5]   = { "            ", "   .[//].   ", "  [ o    o ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const BEEP_A[5]  = { "            ", "   .[||].   ", "  [ o    o ]", "  [ -==- ]  ", "  `------'  " };
  static const char* const BEEP_B[5]  = { "            ", "   .[||].   ", "  [ o    o ]", "  [ =--= ]  ", "  `------'  " };
  static const char* const PING[5]    = { "    .[*].   ", "   .[||].   ", "  [ ^    ^ ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const CLICK[5]   = { "            ", "   .[||].   ", "  [ o    o ]", "  [ ==== ]  ", " /`------'\\ " };

  const char* const* P[10] = { REST, SCAN_L, SCAN_R, BLINK, ANT_L, ANT_R, BEEP_A, BEEP_B, PING, CLICK };
  static const uint8_t SEQ[] = {
    0,0,1,1,0,2,2,0,
    3,0,0,
    4,5,4,5,0,
    6,7,6,7,0,
    0,8,8,0,
    9,9,0,3,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC618);

  // Antenna LED blink
  if ((t / 4) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER - 1, BUDDY_Y_BASE - 4);
    buddyPrint(".");
  }
}

// ─── BUSY ───  ~10s cycle, 6 poses + binary stream
static void doBusy(uint32_t t) {
  static const char* const CALC_A[5]  = { "    01010   ", "   .[||].   ", "  [ #    # ]", "  [ ==== ]  ", " /`------'\\ " };
  static const char* const CALC_B[5]  = { "    10101   ", "   .[||].   ", "  [ #    # ]", "  [ -==- ]  ", " \\`------'/ " };
  static const char* const PROC[5]    = { "     ?      ", "   .[||].   ", "  [ ^    ^ ]", "  [ .... ]  ", "  `------'  " };
  static const char* const WHIRR[5]   = { "    [@@]    ", "   .[||].   ", "  [ o    o ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const DING[5]    = { "     !      ", "   .[||].   ", "  [ O    O ]", "  [ ^^^^ ]  ", " /`------'\\ " };
  static const char* const COOL[5]    = { "    ~~~     ", "   .[||].   ", "  [ -    - ]", "  [ ____ ]  ", "  `------'  " };

  const char* const* P[6] = { CALC_A, CALC_B, PROC, WHIRR, DING, COOL };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1, 5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC618);

  // Binary stream cycling next to body
  static const char* const BITS[] = { "1  ", "10 ", "101", "010", "10 ", "1  " };
  buddySetColor(BUDDY_GREEN);
  buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 14);
  buddyPrint(BITS[t % 6]);
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "    [!]     ", "   .[||].   ", "  [ O    O ]", "  [ #### ]  ", " /`------'\\ " };
  static const char* const SCAN_L[5]  = { "    [!]     ", "   .[\\\\].   ", "  [O     O ]", "  [ #### ]  ", " /`------'\\ " };
  static const char* const SCAN_R[5]  = { "    [!]     ", "   .[//].   ", "  [ O     O]", "  [ #### ]  ", " /`------'\\ " };
  static const char* const SCAN_U[5]  = { "    [!]     ", "   .[||].   ", "  [ ^    ^ ]", "  [ #### ]  ", " /`------'\\ " };
  static const char* const SIREN[5]   = { "    {!!}    ", "   .[||].   ", "  [ X    X ]", "  [ #### ]  ", "//`------'\\\\" };
  static const char* const HUSH[5]    = { "    [.]     ", "   .[||].   ", "  [ o    o ]", "  [ .... ]  ", "  `------'  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, SIREN, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0xC618, xOff);

  // Pulsing exclamations and warning lights
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
  if ((t / 2) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER - 1, BUDDY_Y_BASE - 4);
    buddyPrint("*");
  }
}

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + sparks/confetti
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "            ", "   .[||].   ", "  [ ^    ^ ]", "  [ ==== ]  ", " /`------'\\ " };
  static const char* const JUMP[5]    = { "  \\[||]/    ", "   .----.   ", "  [ ^    ^ ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const PEAK[5]    = { "  \\[**]/    ", "   .----.   ", "  [ O    O ]", "  [ ^^^^ ]  ", "  `------'  " };
  static const char* const SPIN_L[5]  = { "            ", "   .[\\\\].   ", "  [ <    < ]", "  [ ==== ] /", "  `------'  " };
  static const char* const SPIN_R[5]  = { "            ", "   .[//].   ", "  [ >    > ]", " \\[ ==== ]  ", "  `------'  " };
  static const char* const POSE[5]    = { "    [**]    ", "   .[||].   ", "  [ ^    ^ ]", " /[ #### ]\\ ", "  `------'  " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xC618);

  // Sparks and bolts shooting out
  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_CYAN, BUDDY_GREEN, BUDDY_WHITE, BUDDY_PURPLE };
  for (int i = 0; i < 6; i++) {
    int phase = (t * 2 + i * 11) % 22;
    int x = BUDDY_X_CENTER - 36 + i * 14;
    int y = BUDDY_Y_OVERLAY - 6 + phase;
    if (y > BUDDY_Y_BASE + 20 || y < 0) continue;
    buddySetColor(cols[i % 5]);
    buddySetCursor(x, y);
    buddyPrint((i + (int)(t/2)) & 1 ? "+" : "*");
  }
}

// ─── DIZZY ───  ~5.6s cycle, 5 poses + orbiting bolts (system glitch)
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]  = { "            ", "  .[||].    ", " [ x    x ] ", " [ ~~~~ ]   ", "  `------'  " };
  static const char* const TILT_R[5]  = { "            ", "    .[||].  ", "  [ x    x ]", "   [ ~~~~ ] ", "  `------'  " };
  static const char* const GLITCH[5]  = { "            ", "   .[/\\].   ", "  [ X    @ ]", "  [ #v#v ]  ", "  `--__--'  " };
  static const char* const GLITCH2[5] = { "            ", "   .[\\/].   ", "  [ @    X ]", "  [ v#v# ]  ", "  `--__--'  " };
  static const char* const CRASH[5]   = { "            ", "   .[??].   ", "  [ x    x ]", "  [ ____ ]  ", " /`-_--_-'\\ " };

  const char* const* P[5] = { TILT_L, TILT_R, GLITCH, GLITCH2, CRASH };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xC618, X_SHIFT[beat]);

  // Orbiting error symbols
  static const int8_t OX[] = { 0, 5, 7, 5, 0, -5, -7, -5 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 4) % 8;
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 6 + OY[p1]);
  buddyPrint("?");
  buddySetColor(BUDDY_RED);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 6 + OY[p2]);
  buddyPrint("x");
}

// ─── HEART ───  ~10s cycle, 5 poses + rising heart stream
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "    [<3]    ", "   .[||].   ", "  [ ^    ^ ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const BLUSH[5]   = { "    [<3]    ", "   .[||].   ", "  [#^    ^#]", "  [ ==== ]  ", "  `------'  " };
  static const char* const EYES_C[5]  = { "    [<3]    ", "   .[||].   ", "  [ <3  <3 ]", "  [ ==== ]  ", "  `------'  " };
  static const char* const TWIRL[5]   = { "    [<3]    ", "   .[||].   ", "  [ @    @ ]", "  [ ==== ]  ", " /`------'\\ " };
  static const char* const SIGH[5]    = { "    [<3]    ", "   .[||].   ", "  [ -    - ]", "  [ ^^^^ ]  ", "  `------'  " };

  const char* const* P[5] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xC618);

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

}  // namespace robot

extern const Species ROBOT_SPECIES = {
  "robot",
  0xC618,
  { robot::doSleep, robot::doIdle, robot::doBusy, robot::doAttention,
    robot::doCelebrate, robot::doDizzy, robot::doHeart }
};
