// Claude Hardware Buddy — M5Stack Core Basic firmware
//
// Phase 1: BLE peripheral (Nordic UART Service) + 320x240 live status display.
// Receives compact status JSON from the Python bridge and renders session counts.
//
// Phase 2: 3-button input for permission approval (A=allow, C=deny, B=auto-approve
// toggle), permission-takeover screen, TX-notify decision path back to bridge,
// and auto-approve mode with banner + beep.
//
// Privacy: the bridge sends ONLY counts, a short status string, and tool call
// metadata (command / path / URL). File contents, diff bodies, and conversation
// text are NEVER sent to this device.

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <M5Unified.h>
#include <ArduinoJson.h>

#include "buddy_coolS.h"
#include "buddy_palette.h"
#include "buddy_state.h"

// Nordic UART Service — must match the bridge / REFERENCE.md exactly.
#define NUS_SERVICE "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // central writes here
#define NUS_TX      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // device notifies here

static BLECharacteristic* txChar = nullptr;
static volatile bool centralConnected = false;

// Pending permission prompt state (empty id == no prompt).
static char promptId[48]      = {0};
static char promptTool[24]    = {0};
static char promptDetail[200] = {0};
static char promptChange[40]  = {0};
static bool autoApprove       = false;
// Set true when a central connects so loop() re-syncs the auto state once.
static volatile bool pendingAutoSync = false;

// Last-known status (for repaint after prompt clears or toast expires).
static int lastRunning = 0, lastWaiting = 0, lastTotal = 0;
static char lastStatusMsg[64] = "idle";
static int autoCount = 0;
static unsigned long autoFlashUntil = 0;  // millis() deadline for green toast

static M5GFX_Sprite_t buddySprite(&M5.Display);
static BuddyDepth buddyDepth = DEPTH_8;
static bool buddyReady = false;

// New volatile cross-task flags.
static volatile bool          pairing      = false;
static volatile unsigned long heartUntil   = 0;
static volatile uint32_t      passkeyVal   = 0;

// Debug cycle (home screen only).
static bool         debugActive = false;
static PersonaState debugState  = PS_SLEEP;
static unsigned long lastFrameMs = 0;

// Spinlock guarding lastStatusMsg only (see spec "Cross-task synchronization").
static portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;

// ----- AskUserQuestion screen state -----
static char askId[48] = {0};                 // empty == no ask in flight
static bool askMultiSelect = false;
static int  askQCount = 0;                   // number of questions, 1..4
static int  askCurQ   = 0;                   // 0..askQCount-1
static int  askPage   = 0;                   // 0 for 2-3 opts; 0/1 for 4 opts

struct AskOption {
  char label[28];
  char desc[40];
};
struct AskQuestion {
  char text[64];
  AskOption opts[4];
  int        optCount;
  uint8_t    selected;                       // bitmask for multi-select
  int        single;                         // -1 or index for single-select
};
static AskQuestion askQs[4];

static unsigned long btnAPressMs = 0;        // long-press tracking
static unsigned long btnBPressMs = 0;
static const unsigned long LONG_PRESS_MS = 800;

// Forward declarations for functions called from RxCallbacks (defined later).
static void sendNotify(const char* json);
static void sendDecision(const char* decision);
static void toggleAuto();
static void renderAsk();
static void askSendAnswers();
static void askCancel();
static void askAdvance();

// Permission-takeover screen: navy background, tool + detail + optional change
// size (e.g. "+3/-1 lines" for Edit/Write), and button hints.
static void renderPrompt(const char* tool, const char* detail,
                         const char* change) {
  M5.Display.fillScreen(TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(8, 8);
  M5.Display.printf("Approve %s?", tool);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(8, 50);
  M5.Display.setTextWrap(true);
  M5.Display.print(detail);              // full tool call; no file contents
  if (change && change[0] != '\0') {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_NAVY);
    M5.Display.setCursor(8, 140);
    M5.Display.print(change);
  }
  M5.Display.setTextColor(TFT_GREEN, TFT_NAVY);
  M5.Display.setCursor(8, 210);
  M5.Display.print("[A] Allow");
  M5.Display.setTextColor(TFT_RED, TFT_NAVY);
  M5.Display.setCursor(180, 210);
  M5.Display.print("[C] Deny");
}

static void renderPasskey(uint32_t pk) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(8, 8);  M5.Display.print("Pair this code:");
  M5.Display.setTextSize(4);
  M5.Display.setCursor(8, 60); M5.Display.printf("%06u", pk);
}

static void renderAsk() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);

  // Header: question text (wrapped) and Q n/m chip.
  M5.Display.setCursor(8, 8);
  M5.Display.setTextWrap(true);
  M5.Display.print(askQs[askCurQ].text);
  if (askQCount > 1) {
    char chip[10];
    snprintf(chip, sizeof(chip), "Q %d/%d", askCurQ + 1, askQCount);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(264, 8);
    M5.Display.print(chip);
  }

  // Body: A/B/C rows. For 4 options, paged.
  const AskQuestion& q = askQs[askCurQ];
  int rows[3] = {-1, -1, -1};                // which option each row shows
  bool moreRow = false;                      // C = "More >>" / "<< Back"
  if (q.optCount <= 2) {
    rows[0] = 0; rows[2] = q.optCount > 1 ? 1 : -1;   // A and C only
  } else if (q.optCount == 3) {
    rows[0] = 0; rows[1] = 1; rows[2] = 2;
  } else {
    if (askPage == 0) { rows[0] = 0; rows[1] = 1; moreRow = true; }
    else              { rows[0] = 2; rows[1] = 3; moreRow = true; }
  }

  const int rowYs[3] = {64, 116, 168};
  const char letters[3] = {'A', 'B', 'C'};
  const uint16_t tints[3] = {TFT_DARKGREEN, TFT_BLACK, TFT_MAROON};
  for (int r = 0; r < 3; ++r) {
    int oi = rows[r];
    if (r == 2 && moreRow) {
      M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
      M5.Display.setTextSize(2);
      M5.Display.setCursor(8, rowYs[r] + 6);
      M5.Display.print(askPage == 0 ? "C: More >>" : "C: << Back");
      continue;
    }
    if (oi < 0) continue;
    // Tint background strip so A=greenish, C=redish (spatial habit).
    if (tints[r] != TFT_BLACK) {
      M5.Display.fillRect(0, rowYs[r] - 4, 320, 44, tints[r]);
      M5.Display.setTextColor(TFT_WHITE, tints[r]);
    } else {
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    M5.Display.setTextSize(2);
    M5.Display.setCursor(8, rowYs[r]);
    const char* sel = "";
    if (askMultiSelect && (q.selected & (1 << oi))) sel = "[x] ";
    M5.Display.printf("%c: %s%s", letters[r], sel, q.opts[oi].label);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(28, rowYs[r] + 18);
    M5.Display.print(q.opts[oi].desc);
  }

  if (askMultiSelect) {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(8, 228);
    M5.Display.print("B-long = submit  |  A-long = answer on laptop");
  } else {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(8, 228);
    M5.Display.print("A-long = answer on laptop");
  }
}

static void askClear() {
  askId[0] = 0;
  askQCount = 0;
  askCurQ = askPage = 0;
  askMultiSelect = false;
  for (int i = 0; i < 4; ++i) {
    askQs[i].selected = 0;
    askQs[i].single = -1;
  }
}

static void askSendAnswers() {
  // Build {"cmd":"ask_answer","id":"<id>","answers":[ ... ]} with ArduinoJson
  // so option labels containing quotes/backslashes are correctly escaped.
  JsonDocument doc;
  doc["cmd"] = "ask_answer";
  doc["id"] = askId;
  JsonArray answers = doc["answers"].to<JsonArray>();
  for (int i = 0; i < askQCount; ++i) {
    JsonObject a = answers.add<JsonObject>();
    if (askMultiSelect) {
      JsonArray labels = a["labels"].to<JsonArray>();
      for (int o = 0; o < askQs[i].optCount; ++o) {
        if (askQs[i].selected & (1 << o)) labels.add(askQs[i].opts[o].label);
      }
    } else {
      int oi = askQs[i].single;
      a["label"] = (oi >= 0 && oi < askQs[i].optCount)
                   ? askQs[i].opts[oi].label : "";
    }
  }
  char buf[1536];  // headroom for fully-escaped labels (4 questions x 4 opts)
  size_t len = serializeJson(doc, buf, sizeof(buf) - 2);
  buf[len++] = '\n';
  buf[len] = '\0';
  sendNotify(buf);

  // "Sent ✓" splash for 1s
  M5.Display.fillScreen(TFT_DARKGREEN);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(80, 100);
  M5.Display.print("Sent");
  delay(1000);

  askClear();    // loop repaints the buddy
}

static void askCancel() {
  char buf[80];
  snprintf(buf, sizeof(buf), "{\"cmd\":\"ask_cancel\",\"id\":\"%s\"}\n", askId);
  sendNotify(buf);
  askClear();    // loop repaints the buddy
}

static void askAdvance() {
  // Move to next question, or submit if we just answered the last one.
  if (askCurQ + 1 < askQCount) {
    askCurQ++;
    askPage = 0;
    renderAsk();
  } else {
    askSendAnswers();
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    centralConnected = true;
    pendingAutoSync  = true;   // loop() will notify the bridge of current state
    Serial.println("[ble] central connected");
  }
  void onDisconnect(BLEServer* s) override {
    centralConnected = false;
    Serial.println("[ble] central disconnected -- re-advertising");
    s->getAdvertising()->start();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  std::string rxBuf_;  // reassembly buffer for fragmented BLE writes

  void onWrite(BLECharacteristic* c) override {
    std::string v = c->getValue();
    if (v.empty()) return;
    rxBuf_ += v;
    if (rxBuf_.size() > 8192) { rxBuf_.clear(); return; }  // overflow guard
    // The bridge chunks large writes; every message is newline-terminated.
    // Process each complete line; keep any trailing partial in the buffer.
    size_t nl;
    while ((nl = rxBuf_.find('\n')) != std::string::npos) {
      std::string line = rxBuf_.substr(0, nl);
      rxBuf_.erase(0, nl + 1);
      if (!line.empty()) processLine(line);
    }
  }

  void processLine(const std::string& v) {
    Serial.print("[rx] ");
    Serial.write(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    Serial.println();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, v);
    if (err) return;
    if (doc["evt"] == "status") {
      lastRunning = doc["running"] | 0;
      lastWaiting = doc["waiting"] | 0;
      lastTotal   = doc["total"]   | 0;
      portENTER_CRITICAL(&statusMux);
      strlcpy(lastStatusMsg, doc["msg"] | "", sizeof(lastStatusMsg));
      portEXIT_CRITICAL(&statusMux);
      // loop() repaints; no draw here.
    } else if (doc["evt"] == "prompt") {
      const char* incomingId = doc["id"] | "";
      if (promptId[0] != 0 && strcmp(incomingId, promptId) != 0) {
        // Already showing a different prompt — tell the bridge to fall back.
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "{\"cmd\":\"prompt_busy\",\"id\":\"%s\"}\n", incomingId);
        sendNotify(buf);
        return;
      }
      strlcpy(promptId,     doc["id"]     | "", sizeof(promptId));
      strlcpy(promptTool,   doc["tool"]   | "", sizeof(promptTool));
      strlcpy(promptDetail, doc["detail"] | "", sizeof(promptDetail));
      strlcpy(promptChange, doc["change"] | "", sizeof(promptChange));
      if (autoApprove) { sendDecision("allow"); }
      else             { renderPrompt(promptTool, promptDetail, promptChange); }
    } else if (doc["cmd"] == "prompt_cancel") {
      if (strcmp(doc["id"] | "", promptId) == 0) promptId[0] = 0;  // loop repaints
    } else if (doc["cmd"] == "get_auto") {
      char buf[40];
      snprintf(buf, sizeof(buf), "{\"cmd\":\"auto\",\"state\":%s}\n",
               autoApprove ? "true" : "false");
      sendNotify(buf);
    } else if (doc["evt"] == "auto_fired") {
      autoCount++;
      autoFlashUntil = millis() + 1500;
      strlcpy(promptTool, doc["tool"] | "", sizeof(promptTool)); // reuse as toast tool
      // No draw; overlay reads autoCount/autoFlashUntil/autoApprove each frame.
    }
    else if (doc["evt"] == "ask") {
      // Reject if a prompt or a different ask is already on screen.
      const char* incomingId = doc["id"] | "";
      if (promptId[0] != 0 || (askId[0] != 0 && strcmp(incomingId, askId) != 0)) {
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "{\"cmd\":\"prompt_busy\",\"id\":\"%s\"}\n", incomingId);
        sendNotify(buf);
        return;
      }
      askClear();
      strlcpy(askId, incomingId, sizeof(askId));
      askMultiSelect = doc["multiSelect"] | false;
      JsonArray qs = doc["questions"].as<JsonArray>();
      askQCount = 0;
      for (JsonObject q : qs) {
        if (askQCount >= 4) break;
        AskQuestion& aq = askQs[askQCount];
        strlcpy(aq.text, q["text"] | "", sizeof(aq.text));
        aq.optCount = 0;
        aq.selected = 0;
        aq.single = -1;
        JsonArray opts = q["options"].as<JsonArray>();
        for (JsonObject op : opts) {
          if (aq.optCount >= 4) break;
          strlcpy(aq.opts[aq.optCount].label, op["label"] | "",
                  sizeof(aq.opts[0].label));
          strlcpy(aq.opts[aq.optCount].desc,  op["desc"]  | "",
                  sizeof(aq.opts[0].desc));
          aq.optCount++;
        }
        askQCount++;
      }
      askCurQ = askPage = 0;
      renderAsk();
    }
    else if (doc["cmd"] == "ask_cancel") {
      if (strcmp(doc["id"] | "", askId) == 0) askClear();          // loop repaints
    }
  }
};

// DisplayOnly device: the stack picks a random passkey, we display it on
// screen and serial so the user can enter it on the paired host.
class SecurityCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t pk) override {
    Serial.printf("\n  BLE PAIRING PASSKEY: %06u\n\n", pk);
    passkeyVal = pk;
    pairing    = true;            // loop() draws the passkey screen
  }
  bool onConfirmPIN(uint32_t) override { return true; }
  bool onSecurityRequest() override { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    pairing = false;
    Serial.printf("[ble] pairing %s\n", cmpl.success ? "SUCCEEDED" : "FAILED");
  }
};

static void setupBuddySprite() {
  Serial.printf("[buddy] free heap %u, largest block %u\n",
                ESP.getFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  // Try 8bpp full-screen (~75KB) with a 20KB margin.
  buddySprite.setColorDepth(8);
  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) > (76800u + 20480u) &&
      buddySprite.createSprite(320, 240)) {
    uint16_t pal[PAL8_SIZE]; buildPalette8(pal);
    for (int i = 0; i < PAL8_SIZE; ++i) buddySprite.setPaletteColor(i, pal[i]);
    buddyDepth = DEPTH_8; buddyReady = true;
    buddyInit(DEPTH_8);
    Serial.println("[buddy] sprite 8bpp 320x240");
    return;
  }
  // Fallback: 4bpp full-screen (~37.5KB).
  buddySprite.setColorDepth(4);
  if (buddySprite.createSprite(320, 240)) {
    uint16_t pal[PAL4_SIZE]; buildPalette4(pal);
    for (int i = 0; i < PAL4_SIZE; ++i) buddySprite.setPaletteColor(i, pal[i]);
    buddyDepth = DEPTH_4; buddyReady = true;
    buddyInit(DEPTH_4);
    Serial.println("[buddy] sprite 4bpp 320x240 (fallback)");
    return;
  }
  // Last resort: buddy region only, 4bpp (x 100..220).
  buddySprite.setColorDepth(4);
  if (buddySprite.createSprite(120, 240)) {
    uint16_t pal[PAL4_SIZE]; buildPalette4(pal);
    for (int i = 0; i < PAL4_SIZE; ++i) buddySprite.setPaletteColor(i, pal[i]);
    buddyDepth = DEPTH_4; buddyReady = true;
    buddyInit(DEPTH_4);
    Serial.println("[buddy] sprite 4bpp 120x240 (region fallback)");
    return;
  }
  Serial.println("[buddy] ERROR: could not allocate sprite; buddy disabled");
  buddyReady = false;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Speaker.begin();
  M5.Display.setRotation(1);              // 320x240 landscape
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(8, 8);
  M5.Display.print("Claude Buddy");
  M5.Display.setCursor(8, 40);
  M5.Display.print("starting...");

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("[buddy] Claude Buddy firmware starting");

  BLEDevice::init("Claude-Buddy");
  BLEDevice::setMTU(517);
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* svc = server->createService(NUS_SERVICE);

  // Require an encrypted (bonded) link for all GATT access. Without these
  // per-characteristic permissions the global MITM config is not enforced —
  // a nearby unpaired central could write spoofed status to RX.
  txChar = svc->createCharacteristic(NUS_TX, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  BLE2902* cccd = new BLE2902();
  cccd->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED |
                             ESP_GATT_PERM_WRITE_ENCRYPTED);
  txChar->addDescriptor(cccd);

  BLECharacteristic* rxChar = svc->createCharacteristic(
      NUS_RX,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  rxChar->setCallbacks(new RxCallbacks());

  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  BLESecurity* sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  sec->setCapability(ESP_IO_CAP_OUT);  // DisplayOnly
  sec->setKeySize(16);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  Serial.println("[buddy] advertising as 'Claude-Buddy'");

  setupBuddySprite();
}

// Send a JSON string as a NUS TX notification to the central.
// Conservative BLE payload chunk. macOS negotiates an ATT MTU >= 185, so 150
// always fits one packet; the bridge reassembles notifications by newline, so
// the exact chunk size does not need to match the write side.
static const size_t BLE_CHUNK = 150;

static void sendNotify(const char* json) {
  if (txChar == nullptr || !centralConnected) return;
  size_t total = strlen(json);
  size_t off = 0;
  do {
    size_t n = total - off;
    if (n > BLE_CHUNK) n = BLE_CHUNK;
    txChar->setValue((uint8_t*)(json + off), n);
    txChar->notify();
    off += n;
    if (off < total) delay(8);  // let the central drain between chunks
  } while (off < total);
}

// Send the button decision back to the bridge, then clear the prompt state.
// loop() repaints the buddy on the next frame.
static void sendDecision(const char* decision) {
  char buf[160];  // fits a full promptId[48] without truncating the JSON
  snprintf(buf, sizeof(buf),
           "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"%s\"}\n",
           promptId, decision);
  sendNotify(buf);
  promptId[0] = 0;     // clear pending prompt; loop repaints
  // NOTE: heartUntil is set by the BtnA button handler, NOT here, so
  // auto-approve auto-allows (which also call this) do not trigger heart.
}

// Toggle auto-approve mode, notify the bridge, and beep. The AUTO banner is
// drawn by the overlay each frame; this only mutates state + plays the beep.
static void toggleAuto() {
  autoApprove = !autoApprove;
  if (!autoApprove) autoCount = 0;
  char buf[40];
  snprintf(buf, sizeof(buf), "{\"cmd\":\"auto\",\"state\":%s}\n",
           autoApprove ? "true" : "false");
  sendNotify(buf);
  if (autoApprove) M5.Speaker.tone(880, 120);   // keep the confirmation beep
  // Banner is drawn by the overlay each frame; no direct draw here.
}

void loop() {
  M5.update();

  // Re-sync auto-approve state to a freshly (re-)connected bridge.
  // We defer this to loop() so the BLE link is fully ready before notifying.
  if (pendingAutoSync && centralConnected) {
    pendingAutoSync = false;
    char buf[40];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"auto\",\"state\":%s}\n",
             autoApprove ? "true" : "false");
    sendNotify(buf);
    Serial.printf("[ble] sent auto-sync state=%s\n",
                  autoApprove ? "true" : "false");
  }

  // --- Button handling ---------------------------------------------------
  // Long-press tracking for A and B (used by ask mode). Long-press fires once
  // while held; short-press fires on release only if no long-press fired.
  static bool aLongFired = false;
  static bool bLongFired = false;
  const bool aHeld = M5.BtnA.isPressed();
  const bool bHeld = M5.BtnB.isPressed();
  if (aHeld) { if (btnAPressMs == 0) btnAPressMs = millis(); }
  else       { btnAPressMs = 0; }
  if (bHeld) { if (btnBPressMs == 0) btnBPressMs = millis(); }
  else       { btnBPressMs = 0; }

  if (askId[0] != 0) {
    // Long-press A -> cancel (answer on laptop). Fires once per hold.
    if (aHeld && !aLongFired && btnAPressMs != 0 &&
        (millis() - btnAPressMs) >= LONG_PRESS_MS) {
      aLongFired = true;
      askCancel();
    }
    // Long-press B -> multi-select submit. Fires once per hold.
    else if (askMultiSelect && bHeld && !bLongFired && btnBPressMs != 0 &&
             (millis() - btnBPressMs) >= LONG_PRESS_MS) {
      bLongFired = true;
      askSendAnswers();
    }

    // Short-press = release before the long-press threshold. askCancel /
    // askSendAnswers above may have cleared askId, so re-check.
    if (askId[0] != 0) {
      int row = -1;
      if      (M5.BtnA.wasReleased()) { if (!aLongFired) row = 0; }
      else if (M5.BtnB.wasReleased()) { if (!bLongFired) row = 1; }
      else if (M5.BtnC.wasReleased()) { row = 2; }
      if (row >= 0) {
        AskQuestion& q = askQs[askCurQ];
        int  oi = -1;
        bool moreRow = false;
        if (q.optCount <= 2) {
          if (row == 0) oi = 0;
          else if (row == 2 && q.optCount > 1) oi = 1;
        } else if (q.optCount == 3) {
          oi = row;
        } else {  // 4 options, paged
          if (row == 2) { moreRow = true; }
          else if (askPage == 0) { oi = row; }
          else                   { oi = 2 + row; }
        }
        if (moreRow) {
          askPage = 1 - askPage;
          renderAsk();
        } else if (oi >= 0) {
          if (askMultiSelect) {
            q.selected ^= (1 << oi);
            renderAsk();                 // re-render to update [•] marker
          } else {
            q.single = oi;
            askAdvance();                // auto-advance / submit
          }
        }
      }
    }
  } else if (promptId[0] != 0 && M5.BtnA.wasPressed()) {
    heartUntil = millis() + 3000;   // button approve -> heart (auto-allows don't)
    sendDecision("allow");
  } else if (promptId[0] != 0 && M5.BtnC.wasPressed()) {
    sendDecision("deny");
  } else if (M5.BtnB.wasPressed()) {
    toggleAuto();
  }
  // Reset long-press flags on release regardless of ask state. A long-press
  // that cleared the ask (cancel / submit) must not leak a stale "fired" flag
  // into the next ask, where it would swallow the first short-press.
  if (M5.BtnA.wasReleased()) aLongFired = false;
  if (M5.BtnB.wasReleased()) bLongFired = false;

  // Debug cycle: long-press BtnA on the home screen toggles debug; BtnC advances.
  if (askId[0] == 0 && promptId[0] == 0 && !pairing) {
    static bool dbgLongFired = false;
    if (M5.BtnA.isPressed() && btnAPressMs != 0 &&
        (millis() - btnAPressMs) >= LONG_PRESS_MS && !dbgLongFired) {
      dbgLongFired = true;
      debugActive = !debugActive;
      debugState  = PS_SLEEP;
    }
    if (M5.BtnA.wasReleased()) dbgLongFired = false;
    if (debugActive && M5.BtnC.wasPressed())
      debugState = (PersonaState)(((int)debugState + 1) % (int)PS_COUNT);
  }

  // --- Buddy render (home screen only, ~30fps) ---------------------------
  if (buddyReady && askId[0] == 0 && promptId[0] == 0) {
    unsigned long now = millis();
    if (pairing) {
      renderPasskey(passkeyVal);          // loop owns the passkey draw
    } else if (now - lastFrameMs >= 33) {
      lastFrameMs = now;
      BuddyOverlay ov;
      ov.running = lastRunning; ov.waiting = lastWaiting; ov.total = lastTotal;
      portENTER_CRITICAL(&statusMux);
      strlcpy(ov.statusMsg, lastStatusMsg, sizeof(ov.statusMsg));
      portEXIT_CRITICAL(&statusMux);
      ov.autoOn      = autoApprove;
      ov.autoCount   = autoCount;
      ov.autoToast   = (autoFlashUntil != 0 && now < autoFlashUntil);
      strlcpy(ov.autoToolMsg, promptTool, sizeof(ov.autoToolMsg));
      ov.debugActive = debugActive;

      PersonaInputs pin = { centralConnected, lastRunning, heartUntil, now,
                            debugActive, debugState };
      PersonaState  st  = derivePersonaState(pin);
      buddyCoolSTick(buddySprite, st, now, ov);
      buddySprite.pushSprite(0, 0);
    }
  }

  delay(1);   // pace off the 33ms frame accumulator, keep buttons responsive
}
