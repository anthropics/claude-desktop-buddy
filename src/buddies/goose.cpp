#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace goose {

// ─── SLEEP ───  ~12s cycle, 6 poses, head tucked under wing
static void doSleep(uint32_t t) {
  static const char* const TUCK[5]    = { "            ", "    __      ", "   /  \\__   ", "  (  zz  )  ", "   `~~~~`   " };
  static const char* const BREATHE[5] = { "            ", "    __      ", "   /  \\__   ", "  (  --  )  ", "   `~~~~`   " };
  static const char* const SNORE[5]   = { "    o O .   ", "    __      ", "   /  \\__   ", "  (  --  )  ", "   `~~~~`   " };
  static const char* const PEEK[5]    = { "            ", "    _o      ", "   /  \\__   ", "  (  --  )  ", "   `~~~~`   " };
  static const char* const SHIFT[5]   = { "            ", "      __    ", "   __/  \\   ", "  (  zz  )  ", "   `~~~~`   " };
  static const char* const HONK_DR[5] = { "    HnK..   ", "    __      ", "   /  \\__   ", "  (  zZ  )  ", "   `~~~~`   " };

  const char* const* P[6] = { TUCK, BREATHE, SNORE, PEEK, SHIFT, HONK_DR };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,2,1,
    0,1,0,3,
    4,4,4,1,
    0,1,2,2,
    0,1,5,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF);

  // Z particles drift up-right
  int p1 = (t)     % 10;
  int p2 = (t + 4) % 10;
  int p3 = (t + 7) % 10;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 18 + p1, BUDDY_Y_OVERLAY + 18 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 24 + p2, BUDDY_Y_OVERLAY + 14 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 14 + p3 / 2, BUDDY_Y_OVERLAY + 10 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses, swaggering goose
static void doIdle(uint32_t t) {
  static const char* const STAND[5]   = { "            ", "    (>      ", "    ||      ", "  _(__)_    ", "   ^^^^     " };
  static const char* const LOOK_L[5]  = { "            ", "  <)        ", "   ||       ", "  _(__)_    ", "   ^^^^     " };
  static const char* const LOOK_R[5]  = { "            ", "      (>    ", "      ||    ", "  _(__)_    ", "   ^^^^     " };
  static const char* const LOOK_U[5]  = { "    /\\      ", "    (>      ", "    ||      ", "  _(__)_    ", "   ^^^^     " };
  static const char* const BLINK[5]   = { "            ", "    (->     ", "    ||      ", "  _(__)_    ", "   ^^^^     " };
  static const char* const PREEN[5]   = { "            ", "    (v      ", "    |\\__    ", "  _(__)_    ", "   ^^^^     " };
  static const char* const PECK[5]    = { "            ", "            ", "    (>      ", "    || _    ", "  _(__)_    " };
  static const char* const STEP_L[5]  = { "            ", "    (>      ", "    ||      ", "  _(__)_    ", "   ^/ ^     " };
  static const char* const STEP_R[5]  = { "            ", "    (>      ", "    ||      ", "  _(__)_    ", "   ^ \\^     " };
  static const char* const WING_FL[5] = { "            ", "    (>      ", " <==||==>   ", "  _(__)_    ", "   ^^^^     " };

  const char* const* P[10] = { STAND, LOOK_L, LOOK_R, LOOK_U, BLINK, PREEN, PECK, STEP_L, STEP_R, WING_FL };
  static const uint8_t SEQ[] = {
    0,0,4,0,1,0,2,0,
    7,8,7,8,
    0,0,3,0,4,
    5,5,0,
    6,6,0,0,
    9,9,0,4,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF);
}

// ─── BUSY ───  ~10s cycle, 6 poses + honk ticker
static void doBusy(uint32_t t) {
  static const char* const PATROL_A[5]  = { "            ", "    (>      ", " <==||==>   ", "  _(__)_    ", "   /^ ^\\    " };
  static const char* const PATROL_B[5]  = { "            ", "    (>      ", "  =-||-=    ", "  _(__)_    ", "   ^/ \\^    " };
  static const char* const CHARGE[5]    = { "            ", "      (>>   ", "    _||_    ", "   (___)    ", "   //\\\\     " };
  static const char* const PECK_DN[5]   = { "            ", "            ", "    \\v_     ", "  _(__)_    ", "   ^^^^     " };
  static const char* const HISS[5]      = { "    sss     ", "    (>>>    ", "    ||      ", "  _(__)_    ", "   ^^^^     " };
  static const char* const STOMP[5]     = { "            ", "    (>      ", "  /=||=\\    ", "  _(__)_    ", "  >^^  ^^<  " };

  const char* const* P[6] = { PATROL_A, PATROL_B, CHARGE, PECK_DN, HISS, STOMP };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 3,3, 0,1,0,1, 2,2, 4,4, 0,1, 5,5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF);

  static const char* const HONKS[] = { "h  ", "ho ", "hon", "onk", "nk!", "k! ", "!  ", "   " };
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 14);
  buddyPrint(HONKS[t % 8]);
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse + HONK overlay
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "    HONK!   ", "     (>>    ", "     ||     ", "   _(__)_   ", "    ^^^^    " };
  static const char* const SCAN_L[5]  = { "    HONK!   ", "  <<)       ", "    ||      ", "   _(__)_   ", "    ^^^^    " };
  static const char* const SCAN_R[5]  = { "    HONK!   ", "       (>>  ", "      ||    ", "   _(__)_   ", "    ^^^^    " };
  static const char* const RAISE[5]   = { "   HONK!!   ", " <===(>>===>", "     ||     ", "   _(__)_   ", "    ^^^^    " };
  static const char* const LUNGE[5]   = { "  HONK!!!   ", "       (>>> ", "      _||_  ", "    _(__)_  ", "    /^^^\\   " };
  static const char* const GLARE[5]   = { "    HONK!   ", "    (-->    ", "    ||      ", "  _(__)_    ", "  /^ ^^ ^\\  " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, RAISE, LUNGE, GLARE };
  static const uint8_t SEQ[] = {
    0,3,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 2 : -2) : ((pose == 3) ? ((t & 1) ? 1 : -1) : 0);
  buddyPrintSprite(P[pose], 5, 0, 0xFFFF, xOff);

  if ((t / 2) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER - 6, BUDDY_Y_OVERLAY);
    buddyPrint("!");
  }
  if ((t / 3) & 1) {
    buddySetColor(BUDDY_YEL);
    buddySetCursor(BUDDY_X_CENTER + 6, BUDDY_Y_OVERLAY + 4);
    buddyPrint("!");
  }
  if ((t / 4) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER + 14, BUDDY_Y_OVERLAY + 8);
    buddyPrint("!");
  }
}

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + confetti rain + HONK
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "            ", "    (>      ", "   _||_     ", "  (____)    ", "   ^^^^     " };
  static const char* const JUMP[5]    = { "  HONK!     ", "    (>>     ", " <==||==>   ", "  _(__)_    ", "   /  \\     " };
  static const char* const PEAK[5]    = { " *HONK!HONK*", "    (^^>    ", "<===||===>  ", "  _(__)_    ", "   /  \\     " };
  static const char* const SPIN_L[5]  = { "            ", "  <)        ", "<==||        ", "  _(__)_    ", "   ^^^^     " };
  static const char* const SPIN_R[5]  = { "            ", "       (>   ", "       ||==>", "  _(__)_    ", "   ^^^^     " };
  static const char* const POSE[5]    = { "    \\o/     ", "    (>>     ", " <==||==>   ", " _(____)_   ", "   /^^\\     " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-7,-3,0, 0,0,0,0, 0,-3,-7,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xFFFF);

  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_HEART, BUDDY_CYAN, BUDDY_WHITE, BUDDY_GREEN };
  for (int i = 0; i < 7; i++) {
    int phase = (t * 2 + i * 9) % 22;
    int x = BUDDY_X_CENTER - 40 + i * 12;
    int y = BUDDY_Y_OVERLAY - 6 + phase;
    if (y > BUDDY_Y_BASE + 20 || y < 0) continue;
    buddySetColor(cols[i % 5]);
    buddySetCursor(x, y);
    buddyPrint((i + (int)(t/2)) & 1 ? "*" : "o");
  }
}

// ─── DIZZY ───  ~5.6s cycle, 5 poses + orbiting stars
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]  = { "            ", "   (x       ", "   ||\\      ", "  _(__)_    ", "   v^^v     " };
  static const char* const TILT_R[5]  = { "            ", "       x)   ", "      /||   ", "    _(__)_  ", "     v^^v   " };
  static const char* const WOOZY[5]   = { "            ", "    (@      ", "    ||~     ", "  _(__)_    ", "    ~vv~    " };
  static const char* const WOOZY2[5]  = { "            ", "    @)      ", "   ~||      ", "  _(__)_    ", "    ~vv~    " };
  static const char* const STUMBLE[5] = { "            ", "    (X      ", "    /\\      ", " _(____)_   ", "  v^v  v^v  " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, STUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFFFF, X_SHIFT[beat]);

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
  buddySetColor(BUDDY_PURPLE);
  buddySetCursor(BUDDY_X_CENTER + OX[p3] - 2, BUDDY_Y_OVERLAY + 6 + OY[p3]);
  buddyPrint("+");
}

// ─── HEART ───  ~10s cycle, 5 poses + rising heart stream
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "            ", "    (^>     ", "    ||      ", "  _(__)_    ", "   ^^^^     " };
  static const char* const BLUSH[5]   = { "            ", "   #(^>#    ", "    ||      ", "  _(__)_    ", "   ^^^^     " };
  static const char* const EYES_C[5]  = { "            ", "    (<3>    ", "    ||      ", "  _(__)_    ", "   ^^^^     " };
  static const char* const TWIRL[5]   = { "            ", "    (@>     ", " <==||==>   ", "  _(__)_    ", "   ^^^^     " };
  static const char* const SIGH[5]    = { "    ~~~     ", "    (->     ", "    ||      ", "  _(__)_    ", "   ^^^^     " };

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

}  // namespace goose

extern const Species GOOSE_SPECIES = { "goose", 0xFFFF, { goose::doSleep, goose::doIdle, goose::doBusy, goose::doAttention, goose::doCelebrate, goose::doDizzy, goose::doHeart } };
