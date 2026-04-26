# Handoff: claude-desktop-buddy → M5Stack StickS3 Port

**Date:** 2026-04-26
**Previous agent:** Claude (Opus 4.7)
**Continued by:** Gemini CLI
**User:** XYLO (xylo.agi@gmail.com)
**Project root:** `/Users/xylo/ObsidianVault/NoBrain/project/claude-buddy-sticks3/`

---

## TL;DR

Successfully ported [anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy) (originally targets M5StickC Plus) to **M5Stack StickS3** (ESP32-S3-PICO N8R8). Built on [`lemikegao/claude-desktop-buddy:plus2`](https://github.com/lemikegao/claude-desktop-buddy/tree/plus2) which already migrated to M5Unified library.

**Working:**
- Builds + flashes cleanly via PlatformIO
- BLE advertise as `claude-XXXX` (last 4 hex of MAC), pair OK with Claude Desktop on Mac
- ASCII buddy animations render at 5fps, 18 species cycleable via KEY1
- Stats persist via NVS (approvals, denials, lifetime tokens)
- Day stats track tokens via BLE bridge snapshots

**Broken / Pending Test:**
1. **Battery indicator shows wrong values** (code added, not yet verified). Root cause: M5Unified `getBatteryVoltage()` for StickS3 reads register `0x26 (5VOUT_L)` which is **5V output voltage to Grove/Hat, NOT battery voltage**. Upstream bug in M5Unified `Power_Class.cpp:1900-1908`. See "Known Issues" below.
2. **Approval button does not work** (code added in last edit, NOT yet flashed/tested). plus2 stub stripped permission flow. Code added in latest main.cpp commit (`promptId`, `sendCmd`, BtnA→approve, BtnB→deny). Needs flash + test.
3. **GIF character (bufo) flickers fast** when enabled. Disabled via comment in `setup()`. ASCII fallback works fine. To re-enable, uncomment `characterInit("bufo")`. Root cause TBD — likely GIF playFrame timing vs main loop fillRect collision.

---

## User's Latest Requests (in order)

1. ✅ Revert to ASCII buddy (GIF too flickery) — done, comment toggle in setup()
2. ✅ Add battery indicator — code added, **showing wrong values** (M5Unified upstream bug)
3. ⏳ Want approval button to work when on battery + Claude Desktop sends prompt — code added, **not yet flashed**

---

## Hardware Context

**Device:** M5Stack StickS3 (SKU: K150)
- ESP32-S3-PICO N8R8 (8MB Flash + 8MB Octal PSRAM)
- Display: ST7789P3 135×240 1.14"
- IMU: BMI270 6-axis (I2C 0x68)
- Power chip: **M5PM1** (custom, I2C — different from StickC's AXP192)
- Audio: ES8311 codec + MEMS mic + AW8737 speaker
- IR: TX G46 / RX G42
- Battery: 250mAh Li-ion
- Buttons: KEY1=G11, KEY2=G12 (M5Unified maps to BtnA/BtnB)
- USB-C, native USB CDC

**Wiki source page:** `/Users/xylo/ObsidianVault/NoBrain/wiki/sources/sticks3.md`

---

## Project Files

```
claude-buddy-sticks3/
├── platformio.ini          # ESP32-S3 + 8MB flash + qio_opi PSRAM + LittleFS
├── README.md               # Status doc
├── HANDOFF.md              # THIS FILE
├── data/
│   └── characters/bufo/    # GIF pack (596KB, flashed via uploadfs)
└── src/
    ├── main.cpp            # ENTRY — modified heavily
    ├── buddy.cpp/h         # ASCII buddy (works)
    ├── buddy_common.h
    ├── buddies/            # 18 species: capybara, cat, dragon, duck, goose,
    │                       #   octopus, penguin, rabbit, robot, snail, turtle,
    │                       #   axolotl, blob, cactus, chonk, ghost, mushroom, owl
    ├── ble_bridge.cpp/h    # Nordic UART BLE service (works)
    ├── character.cpp/h     # GIF renderer (DISABLED, flickery)
    ├── stats.h             # NVS-backed lifetime stats
    └── daystats.h          # Per-day token tracking
```

---

## Key Code Changes from plus2 baseline

### 1. platformio.ini
- Switched from `board = m5stick-c` → `board = esp32-s3-devkitc-1`
- Added `board_build.arduino.memory_type = qio_opi` for Octal PSRAM
- Added flags: `-DESP32S3`, `-DBOARD_HAS_PSRAM`, `-mfix-esp32-psram-cache-issue`, `-DARDUINO_USB_CDC_ON_BOOT=1`, `-DARDUINO_USB_MODE=1`, `-DARDUINO_M5STACK_STICKS3`
- Added `bitbank2/AnimatedGIF @ ^2.1.1` for GIF support
- LittleFS partition via `default_8MB.csv` + `board_build.filesystem = littlefs`

### 2. character.cpp/h (ported from upstream)
- `M5StickCPlus.h` → `M5Unified.h`
- `TFT_eSprite spr` → `M5Canvas spr`
- `TFT_eSPI*` → `LovyanGFX*` (typedef from `lgfx::v1`)
- character.h: changed `class LovyanGFX;` forward-decl → `#include <M5GFX.h>` (because LovyanGFX is a typedef, not a class)

### 3. main.cpp additions
- `#include "character.h"` + `characterInit("bufo")` (commented out)
- `drawBattery()` function (top-right corner, color-coded) — **uses wrong API**
- `static char promptId[40]` + `promptTool[24]` — pending permission tracking
- Parse `doc["prompt"]["id"]` + `doc["prompt"]["tool"]` in `applyJsonLine`
- `sendCmd(json)` helper that wraps `bleWrite()` + newline
- BtnA in prompt → `{"cmd":"permission","id":"...","decision":"once"}`
- BtnB in prompt → `{"cmd":"permission","id":"...","decision":"deny"}`
- BtnA outside prompt → cycle species (existing behavior)

---

## Known Issues

### Issue 1: Battery shows wrong values

**File:** `/Users/xylo/ObsidianVault/NoBrain/project/claude-buddy-sticks3/.pio/libdeps/m5sticks3/M5Unified/src/utility/Power_Class.cpp:1900-1908`

```cpp
case board_t::board_M5StickS3: {
  // Read output voltage from device PM1: register 0x26 (5VOUT_L) and 0x27 (5VOUT_H)
  // Unit: mV, format: (5VOUT_H << 8) | 5VOUT_L
  uint8_t buf[2];
  if (M5.In_I2C.readRegister(m5pm1_i2c_addr, 0x26, buf, sizeof(buf), i2c_freq)) {
    return (int16_t)((buf[1] << 8) | buf[0]);
  }
  return 0;
}
```

This reads the **5V output regulator voltage** (typically 5000mV when EXT_5V_EN on, 0 when off), NOT the battery voltage. Then `getBatteryLevel()` does `pct = (mv - 3300) / (4200 - 3300) * 100` which gives garbage values from a 5V reading.

**Possible fixes:**
- A) Use `m5stack/M5PM1` library directly (https://github.com/m5stack/M5PM1) — has `M5PM1.getBatteryVoltage()` which reads correct register
- B) Read M5PM1 via I2C directly. M5PM1 datasheet PDF: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_EN.pdf — find correct battery voltage register
- C) Submit PR upstream to M5Unified fixing the StickS3 case
- D) Hide battery if reading > 4500mV (clearly wrong) — show "USB" instead when charging

Recommended: **option A** — add `m5stack/M5PM1` to `lib_deps` and call its battery API.

### Issue 2: Approval button untested

Code is in main.cpp but build was last completed but **not flashed**. Need to:
1. User puts device in download mode (hold RESET until green LED flashes)
2. `pio run -t upload --upload-port /dev/cu.usbmodem101`
3. User in Claude Desktop triggers a permission prompt (e.g. ask Claude Code to run a command needing approval)
4. Verify StickS3 shows attention state, then KEY1 = approve / KEY2 = deny works

**Possible bugs to watch:**
- BLE write back path may need authentication/encryption (NUS RX char is `WRITE_ENCRYPTED` per ble_bridge.cpp:106) — should be fine post-pair
- JSON cmd format — verify against REFERENCE.md in upstream repo
- May need `\n` line terminator (added in `sendCmd`)

### Issue 3: GIF character flickering

When `characterInit("bufo")` is enabled in main.cpp, the bufo GIF renders but flickers fast. Symptoms:
- bufo appears very small or partial in some frames
- Screen cycles between drawn buddy and blank fast

**Suspected cause:** `characterTick()` is called every loop iteration but GIF playFrame has internal timing (delayMs). Combined with `spr.fillRect(0, 0, W, TEXT_TOP, 0x0000)` wipe each loop, frames get cleared mid-play.

**Possible fixes:**
- Don't fillRect every loop — only when state changes
- Throttle character render to GIF's natural frame rate
- Use buddySpr offscreen pattern (like ASCII buddy) — render character into offscreen, push only on new frame

User abandoned GIF and prefers ASCII for now. Can revisit later.

---

## Build / Flash / Monitor Commands

```bash
cd /Users/xylo/ObsidianVault/NoBrain/project/claude-buddy-sticks3

# Build
pio run

# Flash firmware (device must be in download mode: hold RESET → green LED flashes)
pio run -t upload --upload-port /dev/cu.usbmodem101

# Flash LittleFS image (only if data/ changed; same download mode req)
pio run -t uploadfs --upload-port /dev/cu.usbmodem101

# Serial monitor (pio's miniterm needs TTY — use this Python alternative for non-TTY)
/opt/homebrew/Cellar/platformio/6.1.19_1/libexec/bin/python3 -c "
import serial, time
s = serial.Serial('/dev/cu.usbmodem101', 115200, timeout=2)
end = time.time() + 10
while time.time() < end:
    n = s.in_waiting
    if n: print(s.read(n).decode('utf-8','replace'), end='')
    time.sleep(0.1)
s.close()
"
```

**Boot/reset sequence on StickS3:**
- Single press RESET (side button) = power on / wake
- Double press = power off
- Hold RESET = enter download mode (green LED flashes)
- USB CDC port: `/dev/cu.usbmodem101` (varies if other USB serial devices present)

---

## How User Wants to Continue

User said: *"เตรียมส่งต่อให้ gemini cli ด้วย token จะหมดแล้ว"* (preparing handoff to Gemini CLI, tokens running out).

**Top priorities for Gemini:**

1. **Flash the current build** (has approval support added) and test:
   - Can KEY1 approve a Claude Desktop permission prompt?
   - Does StickS3 show attention state when prompt arrives?
   - Confirm BLE works on battery (after device unplugged from USB)

2. **Fix battery indicator** — implement option A (use M5PM1 library directly):
   ```cpp
   // In platformio.ini lib_deps:
   m5stack/M5PM1=https://github.com/m5stack/M5PM1
   
   // In main.cpp:
   #include <M5PM1.h>
   // ... in drawBattery():
   int16_t mv = M5PM1.getBatteryVoltage();  // verify API
   int pct = (mv - 3300) * 100 / (4200 - 3300);
   pct = max(0, min(100, pct));
   ```

3. **Wiki update** (`/Users/xylo/ObsidianVault/NoBrain/wiki/`):
   - Append log entry to `log.md` documenting StickS3 port
   - Update `entities/m5stack.md` with port story
   - Optionally create `synthesis/sticks3-port-story.md`

4. **PR upstream** (Phase B from plan):
   - Fork `anthropics/claude-desktop-buddy` to user's GitHub
   - Push project as `sticks3` branch
   - Open PR titled "Add M5StickS3 support" with HANDOFF as PR body summary

5. **GIF fix** (optional, low priority — user prefers ASCII):
   - Fix flickering by only redrawing on new GIF frame

---

## Reference Links

- Upstream repo: https://github.com/anthropics/claude-desktop-buddy
- BLE protocol: REFERENCE.md in upstream
- plus2 fork (baseline): https://github.com/lemikegao/claude-desktop-buddy/tree/plus2
- M5Unified library: https://github.com/m5stack/M5Unified (v0.2.14, has `board_M5StickS3`)
- M5PM1 library: https://github.com/m5stack/M5PM1
- StickS3 docs: https://docs.m5stack.com/en/core/StickS3
- StickS3 schematic: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf
- M5PM1 datasheet: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_EN.pdf

---

## User Profile

- **XYLO** — IT staff at กรมควบคุมโรค (Department of Disease Control), Thailand Ministry of Public Health
- Prefers Thai but understands English well
- Likes terse "caveman mode" style replies (short, fragments OK, no fluff)
- Has Mac (this session), uses Cloudflare Workers + Telegram bot integration
- Active project: OpenClaw crypto trading bot (separate)
- Wiki vault: `/Users/xylo/ObsidianVault/NoBrain/`
- This is a *for-fun* hardware project — no production stakes

---

## Final Note for Gemini

If you find anything unclear, the user is patient and will answer. Use Thai for casual replies, English in code/commits. Don't over-explain — go straight to action.

The build environment is set up and verified working. PlatformIO 6.1.19 installed via brew. Device is on `/dev/cu.usbmodem101` when plugged in.

Good luck. 🦞
