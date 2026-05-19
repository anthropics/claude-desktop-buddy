#pragma once

// Touch calibration. On first boot, draws crosshairs at the four corners,
// captures the raw XPT2046 readings as the user taps each, and persists
// the linear-transform constants to NVS (namespace "tcal").
//
// touchCalibrateIfNeeded() runs the UI only if NVS has no saved
// calibration. Idempotent on subsequent boots.

void touchCalibrateIfNeeded();
void touchCalibrateForce();    // run UI regardless — exposed for a settings menu entry later
