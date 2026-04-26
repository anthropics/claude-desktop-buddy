// StickS3 port main.cpp — based on plus2 stub (M5Unified migration).
// Targets M5Stack StickS3 (ESP32-S3-PICO-1-N8R8, ST7789P3 135x240, BMI270 IMU,
// M5PM1 power chip, KEY1=G11/KEY2=G12 buttons). M5Unified 0.2.14+ auto-detects
// the board and maps BtnA/BtnB to KEY1/KEY2.
// Reintroduce upstream features (clock, menu, prompts, stats, GIF chars) one
// at a time after this boots cleanly.
#include <M5Unified.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include <time.h>
#include <string.h>
#include "buddy.h"
#include "ble_bridge.h"
#include "daystats.h"
#include "character.h"
#include <M5PM1.h>

M5PM1 pm1;

// PersonaState ordering matches upstream: 0=sleep, 1=idle, 2=busy,
// 3=attention, 4=celebrate, 5=dizzy, 6=heart. Phase B only drives 1/2/3.
enum PersonaState { P_SLEEP = 0, P_IDLE = 1, P_BUSY = 2, P_ATTENTION = 3 };

const int W = 135, H = 240;
M5Canvas spr(&M5.Display);

// Offscreen buffer the buddy renders into. We then pushRotateZoom it onto
// `spr` at a fractional scale so the buddy can be sized between 1× and 2×
// (the renderer itself only supports integer text scales).
const int BUDDY_SPR_H = 170;
M5Canvas buddySpr(&spr);

// Effective buddy scale = 2 * BUDDY_ZOOM. 0.7 → 1.4×.
//
// Hard upper bound: the widest species art line is 15 chars (axolotl's
// gilled poses, post-leading/trailing-space trim). At scale s the body is
// 15*6*s = 90s pixels wide; the display is 135 wide, so s <= 1.5 to even
// fit edge-to-edge, i.e. BUDDY_ZOOM <= 0.75. 0.7 leaves ~4.5px margin each
// side so particle overlays don't kiss the bezel.
const float BUDDY_ZOOM = 0.7f;

// Region below the buddy where we draw stats (name + tokens) and the BtnA
// species-sound flash. With buddy at 2× scale (peek=false, the home-screen
// mode from upstream) it occupies y=0..~162, so we own y=170 down.
const int TEXT_TOP = 170;

// How long the species sound text replaces the stats block after BtnA.
const uint32_t SPECIES_FLASH_MS = 3000;
static uint32_t speciesFlashUntilMs = 0;

// Idle screen-off. After this many ms with no button press AND Claude not
// actively running/attention, kill the backlight. Any button or a state
// transition out of idle wakes it back up.
const uint32_t SCREEN_OFF_MS    = 5 * 60 * 1000;
const uint8_t  SCREEN_BRIGHTNESS = 180;
static uint32_t lastInteractMs = 0;
static bool     screenOff = false;

static const char* speciesSound(const char* name) {
  if (!strcmp(name, "capybara")) return "squee!";
  if (!strcmp(name, "duck"))     return "quack!";
  if (!strcmp(name, "goose"))    return "HONK!";
  if (!strcmp(name, "blob"))     return "blorp...";
  if (!strcmp(name, "cat"))      return "meow!";
  if (!strcmp(name, "dragon"))   return "ROAR!";
  if (!strcmp(name, "octopus"))  return "blub blub";
  if (!strcmp(name, "owl"))      return "hoot hoot";
  if (!strcmp(name, "penguin"))  return "noot noot!";
  if (!strcmp(name, "turtle"))   return "...slurp";
  if (!strcmp(name, "snail"))    return "...";
  if (!strcmp(name, "ghost"))    return "BOO!";
  if (!strcmp(name, "axolotl"))  return "glub glub";
  if (!strcmp(name, "cactus"))   return "ouch!";
  if (!strcmp(name, "robot"))    return "BEEP BOOP";
  if (!strcmp(name, "rabbit"))   return "boing!";
  if (!strcmp(name, "mushroom")) return "spore!";
  if (!strcmp(name, "chonk"))    return "om nom";
  return "...";
}

// Display name for the buddy. Custom kid-given names override the default
// capitalized animal name. Add entries here as more get named.
static const char* buddyDisplayName() {
  const char* species = buddySpeciesName();
  if (!strcmp(species, "cat"))  return "Bauble";
  if (!strcmp(species, "blob")) return "Gloozy";
  // Fallback: capitalize first letter of the species id.
  static char buf[16];
  snprintf(buf, sizeof(buf), "%s", species);
  if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 32);
  return buf;
}

// ---------------------------------------------------------------------------
// Idle screen-off. Backlight goes to 0 after SCREEN_OFF_MS of no interaction
// (no button + Claude not running/attention). BLE stays alive so the desktop
// keeps streaming snapshots; we just don't burn the LCD.
// ---------------------------------------------------------------------------
static void wake() {
  lastInteractMs = millis();
  if (screenOff) {
    M5.Display.setBrightness(SCREEN_BRIGHTNESS);
    screenOff = false;
    // Force a full repaint so the user sees the latest frame, not the
    // stale frame that was on screen when we dimmed.
    spr.fillSprite(0x0000);
    buddyInvalidate();
    Serial.println("[power] wake");
  }
}

static void maybeSleep() {
  if (screenOff) return;
  if (millis() - lastInteractMs >= SCREEN_OFF_MS) {
    M5.Display.setBrightness(0);
    screenOff = true;
    Serial.println("[power] screen off (idle)");
  }
}

// Advertise as "Claude-XXXX" (last two BT MAC bytes) so multiple sticks in
// one room are distinguishable in the desktop picker. Matches upstream.
static char btName[16] = "Claude";
static void startBt() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_BT);
  snprintf(btName, sizeof(btName), "Claude-%02X%02X", mac[4], mac[5]);
  bleInit(btName);
}

// Full-screen takeover while the 6-digit passkey is on screen. Mirrors
// upstream drawPasskey() but uses fixed colors since the Palette/character
// system isn't wired in this stub.
static void drawPasskey(uint32_t pk) {
  spr.fillSprite(0x0000);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(1);
  spr.setTextColor(0x8410, 0x0000);
  spr.drawString("BLUETOOTH PAIRING", W / 2, 56);
  spr.setTextSize(3);
  spr.setTextColor(0xFFFF, 0x0000);
  char b[8]; snprintf(b, sizeof(b), "%06lu", (unsigned long)pk);
  spr.drawString(b, W / 2, 120);
  spr.setTextSize(1);
  spr.setTextColor(0x8410, 0x0000);
  spr.drawString("enter on desktop", W / 2, 184);
  spr.setTextDatum(TL_DATUM);
}

// ---------------------------------------------------------------------------
// Heartbeat reader. The desktop sends one JSON object per line, terminated
// by '\n'. We accumulate bytes until we see '\n', parse with ArduinoJson,
// and pull the few fields we use directly. Deliberately not porting
// upstream's TamaState struct yet — it brings xfer/stats deps that aren't
// useful until much later phases. Add fields here as phases need them.
// ---------------------------------------------------------------------------
static const size_t LINE_CAP = 1024;
static char     lineBuf[LINE_CAP];
static size_t   lineLen = 0;
static uint8_t  sessionsRunning = 0;
static uint8_t  sessionsWaiting = 0;
static uint32_t lastSnapshotMs = 0;
static char     promptId[40]    = {0};   // pending permission request id
static char     promptTool[24]  = {0};   // tool name (Bash, Edit, etc.)

static void applyJsonLine(const char* line) {
  JsonDocument doc;
  if (deserializeJson(doc, line)) {
    Serial.printf("[ble] bad json: %.80s\n", line);
    return;
  }

  // {"time":[epoch_sec, tz_offset_sec]} — one-shot from the desktop on
  // (re)connect. Adjust by tz_offset and use gmtime_r so the broken-down
  // components are local time.
  JsonArray t = doc["time"];
  if (!t.isNull() && t.size() == 2) {
    time_t local = (time_t)t[0].as<uint32_t>() + (int32_t)t[1].as<int32_t>();
    struct tm lt;
    gmtime_r(&local, &lt);
    m5::rtc_datetime_t dt(lt);
    M5.Rtc.setDateTime(dt);
    Serial.printf("[time] sync %04d-%02d-%02d %02d:%02d:%02d\n",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  lt.tm_hour, lt.tm_min, lt.tm_sec);
    return;
  }

  // Heartbeat snapshot. Anything with `running`/`total` is a snapshot;
  // acks and other one-shots aren't.
  bool isSnapshot = doc["running"].is<int>() || doc["total"].is<int>();
  if (isSnapshot) {
    sessionsRunning = doc["running"] | 0;
    sessionsWaiting = doc["waiting"] | 0;
    lastSnapshotMs  = millis();

    if (doc["tokens"].is<uint32_t>()) {
      daystatsOnBridgeTokens(doc["tokens"].as<uint32_t>());
    }
    if (doc["tokens_today"].is<uint32_t>()) {
      daystatsOnTokensToday(doc["tokens_today"].as<uint32_t>());
    }

    // Permission prompt — desktop tells us there's a pending approval.
    JsonObject pr = doc["prompt"];
    if (!pr.isNull()) {
      const char* pid = pr["id"];
      const char* ptl = pr["tool"];
      strncpy(promptId,   pid ? pid : "", sizeof(promptId)-1);   promptId[sizeof(promptId)-1]   = 0;
      strncpy(promptTool, ptl ? ptl : "", sizeof(promptTool)-1); promptTool[sizeof(promptTool)-1] = 0;
    } else {
      promptId[0] = 0; promptTool[0] = 0;
    }
  }
}

// Send a JSON command line back to the desktop over BLE NUS TX.
static void sendCmd(const char* json) {
  size_t n = strlen(json);
  bleWrite((const uint8_t*)json, n);
  bleWrite((const uint8_t*)"\n", 1);
  Serial.printf("[ble] tx %s\n", json);
}

static void pollBle() {
  while (bleAvailable()) {
    int b = bleRead();
    if (b < 0) break;
    if (b == '\r') continue;
    if (b == '\n') {
      lineBuf[lineLen] = 0;
      if (lineLen) applyJsonLine(lineBuf);
      lineLen = 0;
      continue;
    }
    if (lineLen + 1 >= LINE_CAP) {
      // Overflow — drop the partial line; desktop will resend on next snapshot.
      lineLen = 0;
      Serial.println("[ble] line overflow, dropped");
      continue;
    }
    lineBuf[lineLen++] = (char)b;
  }
}

// Heartbeat is "fresh" if we got a snapshot in the last 30s (matches the
// REFERENCE.md liveness window). Stale → treat as offline.
static bool snapshotFresh() {
  return lastSnapshotMs != 0 && (millis() - lastSnapshotMs) <= 30000;
}

static uint8_t personaFromState() {
  if (!snapshotFresh())   return P_IDLE;     // offline → idle, never sad
  if (sessionsWaiting > 0) return P_ATTENTION;
  if (sessionsRunning > 0) return P_BUSY;
  return P_IDLE;
}

// Default render of the y>=170 region: buddy display name big, then a rule,
// then tokens today + lifetime tokens. Anti-streak / anti-FOMO per the plan:
// no goal markers, no comparisons, no day-of-week.
//
// Note: delegation seconds are still tracked (Phase D will use them to scale
// idle-animation amplitude) — they just don't get a numeric display anymore.
static void drawStats() {
  spr.fillRect(0, TEXT_TOP, W, H - TEXT_TOP, 0x0000);

  char buf[24];

  // Buddy name, large and centered. Replaces the elapsed-time display.
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(2);
  spr.setTextColor(0xFFFF, 0x0000);
  spr.drawString(buddyDisplayName(), W / 2, 184);

  // Thin rule under the name.
  spr.drawFastHLine(20, 200, W - 40, 0x4208);

  // Tokens today + lifetime. Until the first heartbeat lands, show a
  // placeholder rather than "0 today" — at boot/reconnect the desktop's
  // keepalive can take ~10s, and "0" reads as "I worked nothing today"
  // when the truth is "I haven't heard from the desktop yet."
  spr.setTextSize(1);
  char line[32];
  if (daystatsTokensTodaySeen()) {
    daystatsFmtTokens(buf, sizeof(buf), daystatsTokensToday());
    snprintf(line, sizeof(line), "%s today", buf);
  } else {
    snprintf(line, sizeof(line), "-- today");
  }
  spr.setTextColor(0x07FF, 0x0000);              // cyan
  spr.drawString(line, W / 2, 212);

  // Lifetime is the device's own persisted counter — even pre-heartbeat
  // we know it from NVS, so show the real value (only "today" gates on
  // having actually heard from the desktop this session).
  daystatsFmtTokens(buf, sizeof(buf), daystatsLifetimeTokens());
  snprintf(line, sizeof(line), "%s lifetime", buf);
  spr.setTextColor(0x8410, 0x0000);
  spr.drawString(line, W / 2, 226);

  spr.setTextDatum(TL_DATUM);
}

// 3-second overlay on BtnA: replaces the stats block with the species
// sound text, then fades back to stats.
static void drawSpeciesFlash() {
  spr.fillRect(0, TEXT_TOP, W, H - TEXT_TOP, 0x0000);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(2);
  spr.setTextColor(0x07FF, 0x0000);
  // Center vertically in the (now smaller) text region: midpoint of 170..240.
  spr.drawString(speciesSound(buddySpeciesName()), W / 2, 205);
  spr.setTextDatum(TL_DATUM);
}

// Battery indicator at top-right corner. Color-coded:
//   cyan = charging, green = >50%, yellow = 20-50%, red = <20%
// Drawn after buddy composite so it survives the per-frame wipe.
static void drawBattery() {
  uint16_t mv = 0;
  pm1.readVbat(&mv);
  int pct = (mv - 3300) * 100 / (4200 - 3300);
  pct = max(0, min(100, pct));
  bool chg = M5.Power.isCharging();
  char buf[12];
  snprintf(buf, sizeof(buf), "%d%%%s", pct, chg ? "+" : "");
  uint16_t col = chg     ? 0x07FF :        // cyan
                 pct < 20 ? 0xF800 :        // red
                 pct < 50 ? 0xFFE0 :        // yellow
                            0x07E0;         // green
  spr.fillRect(W - 36, 0, 36, 10, 0x0000);
  spr.setTextDatum(TR_DATUM);
  spr.setTextSize(1);
  spr.setTextColor(col, 0x0000);
  spr.drawString(buf, W - 2, 1);
  spr.setTextDatum(TL_DATUM);
}

static void drawTextRegion() {
  if (speciesFlashUntilMs && (int32_t)(millis() - speciesFlashUntilMs) < 0) {
    drawSpeciesFlash();
  } else {
    speciesFlashUntilMs = 0;
    drawStats();
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  pm1.begin(&M5.In_I2C);
  // M5Unified does NOT auto-init Serial (legacy M5StickCPlus did). Without
  // this, every Serial.print in ble_bridge silently no-ops.
  Serial.begin(115200);
  M5.Display.setRotation(0);
  M5.Display.setBrightness(180);

  spr.createSprite(W, H);
  // Buddy renders to its own offscreen sprite at scale=2; we then
  // pushRotateZoom it onto `spr` at BUDDY_ZOOM for the final size.
  buddySpr.setColorDepth(16);
  buddySpr.createSprite(W, BUDDY_SPR_H);
  buddySpr.fillSprite(0x0000);
  // Disable text auto-wrap. M5GFX defaults to _textwrap_x=true, which means
  // print() wraps to the next line when a glyph crosses the right edge —
  // that's what was causing the busy-state dot ticker (which draws past
  // x=135 at internal scale 2) to wrap to the next y. With wrap off, the
  // overflowing pixels just clip silently, which is what we want.
  buddySpr.setTextWrap(false, false);
  // Pivot ≈ buddy art's geometric center at 2× scale (BUDDY_X_CENTER=67,
  // body roughly y=30..130). Used as the rotate/zoom anchor.
  buddySpr.setPivot(W / 2, 80);
  buddySetRenderTarget(&buddySpr);

  buddyInit();
  buddySetSpeciesIdx(0);
  // peek=false → internal scale=2. The fractional shrink happens at push
  // time via pushRotateZoom(BUDDY_ZOOM).
  buddySetPeek(false);

  // GIF character disabled — ASCII buddies render cleaner without flicker
  // and let BtnA cycle through 18 species. Re-enable by uncommenting:
  // characterInit("bufo");

  daystatsInit();
  startBt();
  lastInteractMs = millis();

  spr.fillSprite(0x0000);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0xFFFF, 0x0000);
  spr.setTextSize(2);
  spr.drawString("Hello!", W / 2, H / 2 - 12);
  spr.setTextSize(1);
  spr.setTextColor(0x8410, 0x0000);
  spr.drawString("a buddy appears", W / 2, H / 2 + 12);
  spr.setTextDatum(TL_DATUM);
  spr.pushSprite(0, 0);
  delay(1500);
  spr.fillSprite(0x0000);   // splash → blank, then let buddyTick paint in
  drawTextRegion();
}

void loop() {
  M5.update();

  static bool wasShowingPasskey = false;
  uint32_t pk = blePasskey();
  if (pk) {
    // While pairing: passkey takes the whole screen. BtnA cycling is
    // suppressed so the user isn't fighting the UI mid-pair.
    drawPasskey(pk);
    spr.pushSprite(0, 0);
    wasShowingPasskey = true;
    delay(33);
    return;
  }
  if (wasShowingPasskey) {
    // First tick after passkey clears: wipe and force buddy to repaint
    // everything (buddyTick only clears its own region).
    spr.fillSprite(0x0000);
    drawTextRegion();
    buddyInvalidate();
    wasShowingPasskey = false;
  }

  pollBle();
  daystatsTick(sessionsRunning, snapshotFresh());

  // Snapshot before wake() so we can tell whether *this* press was a
  // wake-from-off — we want to swallow species-cycling on that press so
  // it acts purely as "wake," not "wake AND advance species."
  bool wasScreenOff = screenOff;
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
    wake();
  }
  bool inPrompt = promptId[0] != 0;
  if (M5.BtnA.wasPressed() && !wasScreenOff) {
    if (inPrompt) {
      // Approve: BtnA = "allow once" decision
      char cmd[96];
      snprintf(cmd, sizeof(cmd), "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"once\"}", promptId);
      sendCmd(cmd);
      promptId[0] = 0;
      promptTool[0] = 0;
    } else {
      buddyNextSpecies();     // wraps around at the end of the species table
      speciesFlashUntilMs = millis() + SPECIES_FLASH_MS;
      drawTextRegion();
    }
  }
  if (M5.BtnB.wasPressed() && !wasScreenOff && inPrompt) {
    // Deny: BtnB
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"deny\"}", promptId);
    sendCmd(cmd);
    promptId[0] = 0;
    promptTool[0] = 0;
  }

  // The stats block changes infrequently (delegation seconds tick once per
  // second; tokens only on snapshots) but the species flash auto-expires.
  // Re-render every loop is cheap (sprite is offscreen) and keeps the
  // expiration handling simple.
  drawTextRegion();

  // Persona logged on change so we can see state transitions on serial
  // without spamming a line every tick.
  static uint8_t lastPersona = 0xFF;
  uint8_t persona = personaFromState();
  if (persona != lastPersona) {
    Serial.printf("[state] %s (running=%u waiting=%u fresh=%d)\n",
                  persona == P_BUSY ? "busy" :
                  persona == P_ATTENTION ? "attention" : "idle",
                  sessionsRunning, sessionsWaiting, snapshotFresh());
    // Non-idle transitions are interesting → wake the screen so the user
    // sees the buddy come to life. Going-to-idle does NOT wake.
    if (persona != P_IDLE) wake();
    lastPersona = persona;
  }

  // Idle long enough → backlight off. Ticked after the persona check so
  // an attention/busy transition wins over the timeout in the same loop.
  maybeSleep();

  // Character vs buddy: if a GIF character pack is loaded, render that;
  // otherwise fall back to ASCII buddy.
  if (characterLoaded()) {
    static uint8_t lastCharState = 0xFF;
    if (persona != lastCharState) {
      characterSetState(persona);
      lastCharState = persona;
    }
    // Wipe buddy region first; characterTick draws into spr (default target)
    // at full size when peek mode is off (default after characterInit).
    spr.fillRect(0, 0, W, TEXT_TOP, 0x0000);
    characterTick();
  } else {
    // buddyTick is internally throttled to 5fps and clears its own region.
    // It now writes into buddySpr (not spr) thanks to buddySetRenderTarget.
    buddyTick(persona);
    // Wipe spr's buddy region so the previous-frame zoom result doesn't ghost
    // through the transparent push, then composite the buddy at fractional
    // scale. BUDDY_BG (black) is keyed transparent so only ink lands on spr.
    spr.fillRect(0, 0, W, TEXT_TOP, 0x0000);
    buddySpr.pushRotateZoom(&spr, W / 2, 80, 0.0f, BUDDY_ZOOM, BUDDY_ZOOM, (uint16_t)0x0000);
  }
  drawBattery();
  spr.pushSprite(0, 0);
  delay(33);
}
