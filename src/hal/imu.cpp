// IMU HAL implementation. Default (stub) path returns face-up resting
// orientation: gravity entirely on +z, no lateral motion. Net effect in
// main.cpp: isFaceDown() always false, checkShake() always false,
// clockUpdateOrient() pins to orient=0. See PORT.md §5A / §5B item 11.
//
// TODO: -DHAS_IMU to wire an MPU6050 on the P3 header (SDA=22, SCL=27).

#include "imu.h"

void imuBegin() {
    // No hardware to initialise in the stub path.
}

void imuRead(float* ax, float* ay, float* az) {
    *ax = 0.0f;
    *ay = 0.0f;
    *az = 1.0f;  // gravity on +z = device lying face-up, at rest
}
