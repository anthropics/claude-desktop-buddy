#pragma once
#include <stdint.h>

// Drop-in replacements for the M5 RTC type names. Field order/types match
// what main.cpp.upstream's M5.Rtc.GetTime/SetTime/GetDate/SetDate sites
// (and data.h's brace-init) expect — do not reorder.
struct RTC_TimeTypeDef {
    uint8_t Hours;
    uint8_t Minutes;
    uint8_t Seconds;
};

struct RTC_DateTypeDef {
    uint8_t  WeekDay;   // 0=Sun..6=Sat
    uint8_t  Month;     // 1..12
    uint8_t  Date;      // 1..31  (day-of-month)
    uint16_t Year;      // e.g. 2026
};

// Software RTC: baseEpoch + millis() drift, periodically saved to NVS so a
// power-cycle preserves the minute. No battery-backed chip — the bridge's
// {"time":[...]} sync re-anchors us on the next BLE connect.
void rtcBegin();   // load last-saved time from NVS
void rtcLoop();    // call from boardLoop(); persists on minute change

void rtcGetTime(RTC_TimeTypeDef* t);
void rtcGetDate(RTC_DateTypeDef* d);
void rtcSetTime(const RTC_TimeTypeDef* t);
void rtcSetDate(const RTC_DateTypeDef* d);
