#pragma once
#include <M5StickCPlus.h>

// State-of-charge estimation for the M5StickC Plus's 120 mAh LiPo cell.
//
// Voltage-only SoC is fundamentally fragile here:
//
//   - The 1S LiPo OCV-SOC curve is nearly flat across the middle 30–80%
//     band. A few mV of measurement noise = several percentage points.
//   - BLE TX bursts and LCD updates yank the measured voltage by tens to
//     low-hundreds of mV via internal resistance. The same actual SoC
//     reads systematically lower while busy.
//   - **Most importantly**: voltage measured during charging is inflated
//     by charge polarization. Unplug USB and voltage relaxes 50–100 mV.
//     The charging-time pct and the just-unplugged pct describe the same
//     physical battery state but read 10–15 percentage points apart.
//
// This module fixes the unplug step-down by switching from voltage-only to
// **coulomb counting**: AXP192 has a hardware coulomb counter; we read its
// running mAh in/out and convert to SoC%.
//
//   1. At boot, anchor SoC% from the OCV table using the median of the
//      first 30 voltage samples (load is low at startup, so this is the
//      least-noisy moment to take an OCV reading).
//   2. Anchor the coulomb counter value at that same moment.
//   3. SoC_now = anchor_pct + (coulomb_now - coulomb_anchor) / 120 mAh × 100
//   4. Re-anchor whenever we see "fully charged" (V > 4.10 and charge
//      current < 30 mA) so drift gets corrected during normal use.
//
// Smoothing on top: a small ring buffer of derived SoC values, take the
// median. Catches transient AXP192 register glitches without needing a
// long voltage window for noise rejection (the coulomb counter does that
// part already).

namespace battery {

constexpr uint32_t SAMPLE_INTERVAL_MS  = 1000;   // 1 Hz
constexpr uint8_t  V_WINDOW            = 30;     // 30 voltage samples for OCV anchor
constexpr uint8_t  PCT_WINDOW          = 8;      // smooth the derived pct lightly
constexpr float    BAT_CAPACITY_MAH    = 120.0f; // M5StickC Plus internal cell

// Re-anchor thresholds: stable, charged, low draw → battery is genuinely full.
constexpr float    FULL_VOLTAGE        = 4.10f;
constexpr float    FULL_CURRENT_MA     = 30.0f;

inline float    _vSamples[V_WINDOW];
inline uint8_t  _vCount = 0;
inline uint8_t  _vHead  = 0;

inline int      _pctSamples[PCT_WINDOW];
inline uint8_t  _pctCount = 0;
inline uint8_t  _pctHead  = 0;

inline uint32_t _nextSampleMs = 0;

inline bool     _anchored      = false;
inline float    _anchorPct     = 50.0f;     // SoC at the moment we anchored
inline float    _anchorCoulomb = 0.0f;      // GetCoulombData() at anchor time

// LiPo OCV-SOC piecewise table (used only at the OCV anchoring moment).
struct _Pt { float v; int pct; };
static const _Pt _OCV[] = {
    {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.90f, 70},
    {3.80f, 60},  {3.75f, 50}, {3.70f, 40}, {3.65f, 30},
    {3.60f, 20},  {3.50f, 10}, {3.30f,  5}, {3.00f,  0},
};
inline int _ocvToPct(float v) {
    const int N = sizeof(_OCV) / sizeof(_OCV[0]);
    if (v >= _OCV[0].v) return _OCV[0].pct;
    if (v <= _OCV[N-1].v) return _OCV[N-1].pct;
    for (int i = 0; i < N - 1; i++) {
        if (v <= _OCV[i].v && v >= _OCV[i+1].v) {
            float span_v = _OCV[i].v   - _OCV[i+1].v;
            float span_p = (float)(_OCV[i].pct - _OCV[i+1].pct);
            float frac   = (v - _OCV[i+1].v) / span_v;
            int pct = (int)(_OCV[i+1].pct + frac * span_p + 0.5f);
            if (pct < 0) return 0;
            if (pct > 100) return 100;
            return pct;
        }
    }
    return 0;
}

inline float _vMedian() {
    if (_vCount == 0) return 4.20f;
    float buf[V_WINDOW];
    for (uint8_t i = 0; i < _vCount; i++) buf[i] = _vSamples[i];
    for (uint8_t i = 1; i < _vCount; i++) {
        float k = buf[i]; int j = i - 1;
        while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; }
        buf[j+1] = k;
    }
    return buf[_vCount / 2];
}

inline int _pctMedian() {
    if (_pctCount == 0) return 50;
    int buf[PCT_WINDOW];
    for (uint8_t i = 0; i < _pctCount; i++) buf[i] = _pctSamples[i];
    for (uint8_t i = 1; i < _pctCount; i++) {
        int k = buf[i]; int j = i - 1;
        while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; }
        buf[j+1] = k;
    }
    return buf[_pctCount / 2];
}

inline void _pushVoltage(float v) {
    _vSamples[_vHead] = v;
    _vHead = (_vHead + 1) % V_WINDOW;
    if (_vCount < V_WINDOW) _vCount++;
}
inline void _pushPct(int pct) {
    _pctSamples[_pctHead] = pct;
    _pctHead = (_pctHead + 1) % PCT_WINDOW;
    if (_pctCount < PCT_WINDOW) _pctCount++;
}

inline void _setAnchor(float pct) {
    _anchorPct     = pct;
    _anchorCoulomb = M5.Axp.GetCoulombData();
    _anchored      = true;
}

// Compute SoC from coulomb count if anchored, else fall back to OCV.
inline int _computeSoC() {
    if (_anchored) {
        float delta_mAh = M5.Axp.GetCoulombData() - _anchorCoulomb;
        float pct = _anchorPct + (delta_mAh / BAT_CAPACITY_MAH) * 100.0f;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return (int)(pct + 0.5f);
    }
    return _ocvToPct(_vMedian());
}

// Call once per main-loop tick. Internal 1 Hz throttle keeps cost negligible.
inline void poll() {
    uint32_t now = millis();
    if (_nextSampleMs != 0 && (int32_t)(now - _nextSampleMs) < 0) return;
    _nextSampleMs = now + SAMPLE_INTERVAL_MS;

    float v = M5.Axp.GetBatVoltage();
    _pushVoltage(v);

    // First-time anchor: once we have a full voltage window, use its
    // median as the OCV reading and freeze SoC against the coulomb counter.
    if (!_anchored && _vCount >= V_WINDOW) {
        _setAnchor((float)_ocvToPct(_vMedian()));
    }

    // Re-anchor whenever the battery is genuinely full (this corrects any
    // accumulated coulomb-counter drift, common over weeks of use).
    float current = M5.Axp.GetBatCurrent();
    if (v >= FULL_VOLTAGE && fabsf(current) < FULL_CURRENT_MA) {
        _setAnchor(100.0f);
    }

    _pushPct(_computeSoC());
}

inline int percent() {
    if (_pctCount == 0) {
        // Pre-sample fallback; caller likely shouldn't be asking this early.
        return _ocvToPct(M5.Axp.GetBatVoltage());
    }
    return _pctMedian();
}

inline void begin() {
    M5.Axp.EnableCoulombcounter();
}

} // namespace battery
