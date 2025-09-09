#include "ble.h"
#include <NimBLEDevice.h>

static const char* UUID_SVC_STATE = "d9a00001-5f2f-4b6f-9f7c-6f0a4bf50001";
static const char* UUID_CH_JSON   = "d9a00002-5f2f-4b6f-9f7c-6f0a4bf50002";

static NimBLEServer*         s_server = nullptr;
static NimBLEService*        s_svc    = nullptr;
static NimBLECharacteristic* s_chJson = nullptr;
static bool s_started = false;

bool ble_begin(const char* devName) {
  if (s_started) return true;

  NimBLEDevice::init(devName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P7);
  NimBLEDevice::setMTU(185);

  s_server = NimBLEDevice::createServer();
  if (!s_server) return false;

  s_svc = s_server->createService(UUID_SVC_STATE);
  if (!s_svc) return false;

  s_chJson = s_svc->createCharacteristic(UUID_CH_JSON, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  s_chJson->setValue("{\"tipoReporte\":1}");

  s_svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(s_svc->getUUID());
  adv->setName(devName);
  adv->start();

  s_started = true;
  return true;
}

static inline double nz(double v) { return isnan(v) ? 0.0 : v; }
static inline float  nzf(float v) { return isnan(v) ? 0.0f : v; }

void ble_update(const OwlReport& rpt) {
  if (!s_started) return;

  // Construimos JSON exacto solicitado (sin dependencias)
  // Nota: Tamaño ~200B; MTU 185 puede limitar notify; si hiciera falta, reducimos precisión.
  char buf[320];
  snprintf(buf, sizeof(buf),
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
    nz(rpt.latitud),
    nz(rpt.longitud),
    nz(rpt.altitud),
    nzf(rpt.rumbo),
    nzf(rpt.velocidad),
    rpt.fechaHora.c_str()
  );

  s_chJson->setValue((uint8_t*)buf, strlen(buf));
  s_chJson->notify();  // si no hay cliente, no pasa nada
}

void ble_poll() {
  // Placeholder para futuras RX/ACKs por BLE
}
