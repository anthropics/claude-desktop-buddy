#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace rabbit {

// ─── SLEEP ───  ~12s cycle, 6 poses
static void doSleep(uint32_t t) {
  static const char* const CURL[5]    = { "            ", "    (\\_/)   ", "   ( -.- )  ", "  (zzz___)  ", "   `\"\"\"\"`   " };
  static const char* const BREATHE[5] = { "            ", "    (\\_/)   ", "   ( -_- )  ", "  (___zz_)  ", "   `\"\"\"\"`   " };
  static const char* const EARS_DN[5] = { "            ", "    (v_v)   ", "   ( -.- )  ", "  (___..)   ", "   `\"\"\"\"`   " };
  static const char* const SIDE[5]    = { "            ", "            ", "   .-^^^-.  ", "  ( -- zZ ) ", "   `~~~~`   " };
  static const char* const TWITCH[5]  = { "            ", "    (\\_/)   ", "   ( -.- )  ", "  (~__zz_)  ", "   `\"\"\"\"`   " };
  static const char* const DREAM[5]   = { "            ", "    (\\_/)   ", "   ( u.u )  ", "  (___oo_)  ", "   `\"\"\"\"`   " };

  const char* const* P[6] = { CURL, BREATHE, EARS_DN, SIDE, TWITCH, DREAM };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,4,1,
    0,1,0,1,
    3,3,3,3,
    2,2,
    1,5,1,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF);

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
  static const char* const REST[5]    = { "    (\\_/)   ", "   ( o o )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const LOOK_L[5]  = { "    (\\_/)   ", "   (o  o )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const LOOK_R[5]  = { "    (\\_/)   ", "   ( o  o)  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const LOOK_U[5]  = { "    (\\_/)   ", "   ( ^ ^ )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const BLINK[5]   = { "    (\\_/)   ", "   ( - - )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const EAR_L[5]   = { "    (/_\\)   ", "   ( o o )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const NOSE_A[5]  = { "    (\\_/)   ", "   ( o o )  ", "  =(  ^  )= ", "   (\")_(\")  ", "            " };
  static const char* const NOSE_B[5]  = { "    (\\_/)   ", "   ( o o )  ", "  =(  *  )= ", "   (\")_(\")  ", "            " };
  static const char* const CARROT[5]  = { "    (\\_/)   ", "   ( o o )  ", "  =(  V  )= >--", "   (\")_(\")  ", "            " };
  static const char* const STRETCH[5] = { "   (\\___/)  ", "  ( o   o ) ", " =(   v   )=", "   (\")_(\")  ", "            " };

  const char* const* P[10] = { REST, LOOK_L, LOOK_R, LOOK_U, BLINK, EAR_L, NOSE_A, NOSE_B, CARROT, STRETCH };
  static const uint8_t SEQ[] = {
    0,0,0,1,0,2,0,4,
    0,5,0,0,
    6,7,6,7,
    0,0,3,3,0,4,
    8,8,0,0,
    9,9,0,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF);
}

// ─── BUSY ───  ~10s cycle, 6 poses + dot ticker
static void doBusy(uint32_t t) {
  static const char* const DIG_A[5]   = { "    (\\_/)   ", "   ( v v )  ", "  =(  v  )= ", "  /(\")_(\")\\ ", "  ~~~~~~~~  " };
  static const char* const DIG_B[5]   = { "    (\\_/)   ", "   ( v v )  ", "  =(  v  )= ", "  \\(\")_(\")/  ", "  ~~~~~~~~  " };
  static const char* const THINK[5]   = { "  ? (\\_/)   ", "   ( o o )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const GNAW[5]    = { "    (\\_/) >-", "   ( o o )==", "  =(  W  )= ", "   (\")_(\")  ", "            " };
  static const char* const EUREKA[5]  = { "  * (\\_/) * ", "   ( O O )  ", "  =(  ^  )= ", "  /(\")_(\")\\ ", "            " };
  static const char* const WIPE[5]    = { "    (\\_/)   ", "   ( - - )  ", "  =(  _  )= ", "   (\")_(\")  ", "    ~~~~    " };

  const char* const* P[6] = { DIG_A, DIG_B, THINK, GNAW, EUREKA, WIPE };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF);

  static const char* const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 14);
  buddyPrint(DOTS[t % 6]);
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "   /|  |\\   ", "  /(\\_/)\\   ", "  ( O  O )  ", "  =(  v  )= ", "   (\")_(\")  " };
  static const char* const SCAN_L[5]  = { "   /|  |\\   ", "  /(\\_/)\\   ", "  (O   O )  ", "  =(  v  )= ", "   (\")_(\")  " };
  static const char* const SCAN_R[5]  = { "   /|  |\\   ", "  /(\\_/)\\   ", "  ( O   O)  ", "  =(  v  )= ", "   (\")_(\")  " };
  static const char* const SCAN_U[5]  = { "   /|  |\\   ", "  /(\\_/)\\   ", "  ( ^  ^ )  ", "  =(  v  )= ", "   (\")_(\")  " };
  static const char* const TENSE[5]   = { "  /|/  \\|\\  ", "  /(\\_/)\\   ", "  ( O  O )  ", "  =(  v  )= ", "  /(\")_(\")\\ " };
  static const char* const HUSH[5]    = { "   /|  |\\   ", "   (\\_/)    ", "  ( o  o )  ", "  =(  .  )= ", "   (\")_(\")  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, TENSE, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0xFFFF, xOff);

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
  static const char* const CROUCH[5]  = { "    (\\_/)   ", "   ( ^ ^ )  ", "  =(  v  )= ", "   (\")_(\")  ", "  ~~~~~~~~  " };
  static const char* const HOP[5]     = { "  \\ (\\_/) / ", "   ( ^ ^ )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const PEAK[5]    = { "  \\ (\\o/) / ", "   ( ^ ^ )  ", "  =(  W  )= ", "   ('')_('') ", "            " };
  static const char* const SPIN_L[5]  = { "    (\\_/)   ", "   ( <  < ) ", "  =(  v  )=/", "   (\")_(\")  ", "            " };
  static const char* const SPIN_R[5]  = { "    (\\_/)   ", "   ( >  > ) ", " \\=(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const POSE[5]    = { "   \\(\\_/)/  ", "   ( ^ ^ )  ", "  =(  W  )= ", "  /(\")_(\")\\ ", "            " };

  const char* const* P[6] = { CROUCH, HOP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-4,-8,-4,0, 0,0,0,0, 0,-4,-8,-4,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xFFFF);

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
  static const char* const TILT_L[5]  = { "   (\\_/)    ", "  ( @ @ )   ", " =(  ~  )=  ", "  (\")_(\")   ", "            " };
  static const char* const TILT_R[5]  = { "    (\\_/)   ", "    ( @ @ ) ", "   =(  ~  )=", "    (\")_(\") ", "            " };
  static const char* const WOOZY[5]   = { "    (\\_/)   ", "   ( x @ )  ", "  =(  ~v )= ", "   (\")_(\")  ", "            " };
  static const char* const WOOZY2[5]  = { "    (\\_/)   ", "   ( @ x )  ", "  =( v~  )= ", "   (\")_(\")  ", "            " };
  static const char* const FLOP[5]    = { "    (\\_/)   ", "   ( @ @ )  ", "  =(  -  )= ", "  /(\")_(\")\\ ", " ~~~~~~~~~~ " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, FLOP };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF, X_SHIFT[beat]);

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
  static const char* const DREAMY[5]  = { "    (\\_/)   ", "   ( ^ ^ )  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const BLUSH[5]   = { "    (\\_/)   ", "   (#^ ^#)  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const EYES_C[5]  = { "    (\\_/)   ", "   (<3 <3)  ", "  =(  v  )= ", "   (\")_(\")  ", "            " };
  static const char* const TWIRL[5]   = { "   \\(\\_/)/  ", "   ( @ @ )  ", "  =(  v  )= ", "  /(\")_(\")\\ ", "            " };
  static const char* const SIGH[5]    = { "    (v_v)   ", "   ( - - )  ", "  =(  ^  )= ", "   (\")_(\")  ", "            " };

  const char* const* P[5] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xFFFF);

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

}  // namespace rabbit

extern const Species RABBIT_SPECIES = { "rabbit", 0xFFFF, { rabbit::doSleep, rabbit::doIdle, rabbit::doBusy, rabbit::doAttention, rabbit::doCelebrate, rabbit::doDizzy, rabbit::doHeart } };
