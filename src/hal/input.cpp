#include "input.h"
#include "board.h"
#include <Preferences.h>
#include <Arduino.h>

// Display geometry (matches main.cpp W/H — keep in sync if portrait rotation
// changes). 240 × 320 portrait.
static constexpr int16_t DISP_W = 240;
static constexpr int16_t DISP_H = 320;

// Zone footprint constants. Bottom bar gets the lower 80 px; ZONE_SLEEP
// occupies a 50 × 50 box at the top-right corner.
static constexpr int16_t BAR_H = 80;
static constexpr int16_t SLEEP_BOX = 50;

InputButton inputA     (DISP_W / 2, DISP_H - BAR_H, DISP_W / 2, BAR_H);
InputButton inputB     (0,          DISP_H - BAR_H, DISP_W / 2, BAR_H);
InputButton inputSleep (DISP_W - SLEEP_BOX, 0,      SLEEP_BOX,  SLEEP_BOX);

// Calibration: linear remap of raw XPT2046 readings (0..4095) to display
// pixels. Defaults below are typical for 2432S028R panels — overwritten by
// touch_calibration.cpp on first boot.
static int16_t s_rawXMin = 300;
static int16_t s_rawXMax = 3800;
static int16_t s_rawYMin = 300;
static int16_t s_rawYMax = 3800;
static bool    s_swapXY = false;
static bool    s_flipX  = false;
static bool    s_flipY  = false;

static constexpr uint16_t Z_THRESHOLD = 400;     // XPT2046 pressure sensitivity
static constexpr uint32_t DEBOUNCE_MS = 30;

static uint32_t s_lastDownChange = 0;
static bool     s_filteredDown   = false;

static void inputLoadCalibration() {
    Preferences p;
    p.begin("tcal", true);
    if (p.isKey("xMin")) {
        s_rawXMin = p.getShort("xMin", s_rawXMin);
        s_rawXMax = p.getShort("xMax", s_rawXMax);
        s_rawYMin = p.getShort("yMin", s_rawYMin);
        s_rawYMax = p.getShort("yMax", s_rawYMax);
        s_swapXY  = p.getBool ("swap", false);
        s_flipX   = p.getBool ("flipX", false);
        s_flipY   = p.getBool ("flipY", false);
    }
    p.end();
}

bool inputReadDisplay(int16_t* display_x, int16_t* display_y) {
    if (!ts.touched()) return false;
    TS_Point pt = ts.getPoint();
    if (pt.z < Z_THRESHOLD) return false;

    int16_t rx = pt.x;
    int16_t ry = pt.y;
    if (s_swapXY) { int16_t t = rx; rx = ry; ry = t; }

    long dx = (long)(rx - s_rawXMin) * DISP_W / (long)(s_rawXMax - s_rawXMin);
    long dy = (long)(ry - s_rawYMin) * DISP_H / (long)(s_rawYMax - s_rawYMin);
    if (s_flipX) dx = DISP_W - 1 - dx;
    if (s_flipY) dy = DISP_H - 1 - dy;
    if (dx < 0) dx = 0; if (dx >= DISP_W) dx = DISP_W - 1;
    if (dy < 0) dy = 0; if (dy >= DISP_H) dy = DISP_H - 1;

    if (display_x) *display_x = (int16_t)dx;
    if (display_y) *display_y = (int16_t)dy;
    return true;
}

InputButton::InputButton(int16_t x, int16_t y, int16_t w, int16_t h)
    : _x(x), _y(y), _w(w), _h(h) {}

void InputButton::_tick(bool inside, uint32_t now) {
    _prevDown = _down;
    _down = inside;
    if (_down && !_prevDown)      { _justPressed  = true; _downSince = now; }
    else                          { _justPressed  = false; }
    if (!_down && _prevDown)      { _justReleased = true; }
    else                          { _justReleased = false; }
}

bool InputButton::wasPressed()  { bool v = _justPressed;  _justPressed  = false; return v; }
bool InputButton::wasReleased() { bool v = _justReleased; _justReleased = false; return v; }
bool InputButton::isPressed()   { return _down; }
bool InputButton::pressedFor(uint32_t ms) {
    return _down && (millis() - _downSince) >= ms;
}

void inputBegin() {
    inputLoadCalibration();
}

void inputUpdate() {
    uint32_t now = millis();

    int16_t dx = -1, dy = -1;
    bool rawDown = inputReadDisplay(&dx, &dy);

    // Debounce the down-state so XPT2046 chatter doesn't spam edges.
    if (rawDown != s_filteredDown) {
        if (now - s_lastDownChange >= DEBOUNCE_MS) {
            s_filteredDown = rawDown;
            s_lastDownChange = now;
        }
    } else {
        s_lastDownChange = now;
    }

    if (s_filteredDown) {
        inputA._tick(inputA._hits(dx, dy), now);
        inputB._tick(inputB._hits(dx, dy), now);
        inputSleep._tick(inputSleep._hits(dx, dy), now);
    } else {
        inputA._tick(false, now);
        inputB._tick(false, now);
        inputSleep._tick(false, now);
    }
}
