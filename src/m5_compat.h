// Plus → Plus2 (M5Unified) compatibility shim.
//
// The original buddy firmware was written against m5stack/M5StickCPlus
// (with AXP192 PMIC, M5.Beep buzzer driver, RTC_TimeTypeDef structs, etc.).
// M5StickC Plus2 dropped the AXP192, replaced the buzzer path, and the
// codebase moves to M5Unified which uses different APIs.
//
// This header defines lightweight wrappers so the original call sites stay
// readable without #ifdef noise everywhere. main.cpp keeps `M5.Axp.X` style
// calls (mapped to `axpX` free functions here) instead of conditional code.

#pragma once
#include <M5Unified.h>

// ===== RTC type aliases (Plus had Hours/Minutes/Seconds; M5Unified uses
//       hours/minutes/seconds in m5::rtc_datetime_t). Provide a Plus-shaped
//       struct so existing field access compiles. =====
struct RTC_TimeTypeDef {
  uint8_t Hours;
  uint8_t Minutes;
  uint8_t Seconds;
};

struct RTC_DateTypeDef {
  uint8_t WeekDay;  // 0=Sun..6=Sat
  uint8_t Month;    // 1..12
  uint8_t Date;     // 1..31
  uint16_t Year;
};

// ===== Power / display ===== //
inline void axpScreenBreath(uint8_t level_0_100) {
  // Plus took 0-100 (with 0 dim), Plus2 LCD wants 0-255.
  uint16_t v = (uint16_t)level_0_100 * 255 / 100;
  if (v > 255) v = 255;
  M5.Display.setBrightness((uint8_t)v);
}
inline void axpSetLDO2(bool on) {
  // On Plus, LDO2 fed the LCD; toggling it was a hard backlight cut. On
  // Plus2 the closest equivalent is sleep/wakeup.
  if (on) M5.Display.wakeup();
  else    M5.Display.sleep();
}
inline void axpPowerOff() { M5.Power.powerOff(); }

// Plus returned volts as float; Plus2 returns mV as int. Keep the old
// volts-as-float contract so display formatting (V*1000 patterns) works.
inline float axpGetBatVoltage() {
  return M5.Power.getBatteryVoltage() / 1000.0f;
}
inline float axpGetBatCurrent() {
  // Plus2 BMI270 path may not expose battery current; return 0 if unavailable.
  // (M5.Power.getBatteryCurrent() exists on M5Unified but stubs to 0 on
  // boards without per-rail current sense.)
  return 0.0f;
}
// Plus exposed VBus voltage; Plus2 doesn't surface it. Approximate: when
// charging is detected, claim 5.0V; otherwise 0.
inline float axpGetVBusVoltage() {
  return M5.Power.isCharging() == m5::Power_Class::is_charging ? 5.0f : 0.0f;
}
inline int axpGetTempInAXP192() { return 0; }  // not available on Plus2

// Plus's GetBtnPress: 0=none, 1=long, 2=short. We only used "==0x02" (short
// click), which maps to BtnPWR.wasClicked() on Plus2.
inline uint8_t axpGetBtnPress() {
  return M5.BtnPWR.wasClicked() ? 0x02 : 0x00;
}

// ===== Buzzer ===== //
inline void beepTone(uint16_t freq, uint16_t dur) {
#ifdef BUDDY_BOARD_STICKS3
  // ISOLATION: StickS3 reboots (TG1 watchdog) on any button beep. Disabling IMU
  // (app + internal) did NOT help, so the culprit is the ES8311 speaker access
  // itself. No-op beep to confirm + give a stable device.
  (void)freq; (void)dur;
#else
  M5.Speaker.tone(freq, dur);
#endif
}
inline void beepBegin() { /* M5.begin handles it */ }
inline void beepUpdate() { /* not needed on M5Unified */ }

// ===== IMU ===== //
// Plus had M5.Imu.Init() and getAccelData. M5Unified handles init in begin();
// expose a no-op + the new getAccel signature with the Plus parameter order.
inline void imuInit() { /* handled by M5.begin() */ }
inline void imuGetAccel(float* ax, float* ay, float* az) {
  M5.Imu.getAccel(ax, ay, az);
}

// ===== RTC ===== //
inline void rtcGetTime(RTC_TimeTypeDef* t) {
  m5::rtc_time_t rt;
  M5.Rtc.getTime(&rt);
  t->Hours = rt.hours;
  t->Minutes = rt.minutes;
  t->Seconds = rt.seconds;
}
inline void rtcGetDate(RTC_DateTypeDef* d) {
  m5::rtc_date_t rd;
  M5.Rtc.getDate(&rd);
  d->WeekDay = rd.weekDay;
  d->Month = rd.month;
  d->Date = rd.date;
  d->Year = rd.year;
}
inline void rtcSetTime(const RTC_TimeTypeDef* t) {
  m5::rtc_time_t rt{ t->Hours, t->Minutes, t->Seconds };
  M5.Rtc.setTime(&rt);
}
inline void rtcSetDate(const RTC_DateTypeDef* d) {
  m5::rtc_date_t rd{ d->WeekDay, d->Month, d->Date, d->Year };
  M5.Rtc.setDate(&rd);
}
