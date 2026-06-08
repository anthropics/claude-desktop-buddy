// Wire-format reference (all lines newline-terminated, sent via bleWrite):
// audio_begin: {"evt":"audio_begin","sr":12000,"frame_raw_bytes":240,"codec":"adpcm_ima"}
// audio:       {"evt":"audio","seq":N,"crc":"HHHHHHHH","data":"<base64-of-240-raw-bytes>"}
// audio_end:   {"evt":"audio_end","seq_total":N,"crc":"HHHHHHHH","reason":"...","wire_p50":12,"wire_p95":13,"wire_p99":14}
// ptt:         {"evt":"ptt","state":"down|up","ts":<millis>}
// ptt_ack:     {"cmd":"ptt_ack","state":"received|ready|error","reason":"..."}  (incoming, host->device)

#include "audio_ble.h"
#include "audio_capture.h"
#include "ble_bridge.h"
#include <Arduino.h>
#include <string.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static const size_t FRAME_BYTES   = 240;   // raw ADPCM bytes per audio line
static const size_t BASE64_CHARS  = 320;   // ceil(240/3)*4 — 240%3==0, no padding
static const size_t JSON_BUF      = 512;   // stack buffer for each bleWrite line
static const size_t RING_LOW_WATER = 16384; // bytes; burst-drain threshold
static const uint32_t EMIT_INTERVAL_MS = 80; // throttle: ~12.5 frames/sec base rate

// bleWrite duration ring: 600 slots × uint16_t. Covers ~24 s at 25 lines/sec.
static const size_t DUR_RING_CAP = 600;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static bool     s_active      = false;
static uint32_t s_seq         = 0;
static uint32_t s_lastEmitMs  = 0;
static uint32_t s_overallCrc  = 0;   // running CRC32 across all raw bytes in session

// bleWrite duration ring
static uint16_t s_durRing[DUR_RING_CAP];
static size_t   s_durHead = 0;   // next write slot (ring is full when wraps)
static size_t   s_durCount = 0;  // total frames logged (capped at DUR_RING_CAP)

// Latest ptt_ack state (Phase D will read this)
static char s_ackState[16]  = "";
static char s_ackReason[64] = "";

// ---------------------------------------------------------------------------
// CRC32 (zlib polynomial 0xEDB88320, reflected)
// ---------------------------------------------------------------------------

static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320u;
            else         crc >>= 1;
        }
    }
    return ~crc;
}

// ---------------------------------------------------------------------------
// Base64 encoder (standard alphabet, no line wrapping, no padding strip)
// 240 % 3 == 0 so output is always exactly 320 chars with no '=' padding.
// ---------------------------------------------------------------------------

// Standard Base64 alphabet (RFC 4648 Table 1)
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode `inLen` bytes from `in` into `out`. `out` must have room for
// at least ceil(inLen/3)*4 + 1 chars (caller provides `out[321]`).
static void base64Encode(const uint8_t* in, size_t inLen, char* out) {
    size_t o = 0;
    for (size_t i = 0; i < inLen; i += 3) {
        uint32_t v = ((uint32_t)in[i] << 16)
                   | ((uint32_t)in[i+1] << 8)
                   |  (uint32_t)in[i+2];
        out[o++] = B64[(v >> 18) & 0x3F];
        out[o++] = B64[(v >> 12) & 0x3F];
        out[o++] = B64[(v >>  6) & 0x3F];
        out[o++] = B64[(v      ) & 0x3F];
    }
    out[o] = '\0';
}

// ---------------------------------------------------------------------------
// bleWrite string helper
// ---------------------------------------------------------------------------

static void bleWriteStr(const char* s) {
    size_t n = strlen(s);
    bleWrite((const uint8_t*)s, n);
}

// ---------------------------------------------------------------------------
// Duration ring helpers
// ---------------------------------------------------------------------------

static void durPush(uint32_t dt) {
    uint16_t v = (dt > 0xFFFF) ? 0xFFFF : (uint16_t)dt;
    s_durRing[s_durHead] = v;
    s_durHead = (s_durHead + 1) % DUR_RING_CAP;
    if (s_durCount < DUR_RING_CAP) s_durCount++;
}

// Simple insertion-sort percentile on a copy of the occupied ring slots.
// n must be <= DUR_RING_CAP.
static uint16_t durPercentile(uint8_t pct) {
    if (s_durCount == 0) return 0;
    uint16_t tmp[DUR_RING_CAP];
    // Copy the last s_durCount entries in chronological order
    size_t count = s_durCount;
    size_t tail = (s_durHead + DUR_RING_CAP - count) % DUR_RING_CAP;
    for (size_t i = 0; i < count; i++) {
        tmp[i] = s_durRing[(tail + i) % DUR_RING_CAP];
    }
    // Insertion sort (small n, on-stack, no heap needed)
    for (size_t i = 1; i < count; i++) {
        uint16_t key = tmp[i];
        size_t j = i;
        while (j > 0 && tmp[j-1] > key) { tmp[j] = tmp[j-1]; j--; }
        tmp[j] = key;
    }
    size_t idx = (size_t)(((uint32_t)pct * (uint32_t)(count - 1)) / 100);
    return tmp[idx];
}

// ---------------------------------------------------------------------------
// Emit one frame from the ring
// ---------------------------------------------------------------------------

static void emitFrame() {
    uint8_t raw[FRAME_BYTES];
    size_t got = audioCaptureRead(raw, FRAME_BYTES);
    if (got < FRAME_BYTES) return;  // shouldn't happen if caller checked fill

    // Per-frame CRC32 over the 240 raw bytes (before base64)
    uint32_t frameCrc = crc32Update(0, raw, FRAME_BYTES);

    // Running overall CRC32 (feed same raw bytes)
    s_overallCrc = crc32Update(s_overallCrc, raw, FRAME_BYTES);

    // Base64-encode into a fixed buffer (320 chars + NUL)
    char b64[BASE64_CHARS + 1];
    base64Encode(raw, FRAME_BYTES, b64);

    // Build JSON envelope into stack buffer
    char buf[JSON_BUF];
    snprintf(buf, sizeof(buf),
             "{\"evt\":\"audio\",\"seq\":%lu,\"crc\":\"%08lx\",\"data\":\"%s\"}\n",
             (unsigned long)s_seq,
             (unsigned long)frameCrc,
             b64);

    // Time the bleWrite call
    uint32_t t0 = millis();
    bleWriteStr(buf);
    durPush(millis() - t0);

    s_seq++;
    s_lastEmitMs = millis();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void audioBleStreamBegin() {
    if (s_active) return;  // idempotent
    s_active     = true;
    s_seq        = 0;
    s_overallCrc = 0;
    s_lastEmitMs = millis();
    s_durHead    = 0;
    s_durCount   = 0;

    char beginBuf[JSON_BUF];
    snprintf(beginBuf, sizeof(beginBuf),
             "{\"evt\":\"audio_begin\",\"sr\":%lu,\"frame_raw_bytes\":%u,\"codec\":\"adpcm_ima\"}\n",
             (unsigned long)audioCaptureSampleRate(), (unsigned)FRAME_BYTES);
    bleWriteStr(beginBuf);
    Serial.println("[audio] stream begin");
}

void audioBleStreamEnd(const char* reason) {
    if (!s_active) return;  // idempotent

    // Drain remaining full frames from ring
    while (audioCaptureFill() >= FRAME_BYTES) {
        emitFrame();
    }

    // Compute percentiles before printing audio_end
    uint16_t p50 = durPercentile(50);
    uint16_t p95 = durPercentile(95);
    uint16_t p99 = durPercentile(99);

    char buf[JSON_BUF];
    snprintf(buf, sizeof(buf),
             "{\"evt\":\"audio_end\",\"seq_total\":%lu,\"crc\":\"%08lx\",\"reason\":\"%s\","
             "\"wire_p50\":%u,\"wire_p95\":%u,\"wire_p99\":%u}\n",
             (unsigned long)s_seq,
             (unsigned long)s_overallCrc,
             reason ? reason : "",
             (unsigned)p50, (unsigned)p95, (unsigned)p99);
    bleWriteStr(buf);

    Serial.printf("[audio] stream end reason=%s seq=%lu crc=%08lx "
                  "wire_p50=%u wire_p95=%u wire_p99=%u\n",
                  reason ? reason : "",
                  (unsigned long)s_seq,
                  (unsigned long)s_overallCrc,
                  (unsigned)p50, (unsigned)p95, (unsigned)p99);

    s_active = false;
}

bool audioBleStreamPump() {
    if (!s_active) return false;

    size_t fill = audioCaptureFill();
    uint32_t now = millis();

    // Emit if: ring has >= 1 frame AND (burst-drain condition OR throttle elapsed)
    bool burst   = fill >= RING_LOW_WATER;
    bool timeout = (now - s_lastEmitMs) >= EMIT_INTERVAL_MS;

    if (fill >= FRAME_BYTES && (burst || timeout)) {
        emitFrame();
        return true;
    }
    return false;
}

void audioBleEmitPtt(const char* state) {
    char buf[JSON_BUF];
    snprintf(buf, sizeof(buf),
             "{\"evt\":\"ptt\",\"state\":\"%s\",\"ts\":%lu}\n",
             state ? state : "",
             (unsigned long)millis());
    bleWriteStr(buf);
    Serial.printf("[audio] ptt %s\n", state ? state : "");
}

bool audioBleStreamActive() {
    return s_active;
}

void audioBleHandlePttAck(const char* state, const char* reason) {
    strncpy(s_ackState,  state  ? state  : "", sizeof(s_ackState)  - 1);
    strncpy(s_ackReason, reason ? reason : "", sizeof(s_ackReason) - 1);
    s_ackState[sizeof(s_ackState)   - 1] = '\0';
    s_ackReason[sizeof(s_ackReason) - 1] = '\0';
    Serial.printf("[audio] ack state=%s reason=%s\n", s_ackState, s_ackReason);
}
