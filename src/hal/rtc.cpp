// Software RTC for CYD (no BM8563, no coin cell). Anchor a time_t at boot
// (loaded from NVS, defaulting to 2025-01-01 UTC if none saved) and let
// millis() carry the seconds. rtcLoop() rewrites NVS once per minute so a
// quick reboot loses less than 60 s; longer power-off resets the anchor on
// the next BLE time-sync from the desktop bridge.
//
// All time math is UTC — main.cpp / data.h do the local-zone conversion
// before calling us (see data.h: it builds tm with localtime() then hands
// us back Hours/Minutes/Seconds/etc). We just store-and-return.

#include "rtc.h"
#include <Preferences.h>
#include <Arduino.h>
#include <time.h>

static const char* NVS_NS      = "rtc";
static const char* NVS_KEY     = "epoch";
static const time_t DEFAULT_EPOCH = 1735689600;  // 2025-01-01 00:00:00 UTC

static time_t   baseEpoch    = DEFAULT_EPOCH;
static uint32_t baseMillis   = 0;
static int      lastSavedMin = -1;

static time_t rtcNow() {
    uint32_t elapsed = (millis() - baseMillis) / 1000u;
    return baseEpoch + (time_t)elapsed;
}

static void rtcSaveNow() {
    Preferences prefs;
    if (prefs.begin(NVS_NS, false)) {
        prefs.putULong(NVS_KEY, (uint32_t)rtcNow());
        prefs.end();
    }
}

void rtcBegin() {
    baseMillis = millis();
    Preferences prefs;
    if (prefs.begin(NVS_NS, true)) {
        uint32_t saved = prefs.getULong(NVS_KEY, 0);
        prefs.end();
        baseEpoch = (saved != 0) ? (time_t)saved : DEFAULT_EPOCH;
    } else {
        baseEpoch = DEFAULT_EPOCH;
    }
    struct tm tm_now;
    time_t now = baseEpoch;
    gmtime_r(&now, &tm_now);
    lastSavedMin = tm_now.tm_min;
}

void rtcLoop() {
    time_t now = rtcNow();
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    if (tm_now.tm_min != lastSavedMin) {
        lastSavedMin = tm_now.tm_min;
        rtcSaveNow();
    }
}

void rtcGetTime(RTC_TimeTypeDef* t) {
    if (!t) return;
    time_t now = rtcNow();
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    t->Hours   = (uint8_t)tm_now.tm_hour;
    t->Minutes = (uint8_t)tm_now.tm_min;
    t->Seconds = (uint8_t)tm_now.tm_sec;
}

void rtcGetDate(RTC_DateTypeDef* d) {
    if (!d) return;
    time_t now = rtcNow();
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    d->WeekDay = (uint8_t)tm_now.tm_wday;
    d->Month   = (uint8_t)(tm_now.tm_mon + 1);
    d->Date    = (uint8_t)tm_now.tm_mday;
    d->Year    = (uint16_t)(tm_now.tm_year + 1900);
}

void rtcSetTime(const RTC_TimeTypeDef* t) {
    if (!t) return;
    time_t now = rtcNow();
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    tm_now.tm_hour = t->Hours;
    tm_now.tm_min  = t->Minutes;
    tm_now.tm_sec  = t->Seconds;
    time_t repacked = mktime(&tm_now);
    if (repacked != (time_t)-1) {
        baseEpoch  = repacked;
        baseMillis = millis();
        rtcSaveNow();
        lastSavedMin = tm_now.tm_min;
    }
}

void rtcSetDate(const RTC_DateTypeDef* d) {
    if (!d) return;
    time_t now = rtcNow();
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    tm_now.tm_year = (int)d->Year - 1900;
    tm_now.tm_mon  = (int)d->Month - 1;
    tm_now.tm_mday = d->Date;
    // tm_wday is recomputed by mktime; WeekDay field is ignored on set.
    time_t repacked = mktime(&tm_now);
    if (repacked != (time_t)-1) {
        baseEpoch  = repacked;
        baseMillis = millis();
        rtcSaveNow();
        lastSavedMin = tm_now.tm_min;
    }
}
