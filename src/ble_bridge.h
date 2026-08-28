#pragma once
#include <stdint.h>
#include <stddef.h>

// Nordic UART Service-compatible BLE bridge. Clients (browser Web
// Bluetooth, noble, etc.) subscribe to NUS to talk to the Stick exactly
// like a serial port.
//
// Service UUID  6e400001-b5a3-f393-e0a9-e50e24dcca9e
// RX char       6e400002-b5a3-f393-e0a9-e50e24dcca9e   (client → stick, WRITE)
// TX char       6e400003-b5a3-f393-e0a9-e50e24dcca9e   (stick → client, NOTIFY)
//
// Writes from the client are line-buffered and dispatched through the
// same _applyJson path that USB/BT-Classic use. Replies (acks, status
// snapshots) are written via bleWrite() and chunked to the negotiated MTU.

void bleInit(const char* deviceName);
bool bleConnected();
// True once LE Secure Connections bonding has completed for the current
// link. The NUS characteristics are encrypted-only, so in practice this
// is always true by the time any data flows; exposed so the status ack
// can report it to the desktop.
bool bleSecure();
// Non-zero while a 6-digit pairing passkey should be on screen. main.cpp
// renders it; cleared automatically on auth complete or disconnect.
uint32_t blePasskey();
// Bytes lost because the RX ring was full. Non-zero means snapshots are being
// corrupted in transit, which otherwise looks like the bridge going silent.
uint32_t bleRxDropped();
// Erase all stored bonds (LTKs) from NVS. Called from the "unpair" cmd
// and from factory reset.
void bleClearBonds();
size_t bleAvailable();
int bleRead();
size_t bleWrite(const uint8_t* data, size_t len);
