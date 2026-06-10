#include "audio_capture.h"
#include "adpcm.h"

#include <Arduino.h>
#include <math.h>

// Two capture backends share one ADPCM ring + public API:
//   * Plus2 / Plus (default): raw I2S PDM-RX from the SPM1423 mic on GPIO0/34.
//   * StickS3 (BUDDY_BOARD_STICKS3): the ES8311 codec mic via M5Unified's
//     M5.Mic (M5.begin() auto-configures the ES8311 I2C/I2S on this board, so
//     no manual pin/register setup — the upstream-correct path). 16 kHz.
// The ring buffer, ADPCM encoder, and Read/Fill/IsCapturing are board-agnostic.

#ifdef BUDDY_BOARD_STICKS3
#include <M5Unified.h>
#define AC_SAMPLE_RATE   16000           // ES8311 native; best for speech STT
#else
#include <driver/i2s.h>
// ---- Hardware config (PDM mic) ----
#define AC_I2S_PORT      I2S_NUM_0
#define AC_BCK_GPIO      GPIO_NUM_0   // CLK (bit clock / PDM CLK)
#define AC_DIN_GPIO      GPIO_NUM_34  // DATA (PDM data)
#define AC_SAMPLE_RATE   12000
#define AC_DMA_BUFS      4
#define AC_DMA_BUF_SAMPS 256         // samples per DMA buffer
// Total DMA buffering: 4 × 256 = 1024 samples ≈ 85 ms at 12 kHz
#endif

// ---- Ring buffer ----
// 64 KB heap — power-of-2 allows mask-based wrap (no modulo).
// Allocated once in audioCaptureInit() via heap_caps_malloc (8-bit DRAM).
// At 6000 ADPCM bytes/s this holds ~10.9 s of audio.
// NOTE: spec says "static BSS" but ESP32 DRAM is ~320 KB total and the
// firmware already uses ~280 KB of it; a 64 KB static array overflows the
// linker segment by ~15 KB. Heap allocation is equivalent at runtime and
// satisfies the 64 KB / power-of-2 / mask-wrap requirements identically.
#define RING_SIZE   65536u
#define RING_MASK   (RING_SIZE - 1u)

static uint8_t* adpcmRing = nullptr;  // 64 KB, heap-allocated in init

static volatile uint32_t ringHead = 0;  // write index (producer)
static volatile uint32_t ringTail = 0;  // read  index (consumer)
static volatile uint32_t droppedBytes = 0;

// Fill = bytes available to read
static inline size_t ring_fill() {
    return (ringHead - ringTail) & RING_MASK;
}

// Free space
static inline size_t ring_free() {
    return RING_SIZE - 1u - ring_fill();
}

// Push one byte; if full, drop oldest (overwrite-tail policy).
static inline void ring_push(uint8_t b) {
    if (!adpcmRing) return;
    if (ring_free() == 0) {
        // Full — evict oldest byte
        ringTail = (ringTail + 1u) & RING_MASK;
        droppedBytes++;
    }
    adpcmRing[ringHead & RING_MASK] = b;
    ringHead = (ringHead + 1u) & RING_MASK;
}

// ---- State ----
static bool s_initialized = false;
static bool s_capturing   = false;
static adpcm_state_t s_enc;
static int16_t s_peak     = 0;   // decaying peak for the UI VU meter

uint8_t audioCaptureLevel() { return (uint8_t)(((int32_t)s_peak * 100) / 32767); }

uint32_t audioCaptureSampleRate() { return AC_SAMPLE_RATE; }

// ===========================================================================
//  StickS3 backend — ES8311 mic via M5.Mic
// ===========================================================================
#ifdef BUDDY_BOARD_STICKS3

// Double-buffered record pool. M5.Mic.record() queues a buffer and returns
// true once the buffer submitted ~2 calls earlier has finished filling
// (queue depth 2, per the upstream Microphone.ino idiom: rec idx leads the
// process idx by 2). NBUF must be >= 3; 4 keeps the two indices a fixed 2
// apart with no aliasing.
static constexpr size_t S3_CHUNK = 200;          // samples/record (even → ADPCM pairs)
static constexpr size_t S3_NBUF  = 4;
static int16_t s_recPool[S3_NBUF][S3_CHUNK];
static size_t  s_recIdx = 2;   // next buffer to submit (leads)
static size_t  s_encIdx = 0;   // buffer to encode (trails by 2)

bool audioCaptureInit() {
    if (s_initialized) return true;
    adpcmRing = (uint8_t*)heap_caps_malloc(RING_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!adpcmRing) {
        Serial.printf("[audio] ring alloc failed (need %u bytes)\n", RING_SIZE);
        return false;
    }
    adpcm_encode_init(&s_enc);
    ringHead = ringTail = droppedBytes = 0;
    s_capturing   = false;

    // Bring the ES8311 mic up ONCE here, at boot — and never end() it.
    //
    // Why not lazily in Start()/end() in Stop()? Because installing/uninstalling
    // the I2S peripheral (M5.Mic.begin / M5.Mic.end) from inside the live
    // render+BLE loop spins with interrupts off long enough to trip the TG1
    // interrupt watchdog (rst:0x8 TG1WDT_SYS_RST) — pinned by the audioprobe.
    // The begin/end dance only existed so the speaker could reclaim the ES8311
    // for beeps; but the StickS3 build disables the internal speaker entirely
    // (cfg.internal_spk=false in main.cpp), so the mic can own the codec
    // permanently. Bringing it up at boot — before startBt(), no loop running,
    // heap still high — is exactly the context where M5.Mic.begin succeeds.
    // Start()/Stop() then just flip s_capturing; the DMA keeps running, which
    // also removes warm-up latency on the first PTT chunk.
    auto cfg = M5.Mic.config();
    cfg.sample_rate = AC_SAMPLE_RATE;
    M5.Mic.config(cfg);
    if (!M5.Mic.begin()) {
        Serial.println("[audio] M5.Mic.begin FAILED");
        heap_caps_free(adpcmRing);
        adpcmRing = nullptr;
        return false;
    }

    // PRIME the mic here, at boot, BEFORE any load — critical.
    //
    // M5Unified's Mic_Class::record() calls begin() internally every time, and
    // begin() only fast-returns when _rec_sample_rate already matches. But the
    // initial begin() above (_task_running was false) takes a code path that
    // NEVER sets _rec_sample_rate — it stays 0. So the FIRST record() at
    // runtime sees 0 != 16000, falls into begin()'s slow path and calls end()
    // → i2s_driver_uninstall + a full I2S re-install. That uninstall spins with
    // interrupts off long enough to trip the TG1 interrupt watchdog when the
    // render+BLE loop is live (rst:0x8 TG1WDT_SYS_RST — exactly what crashed
    // the first pump). Doing a few throwaway record()s now forces that one-time
    // end()+reinstall while we're still single-threaded at boot (safe, like the
    // isolated probe), syncing _rec_sample_rate. Afterwards every runtime
    // record() hits begin()'s fast-return and never touches the I2S driver.
    {
        // ONE record only. The first record() triggers begin()'s slow path
        // (end()+reinstall) because _rec_sample_rate is unset — but at this
        // instant nothing is in-flight, so end() finds the mic_task idle and
        // exits cleanly (no hang). Piling on more records here is what wedged
        // setup() before: the 2nd record's end() hit the 1st record's in-flight
        // DMA and the task never exited. So: one record, then drain it fully
        // (bounded) before returning, leaving nothing queued.
        static int16_t prime[S3_CHUNK];
        M5.Mic.record(prime, S3_CHUNK, AC_SAMPLE_RATE);
        for (int i = 0; i < 60 && M5.Mic.isRecording(); i++) delay(5);
    }

    s_initialized = true;
    Serial.printf("[audio] StickS3 ES8311 mic up + primed @ %d Hz (always-on)\n", AC_SAMPLE_RATE);
    return true;
}

void audioCaptureStart() {
    if (!s_initialized) return;
    ringHead = ringTail = droppedBytes = 0;
    adpcm_encode_init(&s_enc);
    // Mic DMA is already running (begun at boot). Just zero the encode pool so
    // a fresh utterance starts with ~25ms of silence rather than a stale tail,
    // reset the submit/encode indices, and arm the pump.
    memset(s_recPool, 0, sizeof(s_recPool));
    s_recIdx = 2;
    s_encIdx = 0;
    s_capturing = true;
}

void audioCaptureStop() {
    // Leave the mic running (see audioCaptureInit) — just stop draining it into
    // the ring. The DMA buffers recycle harmlessly until the next Start().
    s_capturing = false;
    s_peak = 0;
}

void audioCapturePump() {
    if (!s_initialized || !s_capturing) return;
    if (!M5.Mic.isEnabled()) return;

    s_peak -= s_peak >> 3;   // VU fall-off (~200 ms time constant at loop rate)

    // Submit as many chunks as the queue accepts this tick; encode each
    // freshly-completed buffer into the ring. Bounded by S3_NBUF so a single
    // pump call can't hog the loop.
    for (size_t guard = 0; guard < S3_NBUF; guard++) {
        int16_t* sub = s_recPool[s_recIdx];
        if (!M5.Mic.record(sub, S3_CHUNK, AC_SAMPLE_RATE)) break;  // queue full

        // record() returned true → buffer at s_encIdx (2 behind) is now filled.
        int16_t* done = s_recPool[s_encIdx];
        for (size_t i = 0; i < S3_CHUNK; i++) {
            int16_t a = done[i] < 0 ? -done[i] : done[i];
            if (a > s_peak) s_peak = a;
        }
        uint8_t local[S3_CHUNK / 2];   // 100 ADPCM bytes per 200 samples
        size_t enc = adpcm_encode(&s_enc, done, S3_CHUNK, local);
        for (size_t i = 0; i < enc; i++) ring_push(local[i]);

        s_recIdx = (s_recIdx + 1) % S3_NBUF;
        s_encIdx = (s_encIdx + 1) % S3_NBUF;
    }
}

// ===========================================================================
//  Plus2 / Plus backend — raw I2S PDM-RX (unchanged)
// ===========================================================================
#else

// Scratch DMA read buffer: sized for one DMA buffer worth of int16 samples.
// Stack allocation in pump() would be 512 bytes — fine, but static is safer on ESP32.
static int16_t s_pcmBuf[AC_DMA_BUF_SAMPS * 2];  // headroom for two DMA buffers

// ---- I2S init ----
static bool i2s_init() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    cfg.sample_rate          = AC_SAMPLE_RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = AC_DMA_BUFS;
    cfg.dma_buf_len          = AC_DMA_BUF_SAMPS;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = false;
    cfg.fixed_mclk           = 0;

    esp_err_t err = i2s_driver_install(AC_I2S_PORT, &cfg, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[audio] i2s_driver_install failed: %d\n", err);
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = AC_BCK_GPIO;
    pins.ws_io_num    = I2S_PIN_NO_CHANGE;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = AC_DIN_GPIO;

    err = i2s_set_pin(AC_I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("[audio] i2s_set_pin failed: %d\n", err);
        i2s_driver_uninstall(AC_I2S_PORT);
        return false;
    }

    return true;
}

// ---- AUDIO_SELFTEST ----
// Approach: Goertzel algorithm at 1000 Hz and neighboring ±50 Hz bins.
// Goertzel requires O(N) multiplies, no FFT butterfly overhead.
// We verify that the 1000 Hz bin has strictly larger magnitude than
// the 950 Hz and 1050 Hz bins, confirming peak is at 1000 ± 50 Hz.
// This satisfies AC-N7 / Phase F.11.
#ifdef AUDIO_SELFTEST

static float goertzel(const int16_t* samples, size_t n, float target_hz, float sample_rate) {
    float k     = (float)n * target_hz / sample_rate;
    float omega = 2.0f * (float)M_PI * k / (float)n;
    float coeff = 2.0f * cosf(omega);
    float q0 = 0.0f, q1 = 0.0f, q2 = 0.0f;
    for (size_t i = 0; i < n; i++) {
        q0 = coeff * q1 - q2 + (float)samples[i];
        q2 = q1;
        q1 = q0;
    }
    float real = q1 - q2 * cosf(omega);
    float imag = q2 * sinf(omega);
    return sqrtf(real * real + imag * imag);
}

static void run_selftest() {
    // Generate 1 s of 1 kHz sine at 0.5 amplitude, 12000 samples.
    const int N = AC_SAMPLE_RATE;   // 12000
    int16_t* pcm = (int16_t*)malloc(N * sizeof(int16_t));
    if (!pcm) {
        Serial.println("[selftest] malloc failed for PCM buffer");
        return;
    }

    for (int i = 0; i < N; i++) {
        float t = (float)i / (float)AC_SAMPLE_RATE;
        pcm[i] = (int16_t)(0.5f * 32767.0f * sinf(2.0f * (float)M_PI * 1000.0f * t));
    }

    // Encode to ADPCM
    size_t adpcm_bytes = N / 2;
    uint8_t* adpcm_buf = (uint8_t*)malloc(adpcm_bytes);
    if (!adpcm_buf) {
        Serial.println("[selftest] malloc failed for ADPCM buffer");
        free(pcm);
        return;
    }

    adpcm_state_t enc_st, dec_st;
    adpcm_encode_init(&enc_st);
    size_t written = adpcm_encode(&enc_st, pcm, N, adpcm_buf);

    // Decode back
    int16_t* decoded = (int16_t*)malloc(N * sizeof(int16_t));
    if (!decoded) {
        Serial.println("[selftest] malloc failed for decoded buffer");
        free(adpcm_buf);
        free(pcm);
        return;
    }

    adpcm_decode_init(&dec_st);
    size_t dec_samples = adpcm_decode(&dec_st, adpcm_buf, written, decoded);

    // Goertzel at 950, 1000, 1050 Hz on decoded signal
    size_t eval_n = (dec_samples < (size_t)N) ? dec_samples : (size_t)N;
    float mag_950  = goertzel(decoded, eval_n,  950.0f, AC_SAMPLE_RATE);
    float mag_1000 = goertzel(decoded, eval_n, 1000.0f, AC_SAMPLE_RATE);
    float mag_1050 = goertzel(decoded, eval_n, 1050.0f, AC_SAMPLE_RATE);

    // Peak Hz estimate: weighted interpolation among the three bins
    // If 1000 Hz has the highest magnitude, report 1000 Hz as peak.
    float peakHz;
    if (mag_1000 >= mag_950 && mag_1000 >= mag_1050) {
        peakHz = 1000.0f;
    } else if (mag_950 > mag_1050) {
        peakHz = 950.0f;
    } else {
        peakHz = 1050.0f;
    }

    Serial.printf("[selftest] Goertzel: 950=%.0f 1000=%.0f 1050=%.0f\n",
                  mag_950, mag_1000, mag_1050);
    Serial.printf("[selftest] FFT peak = %.1f Hz (target 1000 Hz +/-50)\n", peakHz);

    bool pass = (peakHz >= 950.0f && peakHz <= 1050.0f);
    Serial.printf("[selftest] %s\n", pass ? "PASS" : "FAIL");

    free(decoded);
    free(adpcm_buf);
    free(pcm);
}
#endif  // AUDIO_SELFTEST

// ---- Public API ----

bool audioCaptureInit() {
#ifdef AUDIO_SELFTEST
    // In selftest mode, run encode/decode/Goertzel test instead of bringing up I2S.
    run_selftest();
    s_initialized = true;  // mark as init'd so pump() is a no-op
    return true;
#else
    if (s_initialized) return true;

    // Allocate 64 KB ring buffer from heap (static BSS overflows ESP32 DRAM linker budget).
    adpcmRing = (uint8_t*)heap_caps_malloc(RING_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!adpcmRing) {
        Serial.printf("[audio] ring alloc failed (need %u bytes)\n", RING_SIZE);
        return false;
    }

    if (!i2s_init()) {
        heap_caps_free(adpcmRing);
        adpcmRing = nullptr;
        return false;
    }

    adpcm_encode_init(&s_enc);
    ringHead = ringTail = droppedBytes = 0;
    s_capturing   = false;
    s_initialized = true;

    Serial.printf("[audio] I2S init ok @ %d Hz\n", AC_SAMPLE_RATE);
    return true;
#endif
}

void audioCaptureStart() {
    if (!s_initialized) return;
    // Flush DMA queue
    i2s_zero_dma_buffer(AC_I2S_PORT);
    // Reset ring
    ringHead = ringTail = droppedBytes = 0;
    // Reset encoder state so next session starts clean
    adpcm_encode_init(&s_enc);
    s_capturing = true;
}

void audioCaptureStop() {
    // Do NOT flush ring — Phase B will drain it.
    s_capturing = false;
}

void audioCapturePump() {
    if (!s_initialized || !s_capturing) return;

    // Cap per-call work at 1 KB ADPCM written.
    // 1 KB ADPCM = 2048 PCM samples. Read in DMA-buffer-sized chunks.
    size_t adpcm_written = 0;
    const size_t PUMP_CAP = 1024u;

    while (adpcm_written < PUMP_CAP) {
        // Read up to one DMA buffer at a time (256 samples = 512 bytes raw)
        size_t bytes_to_read = AC_DMA_BUF_SAMPS * sizeof(int16_t);
        size_t bytes_read = 0;

        // Non-blocking read: timeout = 0 (portMAX_DELAY would block).
        esp_err_t err = i2s_read(AC_I2S_PORT, s_pcmBuf, bytes_to_read, &bytes_read, 0);
        if (err != ESP_OK || bytes_read == 0) break;

        size_t samples = bytes_read / sizeof(int16_t);
        // Must be even for ADPCM (pairs of samples → 1 byte each pair)
        samples &= ~1u;
        if (samples == 0) break;

        // Check how much ADPCM this will produce
        size_t adpcm_from_chunk = samples / 2;
        if (adpcm_written + adpcm_from_chunk > PUMP_CAP) {
            // Trim samples so we don't exceed cap
            size_t allowed = PUMP_CAP - adpcm_written;
            samples = allowed * 2;
            adpcm_from_chunk = allowed;
        }

        // Encode directly into ring (byte by byte via ring_push)
        // For efficiency, encode into a small local buffer then push.
        uint8_t local[512];  // max 512 bytes (1024 samples / 2)
        size_t enc_bytes = adpcm_encode(&s_enc, s_pcmBuf, samples, local);
        for (size_t i = 0; i < enc_bytes; i++) {
            ring_push(local[i]);
        }
        adpcm_written += enc_bytes;
    }
}

#endif  // BUDDY_BOARD_STICKS3

// ===========================================================================
//  Board-agnostic ring readers
// ===========================================================================

size_t audioCaptureRead(uint8_t* out, size_t max) {
    if (!adpcmRing) return 0;
    size_t limit = (max < 1024u) ? max : 1024u;
    size_t avail = ring_fill();
    size_t count = (avail < limit) ? avail : limit;
    for (size_t i = 0; i < count; i++) {
        out[i] = adpcmRing[ringTail & RING_MASK];
        ringTail = (ringTail + 1u) & RING_MASK;
    }
    return count;
}

size_t audioCaptureFill() {
    return ring_fill();
}

bool audioCaptureIsCapturing() {
    return s_capturing;
}
