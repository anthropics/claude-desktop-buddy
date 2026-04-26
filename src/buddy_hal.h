#pragma once
// Thin hardware abstraction over the M5 SDK so main.cpp doesn't need
// #ifdefs at every call site. StickC Plus (AXP192/MPU6886/BM8563/Beep)
// and StickS3 (M5PM1/BMI270/no RTC/Speaker codec) have very different
// peripheral surfaces but the same logical operations.

#include "buddy_common.h"   // pulls in M5StickCPlus.h or M5Unified.h
#include <stdint.h>

struct BuddyTime { uint8_t hours, minutes, seconds; };
struct BuddyDate { uint16_t year; uint8_t month, date, weekDay; };

// Brightness 0..100 (percent). Some PMICs only honor coarse steps.
inline void buddyHalSetBrightness(uint8_t pct) {
#ifdef BUDDY_TARGET_STICKS3
  // M5GFX takes 0..255.
  M5.Display.setBrightness((uint8_t)((uint16_t)pct * 255 / 100));
#else
  M5.Axp.ScreenBreath(20 + (pct * 80) / 100);  // 20..100 native range
#endif
}

inline void buddyHalScreenOn(bool on) {
#ifdef BUDDY_TARGET_STICKS3
  if (on) { M5.Display.wakeup(); M5.Display.setBrightness(192); }
  else    { M5.Display.sleep();  M5.Display.setBrightness(0);   }
#else
  M5.Axp.SetLDO2(on);
#endif
}

inline void buddyHalPowerOff() {
#ifdef BUDDY_TARGET_STICKS3
  M5.Power.powerOff();
#else
  M5.Axp.PowerOff();
#endif
}

// Returns the AXP power-button event (0x02 = short press) on AXP192.
// On StickS3 the side button is BtnPWR via M5Unified; we report 0x02
// when it was just clicked.
inline uint8_t buddyHalPwrBtnPress() {
#ifdef BUDDY_TARGET_STICKS3
  return M5.BtnPWR.wasClicked() ? 0x02 : 0;
#else
  return M5.Axp.GetBtnPress();
#endif
}

inline float buddyHalVBus() {
#ifdef BUDDY_TARGET_STICKS3
  return M5.Power.getVBUSVoltage() / 1000.0f;  // mV → V
#else
  return M5.Axp.GetVBusVoltage();
#endif
}

inline float buddyHalVBat() {
#ifdef BUDDY_TARGET_STICKS3
  return M5.Power.getBatteryVoltage() / 1000.0f;
#else
  return M5.Axp.GetBatVoltage();
#endif
}

inline float buddyHalIBat() {
#ifdef BUDDY_TARGET_STICKS3
  return (float)M5.Power.getBatteryCurrent();
#else
  return M5.Axp.GetBatCurrent();
#endif
}

// AXP192 internal temperature; no equivalent on M5PM1.
inline int buddyHalAxpTempC() {
#ifdef BUDDY_TARGET_STICKS3
  return -1;
#else
  return (int)M5.Axp.GetTempInAXP192();
#endif
}

inline void buddyHalImuInit() {
#ifdef BUDDY_TARGET_STICKS3
  // M5.begin() initializes the IMU on M5Unified; nothing to do.
#else
  M5.Imu.Init();
#endif
}

inline void buddyHalGetAccel(float* x, float* y, float* z) {
#ifdef BUDDY_TARGET_STICKS3
  M5.Imu.getAccel(x, y, z);
#else
  M5.Imu.getAccelData(x, y, z);
#endif
}

inline void buddyHalBeepBegin() {
#ifdef BUDDY_TARGET_STICKS3
  M5.Speaker.begin();
  M5.Speaker.setVolume(128);
#else
  M5.Beep.begin();
#endif
}

inline void buddyHalTone(uint16_t freq, uint16_t durMs) {
#ifdef BUDDY_TARGET_STICKS3
  M5.Speaker.tone(freq, durMs);
#else
  M5.Beep.tone(freq, durMs);
#endif
}

inline void buddyHalBeepUpdate() {
#ifdef BUDDY_TARGET_STICKS3
  // M5.Speaker is non-blocking + self-managed; no update tick needed.
#else
  M5.Beep.update();
#endif
}

// StickS3 has no hardware RTC and NTP is deferred — return false so the
// clock view can render a placeholder.
inline bool buddyHalRtcAvailable() {
#ifdef BUDDY_TARGET_STICKS3
  return false;
#else
  return true;
#endif
}

inline void buddyHalGetTime(BuddyTime* t) {
#ifdef BUDDY_TARGET_STICKS3
  t->hours = t->minutes = t->seconds = 0;
#else
  RTC_TimeTypeDef tm; M5.Rtc.GetTime(&tm);
  t->hours = tm.Hours; t->minutes = tm.Minutes; t->seconds = tm.Seconds;
#endif
}

inline void buddyHalGetDate(BuddyDate* d) {
#ifdef BUDDY_TARGET_STICKS3
  d->year = 0; d->month = 1; d->date = 1; d->weekDay = 0;
#else
  RTC_DateTypeDef dt; M5.Rtc.GetDate(&dt);
  d->year = dt.Year; d->month = dt.Month; d->date = dt.Date; d->weekDay = dt.WeekDay;
#endif
}

// Bridge pushes wall-clock time over BLE; on StickS3 there's no RTC to
// store it in (NTP/software-clock deferred), so this is a no-op.
inline void buddyHalSetTimeDate(const BuddyTime& t, const BuddyDate& d) {
#ifdef BUDDY_TARGET_STICKS3
  (void)t; (void)d;
#else
  RTC_TimeTypeDef tm = { t.hours, t.minutes, t.seconds };
  RTC_DateTypeDef dt = { d.weekDay, d.month, d.date, d.year };
  M5.Rtc.SetTime(&tm);
  M5.Rtc.SetDate(&dt);
#endif
}
