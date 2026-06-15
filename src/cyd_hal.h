#pragma once
#include <Arduino.h>
// LittleFS must precede TFT_eSPI so its <FS.h> wins the include-guard race
// and `using fs::File` lands in the global namespace before TFT_eSPI.h pulls
// in any conflicting FS headers.
#include <LittleFS.h>
#include <TFT_eSPI.h>
// XPT2046 touch is on a SEPARATE VSPI bus (CLK=25, MISO=39, MOSI=32, CS=33).
// TFT_eSPI has no TOUCH_CS defined, so it never touches pin 33.
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// Color constants provided by M5StickCPlus.h but not by TFT_eSPI directly
#ifndef GREEN
  #define GREEN  0x07E0
#endif
#ifndef RED
  #define RED    0xF800
#endif

// ─── CYD (ESP32-2432S028R) pin layout ───────────────────────────────────────
#define CYD_LED_R     16   // RGB LED red channel, active-low
#define CYD_LED_G     17   // RGB LED green channel, active-low
#define CYD_LED_B      4   // RGB LED blue channel, active-low
#define CYD_BTN_A      0   // BOOT button, active-low (only physical button)
#define CYD_BL_PIN    21   // display backlight, active-high via transistor

// Touch XPT2046 — on VSPI, separate from display HSPI
#define CYD_TOUCH_CS   33
#define CYD_TOUCH_IRQ  36
#define CYD_TOUCH_CLK  25
#define CYD_TOUCH_MISO 39
#define CYD_TOUCH_MOSI 32

// Set to 1 to print raw touch coords to Serial and show them on the button bar.
#define CYD_TOUCH_DEBUG 0

// Raw ADC midpoint for left/right split (0-4095 range).
#define CYD_TOUCH_MID  2048
// Raw ADC y threshold below which touches are ignored (above the button bar).
// Button bar is screen y=240-319; empirical raw y for that region is ~3000+.
#define CYD_BTN_Y_MIN  2700
// Minimum pressure (z) to count as a valid touch.
#define CYD_TOUCH_Z    200

// 135×240 sprite at the top of the 240×320 screen; 80px button bar below.
#define CYD_SPR_X       52
#define CYD_SPR_Y        0
#define CYD_BTN_BAR_Y  240   // y where the virtual button bar begins
#define CYD_BTN_BAR_H   80   // height of the virtual button bar

// ─── Forward declaration of the display object ──────────────────────────────
extern TFT_eSPI _cyd_tft;

// ─── Combined GPIO + touch-zone button ──────────────────────────────────────
// BtnA: physical GPIO 0 OR tap in the left half of screen (raw x < CYD_TOUCH_MID).
// BtnB: tap in the right half of screen (raw x >= CYD_TOUCH_MID).
// Pass pin=0xFF to make a touch-only button.
// Zones are in raw XPT2046 ADC coordinates (0-4095).
class CydCombinedButton {
  uint8_t  _pin;
  uint16_t _zx0, _zy0, _zx1, _zy1;
  bool     _cur = false, _prev = false;
  uint32_t _pressedAt = 0;
  bool     _longFired = false;
public:
  CydCombinedButton(uint8_t pin, uint16_t zx0, uint16_t zy0, uint16_t zx1, uint16_t zy1)
    : _pin(pin), _zx0(zx0), _zy0(zy0), _zx1(zx1), _zy1(zy1) {}

  void begin() { if (_pin != 0xFF) pinMode(_pin, INPUT_PULLUP); }

  void update(bool touched, uint16_t tx, uint16_t ty) {
    bool gpio  = (_pin != 0xFF) && (digitalRead(_pin) == LOW);
    bool zone  = touched && tx >= _zx0 && tx <= _zx1 && ty >= _zy0 && ty <= _zy1;
    _prev = _cur;
    _cur  = gpio || zone;
    if (!_prev && _cur) { _pressedAt = millis(); _longFired = false; }
    if (!_cur) _longFired = false;
  }

  bool isPressed()   const { return _cur; }
  bool wasReleased() const { return _prev && !_cur; }
  bool wasPressed()  const { return !_prev && _cur; }
  bool pressedFor(uint32_t ms) {
    if (_cur && !_longFired && (millis() - _pressedAt) >= ms) {
      _longFired = true;
      return true;
    }
    return false;
  }
};

// ─── AXP192 stub (no power management chip on CYD) ──────────────────────────
// Backlight PWM uses LEDC channel 0 at 5 kHz, 8-bit resolution.
// ScreenBreath accepts 0-100; values mirror M5StickCPlus range (8-100 typical).
#define CYD_BL_LEDC_CH  0
struct CydAxp {
  void ScreenBreath(int v) {
    // clamp and map 0-100 → 0-255
    if (v < 0) v = 0; if (v > 100) v = 100;
    ledcWrite(CYD_BL_LEDC_CH, (v * 255) / 100);
  }
  // SetLDO2 controls the backlight: false=off, true=full (applyBrightness follows)
  void  SetLDO2(bool on) { ledcWrite(CYD_BL_LEDC_CH, on ? 255 : 0); }
  void  PowerOff() { esp_restart(); }
  float GetVBusVoltage() { return 5.0f; }   // always USB-powered
  float GetBatVoltage()  { return 0.0f; }
  float GetBatCurrent()  { return 0.0f; }
  float GetTempInAXP192(){ return 0.0f; }
  int   GetBtnPress()    { return 0; }      // no power button
};

// ─── IMU stub (no accelerometer on CYD) ─────────────────────────────────────
// Z=+1 simulates device upright so isFaceDown() always returns false.
struct CydImu {
  int  Init() { return 0; }
  void getAccelData(float* x, float* y, float* z) { *x=0; *y=0; *z=1.0f; }
};

// ─── Beeper stub ─────────────────────────────────────────────────────────────
struct CydBeep {
  void begin() {}
  void tone(uint16_t, uint16_t) {}
  void update() {}
};

// ─── RTC types matching M5StickCPlus API ────────────────────────────────────
struct RTC_TimeTypeDef { uint8_t Hours, Minutes, Seconds; };
struct RTC_DateTypeDef { uint8_t WeekDay, Month, Date; uint16_t Year; };

// Software RTC: calibrated from the bridge time-sync packet, tracks millis().
struct CydRtc {
  void SetTime(const RTC_TimeTypeDef* t) {
    _baseSec = (uint32_t)t->Hours * 3600 + t->Minutes * 60 + t->Seconds;
    _baseMs  = millis();
  }
  void SetDate(const RTC_DateTypeDef* d) { _date = *d; }
  void GetTime(RTC_TimeTypeDef* t) const {
    uint32_t now = (_baseSec + (millis() - _baseMs) / 1000) % 86400;
    t->Hours   = now / 3600;
    t->Minutes = (now / 60) % 60;
    t->Seconds = now % 60;
  }
  void GetDate(RTC_DateTypeDef* d) const { *d = _date; }
private:
  uint32_t        _baseSec = 0;
  uint32_t        _baseMs  = 0;
  RTC_DateTypeDef _date    = {0, 1, 1, 2024};
};

// ─── Top-level HAL — mirrors the M5 global API used by this project ─────────
class CydHal {
  XPT2046_Touchscreen _ts;
  bool     _touched    = false;
  bool     _promptMode = false;
  uint16_t _tx = 0, _ty = 0;
public:
  TFT_eSPI&          Lcd;
  CydAxp             Axp;
  CydImu             Imu;
  CydBeep            Beep;
  CydRtc             Rtc;
  // BtnA: GPIO 0  OR  left half of screen (raw x < CYD_TOUCH_MID)
  CydCombinedButton  BtnA;
  // BtnB: right half of screen (raw x >= CYD_TOUCH_MID)
  CydCombinedButton  BtnB;

  CydHal()
    : _ts(CYD_TOUCH_CS, CYD_TOUCH_IRQ)
    , Lcd(_cyd_tft)
    // y range 2700-4095 restricts buttons to the bottom bar area only.
    // Adjust CYD_BTN_Y_MIN if taps register too high or not at all.
    , BtnA(CYD_BTN_A,  0,             CYD_BTN_Y_MIN, CYD_TOUCH_MID - 1, 4095)
    , BtnB(0xFF,       CYD_TOUCH_MID, CYD_BTN_Y_MIN, 4095,              4095)
  {}

  void setPromptMode(bool prompt) {
    if (_promptMode == prompt) return;
    _promptMode = prompt;
    _drawButtonBar(BtnA.isPressed(), BtnB.isPressed());
  }

  void begin() {
    Serial.begin(115200);
    Lcd.init();
    Lcd.setRotation(0);
    Lcd.fillScreen(TFT_BLACK);

    // Backlight PWM — full brightness on boot
    ledcSetup(CYD_BL_LEDC_CH, 5000, 8);
    ledcAttachPin(CYD_BL_PIN, CYD_BL_LEDC_CH);
    ledcWrite(CYD_BL_LEDC_CH, 255);

    // Touch is on VSPI (separate from display HSPI).
    // SPI.begin() configures the VSPI instance used by XPT2046_Touchscreen.
    SPI.begin(CYD_TOUCH_CLK, CYD_TOUCH_MISO, CYD_TOUCH_MOSI, CYD_TOUCH_CS);
    _ts.begin();
    _ts.setRotation(0);

    BtnA.begin();

    // RGB LED — all off (active-low)
    pinMode(CYD_LED_R, OUTPUT); digitalWrite(CYD_LED_R, HIGH);
    pinMode(CYD_LED_G, OUTPUT); digitalWrite(CYD_LED_G, HIGH);
    pinMode(CYD_LED_B, OUTPUT); digitalWrite(CYD_LED_B, HIGH);

    Beep.begin();
    _drawButtonBar(false, false);
  }

  void update() {
    if (_ts.tirqTouched() && _ts.touched()) {
      TS_Point pt = _ts.getPoint();
      _touched = (pt.z >= CYD_TOUCH_Z);
      _tx = (uint16_t)pt.x;
      _ty = (uint16_t)pt.y;
    } else {
      _touched = false;
    }

#if CYD_TOUCH_DEBUG
    static uint32_t _lastPrint = 0;
    if (_touched && millis() - _lastPrint > 500) {
      Serial.printf("[touch] raw x=%u y=%u z=%u\n", _tx, _ty,
                    (unsigned)_ts.getPoint().z);
      _lastPrint = millis();
    }
#endif

    BtnA.update(_touched, _tx, _ty);
    BtnB.update(_touched, _tx, _ty);

    // Redraw button bar only when press state changes
    static bool prevA = false, prevB = false;
    bool a = BtnA.isPressed(), b = BtnB.isPressed();
    if (a != prevA || b != prevB) { _drawButtonBar(a, b); prevA = a; prevB = b; }
  }

private:
  void _drawButtonBar(bool aDown, bool bDown) {
    const uint16_t BG  = 0x2104;
    const uint16_t SEP = 0x4208;
    const uint16_t TXT = 0xFFFF;
    const uint16_t HI  = 0xFA20;
    const int BY = CYD_BTN_BAR_Y, BH = CYD_BTN_BAR_H;

    Lcd.drawFastHLine(0, BY, 240, SEP);

    // Left button (A / approve)
    Lcd.fillRect(0, BY + 1, 120, BH - 1, aDown ? HI : BG);
    Lcd.setTextDatum(MC_DATUM);
    Lcd.setTextColor(aDown ? BG : TXT, aDown ? HI : BG);
    if (_promptMode) {
      Lcd.setTextSize(1);
      Lcd.drawString("approve", 60, BY + BH / 2);
    } else {
      Lcd.setTextSize(2);
      Lcd.drawString("A", 60, BY + BH / 2);
    }

    // Right button (B / deny)
    Lcd.fillRect(120, BY + 1, 120, BH - 1, bDown ? HI : BG);
    Lcd.setTextColor(bDown ? BG : TXT, bDown ? HI : BG);
    if (_promptMode) {
      Lcd.setTextSize(1);
      Lcd.drawString("deny", 180, BY + BH / 2);
    } else {
      Lcd.setTextSize(2);
      Lcd.drawString("B", 180, BY + BH / 2);
    }

    Lcd.drawFastVLine(120, BY, BH, SEP);

#if CYD_TOUCH_DEBUG
    if (_touched) {
      Lcd.setTextSize(1);
      Lcd.setTextColor(TXT, aDown ? HI : BG);
      char buf[16];
      snprintf(buf, sizeof(buf), "%u/%u", _tx, _ty);
      Lcd.drawString(buf, 4, BY + 4);
    }
#endif

    Lcd.setTextDatum(TL_DATUM);
    Lcd.setTextSize(1);
  }
};
