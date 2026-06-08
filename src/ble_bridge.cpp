#include "ble_bridge.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <string.h>

// Nordic UART Service UUIDs — every BLE serial example uses these, so
// existing tools (nRF Connect, bluefy, Web Bluetooth examples) can talk to
// us without custom UUIDs.
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// Debug service for cc-bridge (Claude Code daemon) — same line-JSON
// protocol but UNENCRYPTED. The encrypted NUS link kept dropping under
// macOS bleak when held open for sustained writes (CoreBluetooth +
// ESP32 secure pairing don't play well together for our use case),
// which broke the permission echo's 5s window. The debug service skips
// pairing entirely; security tradeoff is acceptable for a desk toy.
#define DBG_SERVICE_UUID "b0c2dbe6-cc01-4000-8000-00805f9b34fb"
#define DBG_RX_UUID      "b0c2dbe6-cc02-4000-8000-00805f9b34fb"
#define DBG_TX_UUID      "b0c2dbe6-cc03-4000-8000-00805f9b34fb"

// Incoming bytes are buffered in a simple ring for bleRead()/bleAvailable().
// Sized to hold a transcript snapshot JSON plus headroom; the GATT layer
// will flow-control if we fall behind.
static const size_t RX_CAP = 2048;
static uint8_t  rxBuf[RX_CAP];
static volatile size_t rxHead = 0;
static volatile size_t rxTail = 0;

static BLEServer*         server = nullptr;
static BLECharacteristic* txChar = nullptr;
static BLECharacteristic* rxChar = nullptr;
// Debug service mirrors txChar/rxChar without encryption.
static BLECharacteristic* dtxChar = nullptr;
static BLECharacteristic* drxChar = nullptr;
static volatile bool      connected = false;
static volatile bool      secure = false;
static volatile uint32_t  passkey = 0;
static volatile uint16_t  mtu = 23;

static void rxPush(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    size_t next = (rxHead + 1) % RX_CAP;
    if (next == rxTail) return;  // full — drop (upstream should keep up)
    rxBuf[rxHead] = p[i];
    rxHead = next;
  }
}

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    std::string v = c->getValue();
    if (!v.empty()) rxPush((const uint8_t*)v.data(), v.size());
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    connected = true;
    Serial.println("[ble] connected");
  }
  void onDisconnect(BLEServer* s) override {
    connected = false;
    secure = false;
    passkey = 0;
    mtu = 23;
    Serial.println("[ble] disconnected");
    // Restart advertising so the next client can find us.
    BLEDevice::startAdvertising();
  }
  void onMtuChanged(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
    mtu = param->mtu.mtu;
    Serial.printf("[ble] mtu=%u\n", mtu);
  }
};

// LE Secure Connections, passkey-entry: we are DisplayOnly, the central
// is KeyboardOnly. The stack picks a random 6-digit passkey, calls
// onPassKeyNotify here, and the user types it on the desktop. main.cpp
// polls blePasskey() to render it.
class SecCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 0; }
  bool onConfirmPIN(uint32_t) override { return false; }
  bool onSecurityRequest() override {
#ifdef BUDDY_BOARD_STICKS3
    return false;   // S3: refuse bonding — daemon uses the open debug service
#else
    return true;
#endif
  }
  void onPassKeyNotify(uint32_t pk) override {
    passkey = pk;
    Serial.printf("[ble] passkey %06lu\n", (unsigned long)pk);
  }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    passkey = 0;
    secure = cmpl.success;
    Serial.printf("[ble] auth %s\n", cmpl.success ? "ok" : "FAIL");
    if (!cmpl.success && server) server->disconnect(server->getConnId());
  }
};

void bleInit(const char* deviceName) {
  BLEDevice::init(deviceName);
  // Request the biggest MTU we can get. macOS negotiates to 185 typically.
  BLEDevice::setMTU(517);

#ifndef BUDDY_BOARD_STICKS3
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
#endif
  BLEDevice::setSecurityCallbacks(new SecCallbacks());

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* svc = server->createService(NUS_SERVICE_UUID);

  txChar = svc->createCharacteristic(
    NUS_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
#ifdef BUDDY_BOARD_STICKS3
  // S3 integrates via the open debug service, so leave the main NUS chars
  // unencrypted too — that way NO characteristic forces pairing and a fresh
  // (unbonded) stick is connectable by the daemon without the flaky passkey
  // dance. Claude-Desktop-direct encrypted use isn't part of the S3 flow.
  txChar->setAccessPermissions(ESP_GATT_PERM_READ);
  BLE2902* cccd = new BLE2902();
  cccd->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
#else
  txChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  BLE2902* cccd = new BLE2902();
  cccd->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  txChar->addDescriptor(cccd);

  rxChar = svc->createCharacteristic(
    NUS_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
#ifdef BUDDY_BOARD_STICKS3
  rxChar->setAccessPermissions(ESP_GATT_PERM_WRITE);
#else
  rxChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  rxChar->setCallbacks(new RxCallbacks());

  svc->start();

  // Debug service — same line-JSON protocol but no encryption. Used by
  // tools/cc-bridge daemon. Default permissions are open (no _ENCRYPTED
  // suffix), so bleak on macOS won't trigger pairing/encryption when it
  // accesses these characteristics.
  BLEService* dsvc = server->createService(DBG_SERVICE_UUID);
  dtxChar = dsvc->createCharacteristic(
    DBG_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  dtxChar->setAccessPermissions(ESP_GATT_PERM_READ);   // open, no MITM
  // CCCD descriptor for the notify characteristic — bleak's start_notify
  // writes to this. Open permissions so encryption is never triggered.
  BLE2902* dcccd = new BLE2902();
  dcccd->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  dtxChar->addDescriptor(dcccd);
  drxChar = dsvc->createCharacteristic(
    DBG_RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  drxChar->setAccessPermissions(ESP_GATT_PERM_WRITE);  // open, no MITM
  drxChar->setCallbacks(new RxCallbacks());   // same callback feeds shared rxBuf
  dsvc->start();

#ifndef BUDDY_BOARD_STICKS3
  // No bonding setup on S3 — nothing here requires encryption, so requesting
  // SC_MITM_BOND would only re-introduce the passkey dance that blocks a fresh
  // stick from connecting.
  BLESecurity* sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  sec->setCapability(ESP_IO_CAP_OUT);
  sec->setKeySize(16);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
#endif

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->addServiceUUID(DBG_SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);   // iOS-friendly connection interval
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.printf("[ble] advertising as '%s'\n", deviceName);
}

bool bleConnected() { return connected; }
bool bleSecure()    { return secure; }
uint32_t blePasskey() { return passkey; }

void bleClearBonds() {
  int n = esp_ble_get_bond_device_num();
  if (n <= 0) return;
  esp_ble_bond_dev_t* list = (esp_ble_bond_dev_t*)malloc(n * sizeof(esp_ble_bond_dev_t));
  if (!list) return;
  esp_ble_get_bond_device_list(&n, list);
  for (int i = 0; i < n; i++) esp_ble_remove_bond_device(list[i].bd_addr);
  free(list);
  Serial.printf("[ble] cleared %d bond(s)\n", n);
}

size_t bleAvailable() {
  return (rxHead + RX_CAP - rxTail) % RX_CAP;
}

int bleRead() {
  if (rxHead == rxTail) return -1;
  int b = rxBuf[rxTail];
  rxTail = (rxTail + 1) % RX_CAP;
  return b;
}

size_t bleWrite(const uint8_t* data, size_t len) {
  if (!connected) return 0;
  // ATT notify payload is limited to (MTU - 3). macOS negotiates 185, so
  // the 182-byte chunk works there; use the live mtu so a peer that caps
  // at the 23-byte default doesn't get truncated notifies.
  size_t chunk = mtu > 3 ? mtu - 3 : 20;
  if (chunk > 180) chunk = 180;
  size_t sent = 0;
  while (sent < len) {
    size_t n = len - sent;
    if (n > chunk) n = chunk;
    // Mirror to both encrypted (NUS) and unencrypted (debug) tx
    // characteristics so whichever central is connected sees the same
    // line-JSON stream — Claude Desktop subscribes to NUS, cc-bridge
    // subscribes to the debug tx.
    if (txChar)  { txChar->setValue((uint8_t*)(data + sent), n);  txChar->notify(); }
    if (dtxChar) { dtxChar->setValue((uint8_t*)(data + sent), n); dtxChar->notify(); }
    sent += n;
    delay(4);   // small yield so the BLE stack flushes before the next chunk
  }
  return sent;
}
