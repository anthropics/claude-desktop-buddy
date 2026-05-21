#include "../buddy.h"
#include "../buddy_common.h"
#include <M5Unified.h>
#include <string.h>

extern M5Canvas spr;

namespace penguin {

// ─── SLEEP ───  ~12s cycle, 6 poses, curled on ice
static void doSleep(uint32_t t) {
  static const char* const TUCK[5]    = { "            ", "   .---.    ", "  ( -- )    ", "  (_____)   ", "   ~~~~~    " };
  static const char* const BREATHE[5] = { "            ", "   .---.    ", "  ( -- )    ", "  (_____)   ", "   =====    " };
  static const char* const SNORE[5]   = { "    o O .   ", "   .---.    ", "  ( __ )    ", "  (_____)   ", "   =====    " };
  static const char* const TIPPED[5]  = { "            ", "            ", "  .-----.   ", " ( --   )=> ", "  `~~~~~`   " };
  static const char* const TIPPED2[5] = { "            ", "            ", "  .-----.   ", " ( zz   )=> ", "  `~~~~~`   " };
  static const char* const TWITCH[5]  = { "            ", "   .---.    ", "  ( ^^ )    ", " /(_____)   ", "   ~~~~~    " };

  const char* const* P[6] = { TUCK, BREATHE, SNORE, TIPPED, TIPPED2, TWITCH };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,2,1,
    0,1,0,1,
    3,4,3,4,3,4,
    3,3,
    1,5,1,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0x041F);

  // Z particles drift up-right with cold-blue tint
  int p1 = (t)     % 10;
  int p2 = (t + 4) % 10;
  int p3 = (t + 7) % 10;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 20 + p1, BUDDY_Y_OVERLAY + 18 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 26 + p2, BUDDY_Y_OVERLAY + 14 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 16 + p3 / 2, BUDDY_Y_OVERLAY + 10 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses of waddling, formal posture
static void doIdle(uint32_t t) {
  static const char* const STAND[5]   = { "   .---.    ", "  ( o>o )   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const WAD_L[5]   = { "   .---.    ", "  ( o>o )   ", "/(     )    ", "  `-----`   ", "  J    L    " };
  static const char* const WAD_R[5]   = { "   .---.    ", "  ( o>o )   ", " (     )\\   ", "  `-----`   ", "   J    L   " };
  static const char* const BLINK[5]   = { "   .---.    ", "  ( ->- )   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const LOOK_L[5]  = { "   .---.    ", "  (o> o )   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const LOOK_R[5]  = { "   .---.    ", "  ( o >o)   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const PREEN[5]   = { "   .---.    ", "  ( o>o )   ", " /(  v  )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const FLAP[5]    = { "  \\.---./   ", "  ( o>o )   ", "/(     )\\   ", "  `-----`   ", "   J   L    " };
  static const char* const BOW[5]     = { "            ", "   .---.    ", "  ( v>v )   ", " /(_____)\\  ", "   J   L    " };
  static const char* const STRETCH[5] = { "  /.---.\\   ", " ( ^>^ )    ", "//(     )\\\\ ", "  `-----`   ", "   J   L    " };

  const char* const* P[10] = { STAND, WAD_L, WAD_R, BLINK, LOOK_L, LOOK_R, PREEN, FLAP, BOW, STRETCH };
  static const uint8_t SEQ[] = {
    0,0,1,2,1,2,0,3,
    0,4,0,5,0,
    6,6,0,3,
    0,1,2,1,2,0,
    7,7,0,0,
    8,8,0,
    9,9,0,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0x041F);
}

// ─── BUSY ───  ~10s cycle, 6 poses + dot ticker, formal flipper-typing
static void doBusy(uint32_t t) {
  static const char* const TYPE_A[5]  = { "   .---.    ", "  ( v>v )   ", " /(     )\\  ", " /`-----`\\  ", "   J   L    " };
  static const char* const TYPE_B[5]  = { "   .---.    ", "  ( v>v )   ", " \\(     )/  ", " \\`-----`/  ", "   J   L    " };
  static const char* const THINK[5]   = { "      ?     ", "   .---.    ", "  ( ^>^ )   ", " /(  .  )\\  ", "   J   L    " };
  static const char* const SIP[5]     = { "    [_]     ", "   .---.|   ", "  ( o>o |   ", " /(     )\\  ", "   J   L    " };
  static const char* const EUREKA[5]  = { "      *     ", "   .---.    ", "  ( O>O )   ", " /(  ^  )\\  ", "   J   L    " };
  static const char* const RELIEF[5]  = { "    ~~~     ", "   .---.    ", "  ( ->- )   ", " /(  _  )\\  ", "   J   L    " };

  const char* const* P[6] = { TYPE_A, TYPE_B, THINK, SIP, EUREKA, RELIEF };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0x041F);

  static const char* const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 14);
  buddyPrint(DOTS[t % 6]);
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse, alert flippers up
static void doAttention(uint32_t t) {
  static const char* const ALERT[5]   = { "   .---.    ", "  ( O>O )   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const SCAN_L[5]  = { "   .---.    ", "  (O> O )   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const SCAN_R[5]  = { "   .---.    ", "  ( O >O)   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const SCAN_U[5]  = { "   .---.    ", "  ( ^>^ )   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const TENSE[5]   = { "  /.---.\\   ", " /( O>O )\\  ", "//(     )\\\\ ", "  `-----`   ", "  J     L   " };
  static const char* const HUSH[5]    = { "   .---.    ", "  ( o>o )   ", " /(  .  )\\  ", "  `-----`   ", "   J   L    " };

  const char* const* P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, TENSE, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  buddyPrintSprite(P[pose], 5, 0, 0x041F, xOff);

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

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + confetti rain, jumping penguin
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]  = { "            ", "   .---.    ", "  ( ^>^ )   ", " /(_____)\\  ", "   J   L    " };
  static const char* const JUMP[5]    = { "  \\.---./   ", "  ( ^>^ )   ", " /(     )\\  ", "  `-----`   ", "   ^   ^    " };
  static const char* const PEAK[5]    = { "  \\^---^/   ", "  ( O>O )   ", " /(  W  )\\  ", "  `-----`   ", "   v   v    " };
  static const char* const SPIN_L[5]  = { "   .---.    ", "  ( <>< )   ", "/(     )    ", "  `-----`   ", "   /   \\    " };
  static const char* const SPIN_R[5]  = { "   .---.    ", "  (>< ><)   ", " (     )\\   ", "  `-----`   ", "   \\   /    " };
  static const char* const POSE[5]    = { "    \\_/     ", "   .---.    ", "  ( ^>^ )   ", "/(  W  )\\   ", "   J   L    " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0x041F);

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

// ─── DIZZY ───  ~5.6s cycle, 5 poses + orbiting stars, slipping on ice
static void doDizzy(uint32_t t) {
  static const char* const SLIP_L[5]  = { "  .---.     ", " ( @>@ )    ", "/(     )    ", " `-----`    ", "  J   L     " };
  static const char* const SLIP_R[5]  = { "    .---.   ", "   ( @>@ )  ", "   (     )\\ ", "    `-----` ", "     J   L  " };
  static const char* const WOOZY[5]   = { "   .---.    ", "  ( x>@ )   ", " /(  ~  )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const WOOZY2[5]  = { "   .---.    ", "  ( @>x )   ", " /(  ~  )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const SPLAT[5]   = { "            ", "   .---.    ", "  ( @>@ )   ", " (_______)= ", "   ~~~~~    " };

  const char* const* P[5] = { SLIP_L, SLIP_R, WOOZY, WOOZY2, SPLAT };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0x041F, X_SHIFT[beat]);

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

// ─── HEART ───  ~10s cycle, 5 poses + rising heart stream, dreamy penguin
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]  = { "   .---.    ", "  ( ^>^ )   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const BLUSH[5]   = { "   .---.    ", "  (#^>^#)   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const EYES_C[5]  = { "   .---.    ", "  (<3><3)   ", " /(     )\\  ", "  `-----`   ", "   J   L    " };
  static const char* const TWIRL[5]   = { "   .---.    ", "  ( @>@ )   ", "/(     )\\   ", "  `-----`   ", "    \\ /     " };
  static const char* const SIGH[5]    = { "   .---.    ", "  ( ->- )   ", " /(  ^  )\\  ", "  `-----`   ", "   J   L    " };

  const char* const* P[5] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0x041F);

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

}  // namespace penguin

extern const Species PENGUIN_SPECIES = {
  "penguin",
  0x041F,
  { penguin::doSleep, penguin::doIdle, penguin::doBusy, penguin::doAttention,
    penguin::doCelebrate, penguin::doDizzy, penguin::doHeart }
};
