#include "board.h"
#include "input.h"
#include "audio.h"
#include "rtc.h"
#include "touch_calibration.h"
#include <SPI.h>

// Display uses the default `SPI` instance (VSPI) per build_flags pin map.
// Touch lives on a physically separate HSPI bus — TFT_eSPI's built-in
// TOUCH_CS sharing model does NOT apply on the CYD.
TFT_eSPI tft;
SPIClass hspiTouch(HSPI);
XPT2046_Touchscreen ts(33, 36);  // CS=GPIO33, IRQ=GPIO36

void boardBegin() {
    tft.init();
    tft.setRotation(0);                 // portrait, 240x320
    hspiTouch.begin(25, 39, 32);        // HSPI: SCK=25, MISO=39, MOSI=32
    ts.begin(hspiTouch);                // bind touch to HSPI (critical)
    ts.setRotation(0);
    touchCalibrateIfNeeded();           // first-boot 4-corner UI; no-op on subsequent boots
}

void boardLoop() {
    // Per-tick HAL housekeeping. main.cpp calls boardLoop() at the top of
    // its main loop (where M5.update() used to live) and gets touch poll,
    // tone auto-stop, and RTC NVS-save for free.
    inputUpdate();
    audioLoop();
    rtcLoop();
}
