# Phase 01 — M5StickS3 hardware port

## Goal

Make the firmware build, flash, and run on the **M5Stack StickS3** (ESP32-S3-PICO-1-N8R8) without breaking the existing **M5StickC Plus** (ESP32) build. Both targets coexist in `platformio.ini` as separate envs.

Target device docs: https://docs.m5stack.com/en/core/StickS3

## Hardware deltas vs current target

| Component | StickC Plus (current) | StickS3 (new) |
|---|---|---|
| MCU | ESP32 (xtensa) | ESP32-S3 (LX7), 8 MB flash, 8 MB PSRAM |
| Library | `m5stack/M5StickCPlus` | `m5stack/M5Unified` |
| Display | ST7789v2 135×240 | ST7789P3 135×240 (same resolution) |
| PMIC | AXP192 | M5PM1 |
| IMU | MPU6886 | BMI270 |
| RTC | BM8563 (hardware) | **none** |
| Audio | passive buzzer (PWM) | ES8311 codec + AW8737 amp + speaker |
| Sprites | `TFT_eSprite` | `M5Canvas` (M5GFX) |

## Decisions (locked before planning)

1. **Two coexisting PIO envs.** `m5stickc-plus` (untouched) and `m5stickc-plus-s3` (new). No shared `[env]` block — keep envs independent so a regression in one doesn't silently affect the other.
2. **Library: M5Unified** for the S3 build. We do **not** also migrate the StickC Plus build to M5Unified in this phase — out of scope, would balloon the diff. Source files use `#ifdef ARDUINO_M5STACK_STAMPS3` (or the actual board macro for StickS3) to switch include + API surface.
3. **Clock feature: stubbed.** No hardware RTC on StickS3 and WiFi/NTP is deferred to a future phase. Landscape clock view shows "—:—" on the S3 build. The view layout still renders (so the gesture/orientation code is exercised), but time/date strings are placeholders. NTP wiring can be added later without touching the port-level changes.
4. **AXP temp readout dropped on S3.** `M5.Axp.GetTempInAXP192()` (debug page) has no equivalent on M5PM1; show `--` on S3.
5. **No new features.** Pure port. No UX changes, no new menu items beyond the WiFi config needed for NTP.
6. **BLE bridge preserved.** `src/ble_bridge.cpp` has zero M5 API references — should compile as-is on S3 (Arduino-ESP32 BLE stack is unchanged).

## Scope (file-by-file)

| File | LOC | M5 API sites | Change |
|---|---|---|---|
| `platformio.ini` | 16 | — | Add `[env:m5stickc-plus-s3]` |
| `src/main.cpp` | 1265 | ~38 | Wrap `M5.Axp/Imu/Rtc/Beep` behind `#ifdef`; replace `TFT_eSprite` with portable typedef; stub clock strings to `"—:—"` on S3 |
| `src/buddy.cpp` | 196 | 4 | Sprite typedef swap |
| `src/character.cpp` | 401 | 4 | Sprite typedef swap |
| `src/ble_bridge.cpp` | 180 | 0 | No change (no M5 API references) |
| `src/buddy_common.h` | 37 | — | Central `BuddySprite` typedef alias (`TFT_eSprite` or `M5Canvas`) |

Rough budget: ~150 LOC changed (smaller without NTP), 2–3 hours of focused work plus on-device validation.

## Out of scope

- Migrating the StickC Plus build to M5Unified
- Any new buddy/character/UI features
- Removing the legacy StickC Plus env (kept indefinitely)
- WiFi configuration UX (deferred — separate phase)
- NTP time sync (deferred — separate phase)
- Captive portal setup
- OTA updates

## Verification (what "done" looks like)

1. `pio run -e m5stickc-plus` succeeds (existing build unchanged)
2. `pio run -e m5stickc-plus-s3 -t upload` flashes to the StickS3 over USB
3. On hardware:
   - Boot to default buddy view, GIF/sprite render correct
   - BtnA/BtnB cycle pets, brightness menu works
   - Speaker plays the boot beep
   - Shake / face-down detection trigger expected state changes
   - Landscape clock view renders layout but time/date show "—:—" (NTP deferred)
   - BLE bridge pairs from desktop and round-trips a prompt

## Next step

`/gsd-plan-phase` style breakdown isn't available (no GSD bootstrap). Write `PLAN.md` in this directory by hand once open questions are resolved.
