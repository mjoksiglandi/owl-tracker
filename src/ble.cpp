// src/ble.cpp
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <cmath>     // std::isnan
#include <cstdio>    // std::snprintf

// Ya no necesitamos cifrar para BLE, pero dejamos includes por si reutilizas en otro lado
// #include "crypto.h"    // owl_encrypt_aes256gcm_base64(.)
// #include "secrets.h"   // OWL_AES256_KEY[32], OWL_GCM_IV_LEN

#include "ble.h"
#include "report.h"    // OwlReport

// ===== UUIDs =====
static const char* UUID_SVC         = "D9A00001-5F2F-4B6F-9F7C-6F0A4BF50001";
static const char* UUID_CHAR_REPORT = "D9A00002-5F2F-4B6F-9F7C-6F0A4BF50002"; // JSON (ahora en claro)
static const char* UUID_CHAR_INFO   = "D9A00003-5F2F-4B6F-9F7C-6F0A4BF50003"; // estado

// ===== Objetos BLE =====
static NimBLEServer*         g_server   = nullptr;
static NimBLEService*        g_service  = nullptr;
static NimBLECharacteristic* g_chReport = nullptr;
static NimBLECharacteristic* g_chInfo   = nullptr;
static NimBLEAdvertising*    g_adv      = nullptr;

static volatile bool g_connected = false;

// ===== Callbacks servidor =====
class OwlServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s) { g_connected = true; }
  void onDisconnect(NimBLEServer* s) {
    g_connected = false;
    if (g_adv) g_adv->start(); // re-adv
  }
};

static void setInitialInfo() {
  if (g_chInfo) g_chInfo->setValue("OwlTracker BLE ready");
}

// ===== API =====
bool ble_begin(const char* devName) {
  const char* name = (devName && *devName) ? devName : "OwlTracker";

  NimBLEDevice::init(name);
  NimBLEDevice::setPower(ESP_PWR_LVL_P4);
  NimBLEDevice::setSecurityAuth(false, false, false);

  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(new OwlServerCallbacks());

  g_service  = g_server->createService(UUID_SVC);

  g_chReport = g_service->createCharacteristic(
    UUID_CHAR_REPORT, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  g_chInfo   = g_service->createCharacteristic(
    UUID_CHAR_INFO,   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  g_chReport->setValue("");
  setInitialInfo();

  g_service->start();

  g_adv = NimBLEDevice::getAdvertising();
  g_adv->addServiceUUID(UUID_SVC);
  g_adv->start();

  return true;
}

bool ble_is_connected() { return g_connected; }

/** AHORA: Notifica por BLE el JSON EN CLARO (sin cifrar) */
bool ble_notify_report_json(const String& jsonPlain) {
  if (!g_chReport) return false;
  g_chReport->setValue(jsonPlain);
  if (g_connected) g_chReport->notify(true);
  return true;
}

void ble_set_info(const String& info) {
  if (!g_chInfo) return;
  g_chInfo->setValue(info);
  if (g_connected) g_chInfo->notify(true);
}

// void ble_set_name(const char* newName) {
//   if (!newName || !*newName) return;
//   NimBLEDevice::setDeviceName(newName);
//   if (g_adv) { g_adv->stop(); g_adv->start(); }
// }

// ===== Compatibilidad con tu main =====
void ble_poll() {}  // no-op

bool ble_update(const String& jsonPlain) {
  return ble_notify_report_json(jsonPlain);
}

// ===== Overload: ble_update(OwlReport) =====
static inline double nz(double v) { return std::isnan(v) ? 0.0 : v; }
static inline float  nzf(float v) { return std::isnan(v) ? 0.0f : v; }

bool ble_update(const OwlReport& rpt) {
  char buf[384];
  std::snprintf(buf, sizeof(buf),
    "{"
      "\"tipoReporte\":%d,"
      "\"IMEI\":\"%s\","
      "\"latitud\":%.6f,"
      "\"longitud\":%.6f,"
      "\"altitud\":%.1f,"
      "\"rumbo\":%.1f,"
      "\"velocidad\":%.2f,"
      "\"fechaHora\":\"%s\""
    "}",
    rpt.tipoReporte,
    rpt.IMEI.c_str(),
    nz(rpt.latitud), nz(rpt.longitud), nz(rpt.altitud),
    nzf(rpt.rumbo), nzf(rpt.velocidad),
    rpt.fechaHora.c_str()
  );

  return ble_notify_report_json(String(buf));
}
