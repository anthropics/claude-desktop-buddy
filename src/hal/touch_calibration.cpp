#include "touch_calibration.h"
#include "board.h"
#include <Preferences.h>
#include <Arduino.h>

static constexpr int16_t DISP_W = 240;
static constexpr int16_t DISP_H = 320;
static constexpr int16_t MARGIN = 20;   // crosshair distance from each edge
static constexpr uint16_t Z_THRESHOLD = 400;

struct RawPoint { int16_t x; int16_t y; };

static void drawCross(int16_t x, int16_t y, uint16_t color) {
    tft.drawFastHLine(x - 12, y, 25, color);
    tft.drawFastVLine(x, y - 12, 25, color);
    tft.drawCircle(x, y, 6, color);
}

// Wait for a clean press → release, return the raw XPT2046 sample taken
// near the start of the press (most stable, before finger drift).
static RawPoint captureTap() {
    // Wait for finger up if currently down.
    while (ts.touched()) { delay(10); }

    // Wait for finger down.
    RawPoint sample = {0, 0};
    while (true) {
        if (ts.touched()) {
            TS_Point p = ts.getPoint();
            if (p.z >= Z_THRESHOLD) {
                sample.x = p.x;
                sample.y = p.y;
                break;
            }
        }
        delay(5);
    }
    // Brief settle then re-sample for better accuracy.
    delay(60);
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        if (p.z >= Z_THRESHOLD) { sample.x = p.x; sample.y = p.y; }
    }
    // Wait for release before returning so the next prompt is clean.
    while (ts.touched()) { delay(10); }
    delay(80);
    return sample;
}

static void promptCorner(int16_t cx, int16_t cy, const char* label) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("touch calibration", DISP_W / 2, DISP_H / 2 - 20);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(label, DISP_W / 2, DISP_H / 2 + 8);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("tap the crosshair", DISP_W / 2, DISP_H / 2 + 24);
    drawCross(cx, cy, TFT_GREEN);
}

void touchCalibrateForce() {
    // Capture 4 corners.
    promptCorner(MARGIN,          MARGIN,          "1 / 4  top-left");
    RawPoint tl = captureTap();
    promptCorner(DISP_W - MARGIN, MARGIN,          "2 / 4  top-right");
    RawPoint tr = captureTap();
    promptCorner(MARGIN,          DISP_H - MARGIN, "3 / 4  bottom-left");
    RawPoint bl = captureTap();
    promptCorner(DISP_W - MARGIN, DISP_H - MARGIN, "4 / 4  bottom-right");
    RawPoint br = captureTap();

    // Infer axis orientation. If raw "x" varies more between TL and BL than
    // TL and TR, the panel is rotated relative to the display — swap axes.
    int xVarHoriz = abs(tr.x - tl.x);
    int xVarVert  = abs(bl.x - tl.x);
    bool swapXY = xVarVert > xVarHoriz;

    int16_t rxTL = swapXY ? tl.y : tl.x;
    int16_t ryTL = swapXY ? tl.x : tl.y;
    int16_t rxBR = swapXY ? br.y : br.x;
    int16_t ryBR = swapXY ? br.x : br.y;

    bool flipX = rxBR < rxTL;
    bool flipY = ryBR < ryTL;
    int16_t xMin = flipX ? rxBR : rxTL;
    int16_t xMax = flipX ? rxTL : rxBR;
    int16_t yMin = flipY ? ryBR : ryTL;
    int16_t yMax = flipY ? ryTL : ryBR;

    // Extrapolate from the 20px margins back to the full 0..DISP edges so
    // input.cpp's linear remap doesn't need to know the margin.
    long spanX_at_marg = xMax - xMin;
    long spanY_at_marg = yMax - yMin;
    long marginScaleX = spanX_at_marg * MARGIN / (DISP_W - 2 * MARGIN);
    long marginScaleY = spanY_at_marg * MARGIN / (DISP_H - 2 * MARGIN);
    xMin -= marginScaleX;
    xMax += marginScaleX;
    yMin -= marginScaleY;
    yMax += marginScaleY;

    Preferences p;
    p.begin("tcal", false);
    p.putShort("xMin", xMin);
    p.putShort("xMax", xMax);
    p.putShort("yMin", yMin);
    p.putShort("yMax", yMax);
    p.putBool ("swap", swapXY);
    p.putBool ("flipX", flipX);
    p.putBool ("flipY", flipY);
    p.end();

    // Acknowledge.
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("calibrated", DISP_W / 2, DISP_H / 2);
    Serial.printf("[tcal] xMin=%d xMax=%d yMin=%d yMax=%d swap=%d flipX=%d flipY=%d\n",
                  xMin, xMax, yMin, yMax, swapXY, flipX, flipY);
    delay(900);
}

void touchCalibrateIfNeeded() {
    Preferences p;
    p.begin("tcal", true);
    bool hasCal = p.isKey("xMin");
    p.end();
    if (!hasCal) touchCalibrateForce();
}
