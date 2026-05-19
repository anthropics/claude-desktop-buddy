#include "audio.h"
#include <Arduino.h>

// LEDC channel 2 for audio (channel 0 reserved for backlight PWM in hal/power.cpp).
static const uint8_t  AUDIO_PIN     = 26;
static const uint8_t  AUDIO_CHANNEL = 2;
static const uint32_t AUDIO_BASE_HZ = 1000;
static const uint8_t  AUDIO_RES_BIT = 8;

static uint32_t s_tone_end_ms = 0;

void audioBegin() {
    ledcSetup(AUDIO_CHANNEL, AUDIO_BASE_HZ, AUDIO_RES_BIT);
    ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
    ledcWriteTone(AUDIO_CHANNEL, 0);
    s_tone_end_ms = 0;
}

void boardTone(uint16_t freq_hz, uint16_t duration_ms) {
    if (freq_hz == 0 || duration_ms == 0) {
        ledcWriteTone(AUDIO_CHANNEL, 0);
        s_tone_end_ms = 0;
        return;
    }
    ledcWriteTone(AUDIO_CHANNEL, freq_hz);
    s_tone_end_ms = millis() + duration_ms;
}

void audioLoop() {
    if (s_tone_end_ms != 0 && (int32_t)(millis() - s_tone_end_ms) >= 0) {
        ledcWriteTone(AUDIO_CHANNEL, 0);
        s_tone_end_ms = 0;
    }
}
