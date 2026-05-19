#pragma once

// IMU HAL — CYD has no onboard IMU. Stub returns face-up, no-shake
// constants so isFaceDown / checkShake / clockUpdateOrient in main.cpp
// degrade silently. Build with -DHAS_IMU to enable an optional MPU6050
// driver on the P3 header (see imu.cpp). See PORT.md §5A.

void imuBegin();
void imuRead(float* ax, float* ay, float* az);  // out: accel in g
