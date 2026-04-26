# claude-desktop-buddy — StickS3 Port

Port of [anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy) to **M5Stack StickS3** (ESP32-S3-PICO N8R8).

Based on [`lemikegao/claude-desktop-buddy`](https://github.com/lemikegao/claude-desktop-buddy/tree/plus2) `plus2` branch — uses M5Unified + M5Canvas (M5GFX) instead of legacy M5StickCPlus + TFT_eSPI.

## Status

- ✅ Phase 1: Bootstrap from plus2 source (main.cpp, buddy.cpp, buddies/, ble_bridge.cpp, stats.h, daystats.h)
- ✅ Phase 2: PlatformIO config for ESP32-S3 (8MB Flash, OPI PSRAM, USB CDC)
- ✅ Phase 3: M5Unified handles StickS3 auto-detection (board_M5StickS3 in 0.2.14)
- ✅ Phase 4: Build succeeds (RAM 15.8% / Flash 35.1%)
- ⏳ Phase 5: Flash + smoke test (needs device)
- ⏳ Phase 6: BLE pair with Claude Desktop

## What works (vs upstream main)

| Feature | Status |
|---------|--------|
| Display + ASCII buddy animations | ✅ ported |
| BLE Nordic UART bridge | ✅ ported |
| Permission approval via BtnA | ✅ ported (KEY1=G11) |
| 4 PersonaStates (sleep/idle/busy/attention) | ✅ ported |
| NVS-backed stats (approvals/denials/velocity) | ✅ included |
| Day stats (token tracking) | ✅ included |
| GIF character loader | ❌ stripped (Phase 7) |
| Transcript transfer | ❌ stripped (Phase 7) |
| RTC clock screen | ❌ N/A (StickS3 has no RTC chip) |
| Beep sounds | ❌ N/A (StickS3 has no buzzer; ES8311 speaker available for Phase 7) |

## Build

```bash
cd /Users/xylo/ObsidianVault/NoBrain/project/claude-buddy-sticks3
pio run
```

## Flash

```bash
# Hold RESET button on side until green LED flashes (download mode)
pio run -t upload
pio device monitor
```

## Pair with Claude Desktop

1. Claude Desktop → Help → Troubleshooting → **Enable Developer Mode**
2. Developer → **Open Hardware Buddy…** → Connect → pick device

## Hardware Diff vs M5StickC Plus 2

| Component | StickC Plus 2 | StickS3 | M5Unified Handling |
|-----------|---------------|---------|---------------------|
| MCU | ESP32-PICO-V3-02 | ESP32-S3-PICO N8R8 | board auto-detect |
| Display | ST7789V2 135×240 | ST7789P3 135×240 | M5.Display abstracted |
| IMU | MPU6886 | BMI270 | M5.Imu.getAccel() abstracted |
| Power | direct GPIO | M5PM1 chip | M5.Power abstracted |
| Buttons | BtnA G37 / BtnB G39 | KEY1 G11 / KEY2 G12 | M5.BtnA/BtnB auto-mapped |
| RTC | BM8563 | none | clock feature dropped |
| Buzzer | yes | none (has ES8311 speaker) | beep dropped |

## References

- Upstream: https://github.com/anthropics/claude-desktop-buddy
- plus2 fork: https://github.com/lemikegao/claude-desktop-buddy/tree/plus2
- StickS3 docs: https://docs.m5stack.com/en/core/StickS3
- M5Unified: https://github.com/m5stack/M5Unified
