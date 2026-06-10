#pragma once
// ---------------------------------------------------------------------------
// Hardware compatibility shim.
//
// The firmware was originally written against the board-specific
// `M5StickCPlus` library (ESP32 + AXP192 + MPU6886). That library does not
// support the ESP32-S3, so the project now builds against `M5Unified`, which
// supports BOTH the original M5StickC Plus and the newer M5StickS3
// (ESP32-S3 + M5PM1 PMIC + BMI270 IMU + ST7789P3 LCD + ES8311 audio) and
// auto-detects the board at runtime.
//
// To keep the ~3k lines of UI/render code untouched, this header re-creates
// the handful of legacy names the code still uses (TFT_eSprite, TFT_eSPI,
// RTC_*TypeDef) on top of M5Unified / M5GFX, plus small helpers for the few
// peripherals whose APIs changed (power, LED, chip temperature).
// ---------------------------------------------------------------------------
#include <M5Unified.h>
#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

// --- Display surfaces -------------------------------------------------------
// TFT_eSprite -> M5Canvas (off-screen sprite, same drawing API).
// TFT_eSPI    -> lgfx::LGFXBase, the common base of both M5GFX (M5.Lcd /
//                M5.Display) and M5Canvas. The *RenderTo() helpers take a
//                pointer to it so they can draw to either the sprite or the
//                live LCD (landscape clock).
using TFT_eSprite = M5Canvas;
using TFT_eSPI    = lgfx::LGFXBase;

// --- Legacy bare color names (the old build pulled these from TFT_eSPI) -----
#ifndef GREEN
#define GREEN 0x07E0
#endif
#ifndef RED
#define RED 0xF800
#endif

// --- Software RTC -----------------------------------------------------------
// The StickS3 has no RTC chip, so the clock is backed by the ESP32 system
// clock instead of M5.Rtc. The bridge pushes already-localized time
// components; we round-trip them through the system clock as UTC
// (timegm/gmtime_r) so no timezone offset is applied twice. Behaves the same
// on the StickC Plus, which keeps the clock working without its coin-cell RTC.
struct RTC_TimeTypeDef { uint8_t Hours, Minutes, Seconds; };
struct RTC_DateTypeDef { uint8_t WeekDay, Month, Date; uint16_t Year; };

// Portable UTC broken-down-time -> epoch (newlib lacks timegm). Pairs with
// gmtime_r below, so the conversion is fully timezone-independent.
static inline time_t _compatTimegm(const struct tm* t) {
  static const int mdays[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  long y = t->tm_year + 1900;
  long days = (y - 1970) * 365 + (y - 1969) / 4 - (y - 1901) / 100 + (y - 1601) / 400;
  days += mdays[t->tm_mon % 12];
  if (t->tm_mon > 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days += 1;
  days += t->tm_mday - 1;
  return ((time_t)days * 24 + t->tm_hour) * 3600 + t->tm_min * 60 + t->tm_sec;
}

static inline void compatRtcSet(const RTC_TimeTypeDef* tm, const RTC_DateTypeDef* dt) {
  struct tm t = {};
  t.tm_hour = tm->Hours;  t.tm_min  = tm->Minutes;    t.tm_sec  = tm->Seconds;
  t.tm_mday = dt->Date;   t.tm_mon  = (int)dt->Month - 1;
  t.tm_year = (int)dt->Year - 1900;
  time_t epoch = _compatTimegm(&t);
  struct timeval now = { epoch, 0 };
  settimeofday(&now, nullptr);
}
static inline void _compatRtcNow(struct tm* out) {
  time_t now = time(nullptr);
  gmtime_r(&now, out);
}
static inline void compatRtcGetTime(RTC_TimeTypeDef* tm) {
  struct tm t; _compatRtcNow(&t);
  tm->Hours = t.tm_hour; tm->Minutes = t.tm_min; tm->Seconds = t.tm_sec;
}
static inline void compatRtcGetDate(RTC_DateTypeDef* dt) {
  struct tm t; _compatRtcNow(&t);
  dt->WeekDay = t.tm_wday; dt->Month = t.tm_mon + 1;
  dt->Date    = t.tm_mday; dt->Year  = t.tm_year + 1900;
}

// --- Power / USB ------------------------------------------------------------
// getVBUSVoltage() returns mV, or <=0 when the PMIC can't report it. Fall back
// to the charge state so USB-powered clock mode still triggers on StickS3.
static inline bool compatOnUsb() {
  int v = M5.Power.getVBUSVoltage();
  if (v > 0) return v > 4000;
  return (int)M5.Power.isCharging() != 0;   // charging/unknown => assume USB
}

// --- Onboard LED ------------------------------------------------------------
// StickC Plus: red LED on GPIO10, active-low. StickS3 has no user LED (GPIO10
// is Grove Port-A there), so the LED calls compile to no-ops.
#if defined(BOARD_STICKS3)
static inline void compatLedInit() {}
static inline void compatLedSet(bool) {}
#else
static const int COMPAT_LED_PIN = 10;
static inline void compatLedInit() {
  pinMode(COMPAT_LED_PIN, OUTPUT);
  digitalWrite(COMPAT_LED_PIN, HIGH);   // active-low: HIGH = off
}
static inline void compatLedSet(bool on) {
  digitalWrite(COMPAT_LED_PIN, on ? LOW : HIGH);
}
#endif

// --- Chip temperature (replaces AXP192 GetTempInAXP192) ---------------------
// No PMIC temperature on either unified path; report the MCU internal sensor.
static inline int compatChipTempC() { return (int)temperatureRead(); }
