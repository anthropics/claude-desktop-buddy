#include "character.h"
#include "compat.h"
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include <ArduinoJson.h>

extern TFT_eSprite spr;

static const char* STATE_NAMES[] = {
  "sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart"
};
static const uint8_t N_STATES = 7;

// Text mode: manifest has "mode":"text", states contain {frames:[...],delay:N}.
// Frames are short strings rendered at text size 2, centered. No GIF pipeline.
struct TextState {
  char     frames[8][20];
  uint8_t  nFrames;
  uint16_t delayMs;
};
static TextState textStates[N_STATES];
static bool      textMode = false;
static uint8_t   textFrame = 0;
static uint32_t  textNext = 0;

static bool    loaded = false;
static Palette pal = { 0xC2A6, 0x0000, 0xFFFF, 0x8410, 0x0000 };
static char    basePath[48];
static const uint8_t MAX_GIFS = 32;
static char    gifPaths[MAX_GIFS][32];
static uint8_t stateStart[N_STATES];
static uint8_t stateCount[N_STATES];
static uint8_t stateRot[N_STATES];
static uint8_t gifTotal = 0;
static uint8_t curState = 0xFF;
// Which state's art is actually on screen. Equal to curState for a complete
// pack; for a partial one it points at the substitute, so the frame and
// rotation bookkeeping below indexes a state that really has files. Indexing
// by curState there would divide by a zero count.
static uint8_t drawState = 0xFF;
static const uint8_t ST_IDLE = 1;   // index into STATE_NAMES

static AnimatedGIF gif;
static File        gifFile;
static int         gifX = 0, gifY = 0, gifW = 0, gifH = 0;
// Peek mode pins the GIF bottom to the info-panel top (y=70) so the pet
// sits on the panel edge regardless of canvas height. Home mode centers
// in the upper 140px. No padding assumed in the source art.
// Per-level peek window heights: how far down the pet may draw before the
// panel underneath repaints over it. Info and pet pages clear from y=70, the
// clock face from y=98.
static const int   PEEK_TOP_HALF = 70;
static const int   PEEK_TOP_3Q   = 96;
static inline int  peekTop(uint8_t lvl) {
  return lvl == PEEK_3Q ? PEEK_TOP_3Q : PEEK_TOP_HALF;
}
static uint8_t     peekMode = PEEK_OFF;
// Draw target — defaults to the sprite; characterRenderTo() retargets to
// M5.Lcd for the landscape clock (both inherit TFT_eSPI).
static TFT_eSPI*   _tgt = &spr;
// 1:2 is a bare bit-shift and so the cheapest downscale there is, which is
// why it was the only one. It costs the clock face three quarters of the pet's
// area though, so that one now gets 3:4 at the price of a multiply per pixel.
static void gifScaled(int& w, int& h) {
  if (peekMode == PEEK_3Q)        { w = gifW * 3 / 4; h = gifH * 3 / 4; }
  else if (peekMode == PEEK_HALF) { w = gifW / 2;     h = gifH / 2; }
  else                            { w = gifW;         h = gifH; }
}
static void gifPlace() {
  int outW, outH;
  gifScaled(outW, outH);
  gifX = (spr.width() - outW) / 2;
  gifY = peekMode ? (peekTop(peekMode) - outH) / 2 : (140 - outH) / 2;
}
static uint32_t    nextFrameAt = 0;
static uint32_t    animPauseUntil = 0;
static uint32_t    variantStartedMs = 0;
static const uint32_t VARIANT_DWELL_MS = 5000;
static const uint32_t ANIM_PAUSE_MS    = 800;
static bool        gifOpen = false;
static uint16_t    framesThisPlay = 0;   // frames drawn since open/reset

static uint16_t parseHexColor(const char* s, uint16_t fallback) {
  if (!s) return fallback;
  if (*s == '#') s++;
  uint32_t v = strtoul(s, nullptr, 16);
  return (uint16_t)(((v >> 19) & 0x1F) << 11 | ((v >> 10) & 0x3F) << 5 | ((v >> 3) & 0x1F));
}

// --- AnimatedGIF file callbacks (LittleFS) ------------------------------

static void* gifOpenCb(const char* fname, int32_t* pSize) {
  gifFile = LittleFS.open(fname, "r");
  if (!gifFile) return nullptr;
  *pSize = gifFile.size();
  return (void*)&gifFile;
}

static void gifCloseCb(void* handle) {
  File* f = (File*)handle;
  if (f) f->close();
}

static int32_t gifReadCb(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  File* f = (File*)pFile->fHandle;
  int32_t n = f->read(pBuf, iLen);
  pFile->iPos = f->position();
  return n;
}

static int32_t gifSeekCb(GIFFILE* pFile, int32_t iPosition) {
  File* f = (File*)pFile->fHandle;
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

// --- Draw callback: one scanline → line buffer → pushImage ------------
// Transparent pixels get the character's bg color so each frame fully
// paints its region — no ghosting from prior frames.

static void gifDrawCb(GIFDRAW* d) {
  uint16_t* pal16 = d->pPalette;
  uint8_t*  src   = d->pPixels;
  uint8_t   t     = d->ucTransparent;
  bool      hasT  = d->ucHasTransparency;
  int       srcY  = d->iY + d->y;
  // GIFs are unoptimized full-frame (gifsicle --unoptimize --lossy) so
  // transparent always means background — no disposal/delta handling.
  // The -O2/-O3 sub-rect + delta-transparency path was tried and reverted:
  // disposal semantics are encoder-dependent and don't compose with the
  // 2:1 peek downscale's sample alignment.
  auto put = [&](int x, int y, uint8_t idx) {
    _tgt->drawPixel(x, y, (hasT && idx == t) ? pal.bg : pal16[idx]);
  };

  if (peekMode == PEEK_HALF) {
    if (srcY & 1) return;
    int y = gifY + (srcY >> 1);
    if (y < 0 || y >= PEEK_TOP_HALF) return;
    int x0 = gifX + (d->iX >> 1);
    int w  = d->iWidth >> 1;
    for (int i = 0; i < w; i++) put(x0 + i, y, src[i << 1]);
    return;
  }
  if (peekMode == PEEK_3Q) {
    // 3:4 nearest-neighbour: keep three source rows and columns out of every
    // four. srcY % 4 == 1 is the row whose destination duplicates its
    // predecessor's, so that is the one to drop.
    if ((srcY & 3) == 1) return;
    int y = gifY + srcY * 3 / 4;
    if (y < 0 || y >= PEEK_TOP_3Q) return;
    int x0 = gifX + d->iX * 3 / 4;
    int w  = d->iWidth * 3 / 4;
    for (int i = 0; i < w; i++) {
      int si = i * 4 / 3;
      if (si >= d->iWidth) break;
      put(x0 + i, y, src[si]);
    }
    return;
  }

  int y = gifY + srcY;
  if (y < 0 || y >= spr.height()) return;
  int x0 = gifX + d->iX;
  int w  = d->iWidth;
  if (w > 256) w = 256;
  if (x0 < 0) { src -= x0; w += x0; x0 = 0; }
  if (x0 + w > spr.width()) w = spr.width() - x0;
  if (w <= 0) return;
  for (int i = 0; i < w; i++) put(x0 + i, y, src[i]);
}

// --- Public -------------------------------------------------------------

bool characterInit(const char* name) {
  // format-on-fail=true: a fresh device (or the new StickS3 partition) has an
  // unformatted LittleFS partition; format it once so character storage works.
  if (!LittleFS.begin(true)) {
    // begin() fails if already mounted — that's fine on reload
    if (!LittleFS.open("/")) {
      Serial.println("[char] LittleFS mount failed");
      return false;
    }
  }

  // No name → scan /characters/ for the first directory present.
  // Makes the boot character whatever you last installed.
  static char scanned[24];
  if (!name) {
    File d = LittleFS.open("/characters");
    if (d && d.isDirectory()) {
      File e = d.openNextFile();
      while (e) {
        if (e.isDirectory()) {
          const char* n = strrchr(e.name(), '/');
          strncpy(scanned, n ? n + 1 : e.name(), sizeof(scanned) - 1);
          scanned[sizeof(scanned) - 1] = 0;
          name = scanned;
          break;
        }
        e = d.openNextFile();
      }
      d.close();
    }
    if (!name) { Serial.println("[char] no characters installed"); return false; }
  }

  snprintf(basePath, sizeof(basePath), "/characters/%s", name);
  char mpath[64];
  snprintf(mpath, sizeof(mpath), "%s/manifest.json", basePath);

  File mf = LittleFS.open(mpath, "r");
  if (!mf) {
    Serial.printf("[char] manifest not found: %s\n", mpath);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, mf);
  mf.close();
  if (err) {
    Serial.printf("[char] manifest parse: %s\n", err.c_str());
    return false;
  }

  JsonObject colors = doc["colors"];
  pal.body    = parseHexColor(colors["body"],    pal.body);
  pal.bg      = parseHexColor(colors["bg"],      pal.bg);
  pal.text    = parseHexColor(colors["text"],    pal.text);
  pal.textDim = parseHexColor(colors["textDim"], pal.textDim);
  pal.ink     = parseHexColor(colors["ink"],     pal.ink);

  const char* mode = doc["mode"];
  textMode = (mode && strcmp(mode, "text") == 0);

  JsonObject states = doc["states"];

  if (textMode) {
    for (uint8_t i = 0; i < N_STATES; i++) {
      TextState& ts = textStates[i];
      ts.nFrames = 0;
      ts.delayMs = 200;
      JsonObject st = states[STATE_NAMES[i]];
      if (st.isNull()) continue;
      ts.delayMs = st["delay"] | 200;
      JsonArray fr = st["frames"];
      for (JsonVariant v : fr) {
        if (ts.nFrames >= 8) break;
        const char* s = v.as<const char*>();
        strncpy(ts.frames[ts.nFrames], s ? s : "", 19);
        ts.frames[ts.nFrames][19] = 0;
        ts.nFrames++;
      }
    }
    loaded = true;
    Serial.printf("[char] loaded '%s' (text mode, %d states)\n", name, N_STATES);
    return true;
  }

  gifTotal = 0;
  for (uint8_t i = 0; i < N_STATES; i++) {
    stateStart[i] = gifTotal;
    stateCount[i] = 0;
    stateRot[i]   = 0;
    JsonVariant v = states[STATE_NAMES[i]];
    if (v.is<JsonArray>()) {
      for (JsonVariant e : v.as<JsonArray>()) {
        if (gifTotal >= MAX_GIFS) break;
        const char* fn = e.as<const char*>();
        if (fn) { snprintf(gifPaths[gifTotal], 32, "%s", fn); gifTotal++; stateCount[i]++; }
      }
    } else {
      const char* fn = v.as<const char*>();
      if (fn) { snprintf(gifPaths[gifTotal], 32, "%s", fn); gifTotal++; stateCount[i] = 1; }
    }
  }

  gif.begin(LITTLE_ENDIAN_PIXELS);
  loaded = true;
  Serial.printf("[char] loaded '%s' from %s\n", (const char*)doc["name"], basePath);
  return true;
}

bool characterLoaded() { return loaded; }
const Palette& characterPalette() { return pal; }

// One-shot half-scale render to an arbitrary surface (M5.Lcd for the
// landscape clock). Caller owns clearing. Advances frame timing so
// animation runs even when characterTick() is bypassed.
void characterRenderTo(TFT_eSPI* tgt, int cx, int cy) {
  if (!gifOpen) return;   // caller opens via characterSetState(activeState)
  TFT_eSPI* prevT = _tgt; uint8_t prevP = peekMode; int px = gifX, py = gifY;
  // Landscape clock draws into a small corner: half scale, and the gifW/4
  // offsets below assume it.
  _tgt = tgt; peekMode = PEEK_HALF;
  gifX = cx - gifW / 4;
  gifY = cy - gifH / 4;
  uint32_t now = millis();
  if (now >= nextFrameAt) {
    int delayMs = 0;
    if (!gif.playFrame(false, &delayMs)) { gif.reset(); gif.playFrame(false, &delayMs); }
    nextFrameAt = now + (delayMs > 0 ? delayMs : 100);
  }
  _tgt = prevT; peekMode = prevP; gifX = px; gifY = py;
}

bool characterSetPeek(uint8_t level) {
  if (peekMode == level) return false;
  peekMode = level;
  characterInvalidate();
  return true;
}

void characterClose() {
  if (gifOpen) { gif.close(); gifOpen = false; }
  loaded = false;
  textMode = false;
  curState = 0xFF;
  drawState = 0xFF;
}

void characterInvalidate() {
  if (!loaded) return;
  if (textMode) {
    spr.fillSprite(pal.bg);
    uint8_t s = curState; curState = 0xFF;
    characterSetState(s);
    return;
  }
  if (gifOpen) { gif.close(); gifOpen = false; }
  animPauseUntil = 0;
  uint8_t s = curState; curState = 0xFF;
  characterSetState(s);
}

void characterSetState(uint8_t s) {
  if (!loaded || s >= N_STATES || s == curState) return;

  if (textMode) {
    curState = s;
    textFrame = 0;
    textNext = 0;
    spr.fillSprite(pal.bg);
    return;
  }

  if (gifOpen) { gif.close(); gifOpen = false; }
  animPauseUntil = 0;
  curState = s;

  // A pack need not cover all seven states. Missing ones used to close the
  // open GIF and open nothing, and the renderer draws nothing without one --
  // so the pet froze on whatever frame happened to be in the sprite, which
  // reads as a hang rather than as a state with no art. Substitute instead:
  // idle if the pack has it, otherwise the first state that does.
  uint8_t draw = s;
  if (stateCount[draw] == 0) {
    draw = ST_IDLE;
    if (stateCount[draw] == 0) {
      draw = 0xFF;
      for (uint8_t i = 0; i < N_STATES; i++) {
        if (stateCount[i]) { draw = i; break; }
      }
    }
    if (draw == 0xFF) {
      Serial.printf("[char] state %d missing and pack has no frames at all\n", s);
      return;
    }
    Serial.printf("[char] state %d missing, falling back to %d\n", s, draw);
  }
  drawState = draw;

  uint8_t idx = stateStart[draw] + stateRot[draw];
  char full[80];
  snprintf(full, sizeof(full), "%s/%s", basePath, gifPaths[idx]);
  if (gif.open(full, gifOpenCb, gifCloseCb, gifReadCb, gifSeekCb, gifDrawCb)) {
    gifOpen = true;
    gifW = gif.getCanvasWidth();
    gifH = gif.getCanvasHeight();
    gifPlace();
    // Clear only when the pet lands somewhere new. The GIFs are unoptimised
    // full-frame, so a same-size, same-place animation repaints every pixel it
    // owns and a wipe would just be a visible blink on every state change.
    // A different size or origin does leave stale pixels, so that still wipes.
    static int lastX = -1, lastY = -1, lastW = -1, lastH = -1;
    if (gifX != lastX || gifY != lastY || gifW != lastW || gifH != lastH) {
      spr.fillSprite(pal.bg);
      lastX = gifX; lastY = gifY; lastW = gifW; lastH = gifH;
    }
    nextFrameAt = 0;
    framesThisPlay = 0;
    variantStartedMs = millis();
    Serial.printf("[char] %s: %dx%d @ (%d,%d) heap=%u\n",
      gifPaths[idx], gifW, gifH, gifX, gifY, ESP.getFreeHeap());
  } else {
    Serial.printf("[char] open failed: %s (err %d)\n", full, gif.getLastError());
  }
}

void characterTick() {
  if (!loaded) return;

  if (textMode) {
    TextState& ts = textStates[curState];
    if (ts.nFrames == 0) return;
    uint32_t now = millis();
    if (now < textNext) return;
    textNext = now + ts.delayMs;

    // Clear a band around the text, not the whole sprite — keeps overlays
    // like the approval panel and the HUD untouched.
    int cy = peekMode ? 35 : 60;
    spr.fillRect(0, cy - 14, spr.width(), 28, pal.bg);

    const char* line = ts.frames[textFrame];
    int len = strlen(line);
    int tw = len * 12;                                    // size-2 glyph width
    spr.setTextColor(pal.body, pal.bg);
    spr.setTextSize(2);
    spr.setCursor((spr.width() - tw) / 2, cy - 8);
    spr.print(line);

    textFrame = (textFrame + 1) % ts.nFrames;
    return;
  }

  uint32_t now = millis();

  if (!gifOpen) {
    // Between animations in a rotation: hold the last frame, then open
    // the next gif when the pause elapses.
    if (animPauseUntil && now >= animPauseUntil) {
      animPauseUntil = 0;
      uint8_t s = curState; curState = 0xFF;
      characterSetState(s);
    }
    return;
  }
  if (now < nextFrameAt) return;

  int delayMs = 0;
  if (!gif.playFrame(false, &delayMs)) {
    // End of animation. Single-gif states freeze on the last frame instead
    // of reopening — the LittleFS open + GIF header decode is a multi-ms
    // blocking burst, and during sleep state it was looping every ~4s,
    // possibly starving the BT controller. The sprite already holds the
    // last frame; just stop ticking. Multi-gif states (idle rotation)
    // still advance after a brief pause.
    if (stateCount[drawState] == 1) {
      // Loop in place rather than freezing. What made freezing attractive was
      // the cost of *reopening* the file -- a LittleFS open plus header decode
      // -- but gif.reset() only rewinds the decoder, which is exactly what the
      // multi-variant path below already does inside its dwell window. Looping
      // here means a state needs only one file to animate continuously, with
      // no reopen, no full-sprite clear and no pause between passes.
      if (framesThisPlay <= 1) {   // nothing to animate: a still image
        gif.close();
        gifOpen = false;
        return;
      }
      gif.reset();
      framesThisPlay = 0;
      nextFrameAt = now;
      return;
    }
    // Multi-variant: loop the same GIF until the dwell window elapses, then
    // rotate. Short bufo idles (~0.5s/loop) get ~10 plays instead of one
    // flash + 3s freeze.
    if (now - variantStartedMs < VARIANT_DWELL_MS) {
      gif.reset();
      nextFrameAt = now;
      return;
    }
    gif.close(); gifOpen = false;
    stateRot[drawState] = (stateRot[drawState] + 1) % stateCount[drawState];
    animPauseUntil = now + ANIM_PAUSE_MS;
    return;
  }
  framesThisPlay++;
  nextFrameAt = now + (delayMs > 0 ? delayMs : 100);
}
