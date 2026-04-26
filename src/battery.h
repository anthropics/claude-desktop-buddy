#pragma once
#include <M5StickCPlus.h>

// Battery state-of-charge for M5StickC Plus.
//
// The default approach in this firmware was `pct = (vBat - 3200) / 10`, a
// linear voltage→% mapping. That produces wildly fluctuating readings:
//   - LiPo voltage drops several hundred mV under typical BLE+LCD load,
//     so the same SoC reads ~20% lower while busy than while idle.
//   - The real LiPo OCV curve is far from linear; it's nearly flat in the
//     middle 30–80% band and steep at the extremes.
//
// This replacement does two things:
//   1. Sample voltage at 1 Hz into a ring buffer; report the median of the
//      last 30 samples to filter transient load dips.
//   2. Map the smoothed voltage through a piecewise OCV-SOC table sized for
//      a typical 1S LiPo (the M5StickC Plus's internal 120 mAh cell).
//
// Result: instead of jumping ±20% in seconds, the SoC moves slowly and
// monotonically except across genuine charge/discharge transitions.

namespace battery {

constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;   // 1 Hz
constexpr uint8_t  WINDOW = 30;                 // 30 samples = 30 s of history

inline float _samples[WINDOW];
inline uint8_t _count = 0;
inline uint8_t _head  = 0;
inline uint32_t _nextSampleMs = 0;

inline void _push(float v) {
    _samples[_head] = v;
    _head = (_head + 1) % WINDOW;
    if (_count < WINDOW) _count++;
}

inline float _median() {
    if (_count == 0) return 4.20f;
    float buf[WINDOW];
    for (uint8_t i = 0; i < _count; i++) buf[i] = _samples[i];
    // insertion sort — n ≤ 30
    for (uint8_t i = 1; i < _count; i++) {
        float k = buf[i]; int j = i - 1;
        while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; }
        buf[j+1] = k;
    }
    return buf[_count / 2];
}

// Piecewise linear interpolation through an empirical 1S LiPo OCV curve.
// Voltage in volts; returns 0..100.
inline int _ocvToPct(float v) {
    struct Pt { float v; int pct; };
    // Coarse but well-behaved for a desk pet — exact SoC isn't critical.
    static const Pt T[] = {
        {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.90f, 70},
        {3.80f, 60},  {3.75f, 50}, {3.70f, 40}, {3.65f, 30},
        {3.60f, 20},  {3.50f, 10}, {3.30f,  5}, {3.00f,  0},
    };
    static const int N = sizeof(T) / sizeof(T[0]);
    if (v >= T[0].v) return T[0].pct;
    if (v <= T[N-1].v) return T[N-1].pct;
    for (int i = 0; i < N - 1; i++) {
        if (v <= T[i].v && v >= T[i+1].v) {
            float span_v = T[i].v   - T[i+1].v;
            float span_p = (float)(T[i].pct - T[i+1].pct);
            float frac   = (v - T[i+1].v) / span_v;
            int pct = (int)(T[i+1].pct + frac * span_p + 0.5f);
            if (pct < 0) return 0;
            if (pct > 100) return 100;
            return pct;
        }
    }
    return 0;
}

// Call once per main-loop tick. Internal 1 Hz throttle keeps cost negligible.
inline void poll() {
    uint32_t now = millis();
    if (_nextSampleMs == 0 || (int32_t)(now - _nextSampleMs) >= 0) {
        _push(M5.Axp.GetBatVoltage());
        _nextSampleMs = now + SAMPLE_INTERVAL_MS;
    }
}

// Smoothed state-of-charge in 0..100. Uses last_known_good before enough
// samples have accumulated (first second after boot).
inline int percent() {
    if (_count == 0) {
        return _ocvToPct(M5.Axp.GetBatVoltage());
    }
    return _ocvToPct(_median());
}

} // namespace battery
