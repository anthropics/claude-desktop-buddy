#pragma once

// CYD port: there is no M5StickCPlus library — this file is the shim.
// Upstream firmware code does `#include <M5StickCPlus.h>` and uses a small
// subset of the M5 API. We map those calls onto our HAL so upstream files
// compile unmodified (and pulling in upstream changes stays a clean merge).

#include "hal/board.h"
#include "hal/rtc.h"
#include "hal/power.h"

struct _M5Rtc {
  void SetTime(const RTC_TimeTypeDef* t) const { rtcSetTime(t); }
  void SetDate(const RTC_DateTypeDef* d) const { rtcSetDate(d); }
  void GetTime(RTC_TimeTypeDef* t)       const { rtcGetTime(t); }
  void GetDate(RTC_DateTypeDef* d)       const { rtcGetDate(d); }
};

struct _M5Axp {
  float GetBatVoltage()  const { return powerStatus().mV / 1000.0f; }
  float GetBatCurrent()  const { return (float)powerStatus().mA; }
  float GetVBusVoltage() const { return powerStatus().usb ? 5.0f : 0.0f; }
};

struct _M5Compat {
  _M5Rtc Rtc;
  _M5Axp Axp;
};

extern _M5Compat M5;
