#include "ble.h"
#include <NimBLEDevice.h>

// Servicio y características (UUIDs aleatorias)
static const char* UUID_SVC_STATE   = "d9a00001-5f2f-4b6f-9f7c-6f0a4bf50001";
static const char* UUID_CH_JSON     = "d9a00002-5f2f-4b6f-9f7c-6f0a4bf50002"; // snapshot texto
static const char* UUID_CH_FLAGS    = "d9a00003-5f2f-4b6f-9f7c-6f0a4bf50003"; // flags (futuro)

static NimBLEServer*         s_server = nullptr;
static NimBLEService*        s_svc    = nullptr;
static NimBLECharacteristic* s_chJson = nullptr;
static NimBLECharacteristic* s_chFlags= nullptr;

static bool s_started = false;

bool ble_begin(const char* devName) {
  if (s_started) return true;

  NimBLEDevice::init(devName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P7);   // TX bajo para ahorro
  NimBLEDevice::setMTU(185);

  s_server = NimBLEDevice::createServer();
  if (!s_server) return false;

  s_svc = s_server->createService(UUID_SVC_STATE);
  if (!s_svc) return false;

  s_chJson  = s_svc->createCharacteristic(UUID_CH_JSON,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  s_chFlags = s_svc->createCharacteristic(UUID_CH_FLAGS, NIMBLE_PROPERTY::READ);

  s_chJson->setValue("{\"status\":\"boot\"}");
  s_chFlags->setValue((uint8_t)0);

  s_svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(s_svc->getUUID());
  adv->setName(devName);          // ← usar nombre en el ADV
  // Si quisieras scan response, usar:
  // BLEAdvertisementData scanResp; scanResp.setName(devName);
  // adv->setScanResponseData(scanResp);

  adv->start();
  s_started = true;
  return true;
}

static String prec_code(float pdop) {
  if (!(pdop >= 0)) return "--";
  if (pdop < 1.5f)  return "EXC";
  if (pdop < 4.0f)  return "GUD";
  if (pdop < 6.0f)  return "MOD";
  if (pdop < 10.0f) return "ACC";
  return "POR";
}

void ble_update(const OwlUiData& ui) {
  if (!s_started) return;

  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"csq\":%d,\"sats\":%d,\"prec\":\"%s\",\"lat\":%.5f,\"lon\":%.5f,"
    "\"alt\":%.1f,\"utc\":\"%s\",\"msg\":%lu}",
    ui.csq,
    ui.sats,
    prec_code(ui.pdop).c_str(),
    isnan(ui.lat)?0.0:ui.lat,
    isnan(ui.lon)?0.0:ui.lon,
    isnan(ui.alt)?0.0:ui.alt,
    ui.utc.c_str(),
    (unsigned long)ui.msgRx
  );

  s_chJson->setValue((uint8_t*)buf, strlen(buf));
  s_chJson->notify(); // se ignora si no hay cliente conectado
}

void ble_poll() {
  // placeholder para futura RX/comandos por BLE
}
