#pragma once
// Daystats: NVS-backed tally of today's delegation time + tokens.
//
// "Delegation seconds today" = wall seconds during which the desktop
//   reported running > 0 (and the snapshot was fresh). Device-tracked.
// "Tokens today" = whatever the desktop most recently sent in
//   tokens_today. Transient; not persisted (desktop owns it).
// "Lifetime tokens" = device-observed cumulative output tokens. Tracked
//   by deltaing the desktop's `tokens` field across snapshots. Survives
//   reboot. If the desktop restarts (tokens jumps backward) we just
//   re-baseline without decrementing.
//
// Auto-reset: dayKey is YYYYMMDD; checked on every tick. When the local
// date rolls over, delegation seconds reset to 0. Tokens/lifetime never
// reset.
//
// Header-only by design (per claude-integration.md). All state is
// file-static; daystatsInit() loads from NVS, daystatsTick() persists
// throttled to once per 30 s.

#include <Preferences.h>
#include <M5Unified.h>
#include <stdint.h>

// ---- state ----------------------------------------------------------------
static Preferences _dsPrefs;
static uint32_t _dsDelegationSecsToday = 0;
static uint32_t _dsDayKey              = 0;   // YYYYMMDD; 0 = unknown
static uint64_t _dsLifetimeTokens      = 0;
static uint32_t _dsBridgeBaseline      = 0;
static bool     _dsBridgeBaselineSet   = false;
static uint32_t _dsTokensToday         = 0;
static bool     _dsTokensTodaySeen     = false;  // true once a heartbeat arrives
static uint32_t _dsLastTickMs          = 0;
static uint32_t _dsFracMs              = 0;
static uint32_t _dsLastPersistMs       = 0;
static bool     _dsDirty               = false;

// ---- helpers --------------------------------------------------------------
static inline uint32_t _dsDayKeyFromRtc() {
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt)) return 0;
  // Filter pre-sync RTC: real RTC will report year >= 2025 once we've sync'd
  // from the desktop. Pre-sync the BM8563 may report 2000-01-01 or whatever
  // was on the coin cell. Treat anything before 2025 as "not synced yet".
  if (dt.date.year < 2025) return 0;
  return (uint32_t)dt.date.year * 10000u
       + (uint32_t)dt.date.month * 100u
       + (uint32_t)dt.date.date;
}

static inline void _dsPersistIfDirty(uint32_t now, bool force = false) {
  if (!_dsDirty) return;
  if (!force && (now - _dsLastPersistMs) < 30000) return;
  _dsPrefs.putUInt("delsec",  _dsDelegationSecsToday);
  _dsPrefs.putUInt("daykey",  _dsDayKey);
  _dsPrefs.putULong64("lifeT", _dsLifetimeTokens);
  _dsPrefs.putUInt("brgBL",   _dsBridgeBaseline);
  _dsPrefs.putBool("brgBLs",  _dsBridgeBaselineSet);
  _dsLastPersistMs = now;
  _dsDirty = false;
}

// ---- public ---------------------------------------------------------------
inline void daystatsInit() {
  _dsPrefs.begin("daystats", false);
  _dsDelegationSecsToday = _dsPrefs.getUInt("delsec", 0);
  _dsDayKey              = _dsPrefs.getUInt("daykey", 0);
  _dsLifetimeTokens      = _dsPrefs.getULong64("lifeT", 0);
  _dsBridgeBaseline      = _dsPrefs.getUInt("brgBL", 0);
  _dsBridgeBaselineSet   = _dsPrefs.getBool("brgBLs", false);
}

// Call every loop iteration. `running` is sessionsRunning from the latest
// snapshot; `fresh` is whether that snapshot is recent. Accumulator only
// runs while both are true (we don't count delegation time when offline).
inline void daystatsTick(uint8_t running, bool fresh) {
  uint32_t now = millis();

  // Rollover. Cheap (RTC is I2C-bound but the call is small).
  uint32_t key = _dsDayKeyFromRtc();
  if (key && key != _dsDayKey) {
    Serial.printf("[daystats] day rollover %lu -> %lu, reset delegation\n",
                  (unsigned long)_dsDayKey, (unsigned long)key);
    _dsDayKey = key;
    _dsDelegationSecsToday = 0;
    _dsDirty = true;
    _dsPersistIfDirty(now, /*force=*/true);
  }

  // Bootstrap accumulator on first tick (avoids counting boot-to-now as
  // delegation time on the very first call).
  if (_dsLastTickMs == 0) { _dsLastTickMs = now; return; }

  if (running > 0 && fresh) {
    uint32_t elapsedMs = (now - _dsLastTickMs) + _dsFracMs;
    uint32_t addSecs   = elapsedMs / 1000;
    _dsFracMs          = elapsedMs % 1000;
    if (addSecs) {
      _dsDelegationSecsToday += addSecs;
      _dsDirty = true;
    }
  } else {
    _dsFracMs = 0;  // pause: drop the partial-second tail
  }
  _dsLastTickMs = now;

  _dsPersistIfDirty(now);
}

// Called from the heartbeat parser when `tokens` (cumulative since desktop
// start) is present. Track deltas, rebaseline on backward jumps.
inline void daystatsOnBridgeTokens(uint32_t bridgeTokens) {
  if (!_dsBridgeBaselineSet) {
    _dsBridgeBaseline = bridgeTokens;
    _dsBridgeBaselineSet = true;
    _dsDirty = true;
    return;
  }
  if (bridgeTokens >= _dsBridgeBaseline) {
    uint32_t delta = bridgeTokens - _dsBridgeBaseline;
    if (delta) {
      _dsLifetimeTokens += delta;
      _dsDirty = true;
    }
  } else {
    Serial.printf("[daystats] bridge restart (%lu -> %lu), rebaseline\n",
                  (unsigned long)_dsBridgeBaseline, (unsigned long)bridgeTokens);
    _dsDirty = true;
  }
  _dsBridgeBaseline = bridgeTokens;
}

inline void daystatsOnTokensToday(uint32_t tt) {
  _dsTokensToday = tt;
  _dsTokensTodaySeen = true;
  // Invariant: lifetime is a superset of today, so lifetime >= today must
  // always hold. On a fresh device this floor is what makes day-1 coherent
  // (otherwise we'd show "873K today / 0 lifetime" until the next bridge
  // delta trickled in). Lifetime never decreases, so post-midnight when
  // today resets, this is a no-op.
  if (tt > _dsLifetimeTokens) {
    _dsLifetimeTokens = tt;
    _dsDirty = true;
  }
}

inline uint32_t daystatsDelegationSecsToday() { return _dsDelegationSecsToday; }
inline uint32_t daystatsTokensToday()         { return _dsTokensToday; }
inline bool     daystatsTokensTodaySeen()     { return _dsTokensTodaySeen; }
inline uint64_t daystatsLifetimeTokens()      { return _dsLifetimeTokens; }
inline uint32_t daystatsDayKey()              { return _dsDayKey; }

// Pretty-formatters used by the renderer.
//
// fmtDuration: "0s" / "47s" / "12m" / "1h 23m"
// fmtTokens:   "0" / "847" / "47K" / "3.2M" / "12.4M"
//   - Compact below 10K: 4521 -> "4.5K"
//   - 10K..1M: "47K"
//   - 1M+: "3.2M"
inline void daystatsFmtDuration(char* out, size_t cap, uint32_t secs) {
  if (secs < 60) {
    snprintf(out, cap, "%us", (unsigned)secs);
  } else if (secs < 3600) {
    snprintf(out, cap, "%um", (unsigned)(secs / 60));
  } else {
    uint32_t h = secs / 3600;
    uint32_t m = (secs % 3600) / 60;
    snprintf(out, cap, "%uh %um", (unsigned)h, (unsigned)m);
  }
}

inline void daystatsFmtTokens(char* out, size_t cap, uint64_t n) {
  if (n < 1000) {
    snprintf(out, cap, "%llu", (unsigned long long)n);
  } else if (n < 10000) {
    // 4521 -> "4.5K"
    snprintf(out, cap, "%llu.%lluK",
             (unsigned long long)(n / 1000),
             (unsigned long long)((n % 1000) / 100));
  } else if (n < 1000000) {
    // 47200 -> "47K"
    snprintf(out, cap, "%lluK", (unsigned long long)(n / 1000));
  } else {
    // 3210000 -> "3.2M"
    snprintf(out, cap, "%llu.%lluM",
             (unsigned long long)(n / 1000000),
             (unsigned long long)((n % 1000000) / 100000));
  }
}
