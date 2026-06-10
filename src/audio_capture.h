#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Audio capture module — I2S0 PDM-RX, 12 kHz mono 16-bit → IMA ADPCM ring buffer.
//
// Hardware: M5StickC Plus SPM1423 PDM microphone
//   BCK (CLK) = GPIO 0
//   DIN (DATA) = GPIO 34
//
// Ring buffer: 64 KB BSS (65536 bytes), power-of-2 for mask-based wrap.
// ADPCM rate: 12000 samples/s × 4 bits/sample = 6000 bytes/s ≈ 10.9 s ring capacity.

// Call from setup() after M5.Imu.Init(). Returns true on success.
bool audioCaptureInit();

// Capture sample rate (Hz). Plus2 PDM path = 12000; StickS3 ES8311 = 16000.
// audio_ble.cpp reports this in the audio_begin envelope so the daemon decodes
// at the right rate.
uint32_t audioCaptureSampleRate();

// PTT down — flush DMA, reset ring head/tail, begin capturing.
void audioCaptureStart();

// PTT up — stop capturing; ring is NOT flushed (Phase B drains it).
void audioCaptureStop();

// Copy up to min(max, 1024) ADPCM bytes from ring tail into out.
// Returns bytes copied (0 if ring empty or not capturing).
size_t audioCaptureRead(uint8_t* out, size_t max);

// Returns current ring fill in ADPCM bytes.
size_t audioCaptureFill();

// Call every loop() iteration — drains DMA, encodes, appends to ring.
// Non-blocking (I2S read timeout = 0). Caps per-call work at 1 KB ADPCM written.
void audioCapturePump();

// Returns true while a capture session is active (after Start, before Stop).
bool audioCaptureIsCapturing();

// Decaying peak level of the most recent captured audio, 0..100 (% of full
// scale). Drives the on-screen VU meter; 100 ≈ clipping. Always 0 on the
// Plus2 PDM backend (no UI consumer there).
uint8_t audioCaptureLevel();
