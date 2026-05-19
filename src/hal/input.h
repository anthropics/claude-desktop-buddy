#pragma once
#include <stdint.h>

// Touch-zone "buttons" mirroring the M5.BtnA/B semantics the upstream
// firmware was written against. Zones are defined in input.cpp and
// resolved per tick from the XPT2046 touch controller.

class InputButton {
public:
    InputButton(int16_t x, int16_t y, int16_t w, int16_t h);
    bool wasPressed();
    bool wasReleased();
    bool isPressed();
    bool pressedFor(uint32_t ms);

    // internal — updated by inputUpdate()
    void _tick(bool inside, uint32_t now);
    bool _hits(int16_t px, int16_t py) const {
        return px >= _x && px < _x + _w && py >= _y && py < _y + _h;
    }

private:
    int16_t _x, _y, _w, _h;
    bool _down = false;
    bool _prevDown = false;
    bool _justPressed = false;
    bool _justReleased = false;
    uint32_t _downSince = 0;
};

extern InputButton inputA;       // ZONE_APPROVE — bottom-right half
extern InputButton inputB;       // ZONE_DENY    — bottom-left half
extern InputButton inputSleep;   // ZONE_SLEEP   — top-right corner

void inputBegin();
void inputUpdate();              // poll touch, transform via calibration, dispatch to zones

// Translate a raw XPT2046 reading to display pixel coords. Uses calibration
// if available, else sane defaults. Returns true if the point is "pressed"
// (z >= threshold) and writes display_x / display_y.
bool inputReadDisplay(int16_t* display_x, int16_t* display_y);
