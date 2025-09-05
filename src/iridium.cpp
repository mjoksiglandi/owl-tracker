#include "iridium.h"
#include <Wire.h>
#include <IridiumSBD.h>  // SparkFun Iridium I2C library

// Dirección I2C por defecto del Qwiic Iridium
static uint8_t s_addr = 0x43;
static IridiumSBD *s_isbd = nullptr;
static bool s_present = false;
static int s_lastRssi = -1;

bool iridium_begin(uint8_t addr) {
  s_addr = addr;
  Wire.begin();
  s_isbd = new IridiumSBD(Wire, s_addr);

  if (!s_isbd) {
    Serial.println("[Iridium] Error: no se pudo instanciar objeto");
    return false;
  }

  // Power profile por defecto
  if (s_isbd->begin() != ISBD_SUCCESS) {
    Serial.println("[Iridium] No responde en I2C");
    s_present = false;
    return false;
  }

  s_present = true;
  Serial.println("[Iridium] Detectado OK");
  return true;
}

void iridium_poll() {
  if (!s_present) return;

  int quality = -1;
  int err = s_isbd->getSignalQuality(quality);
  if (err == ISBD_SUCCESS) {
    s_lastRssi = quality; // escala 0..5
  } else {
    s_lastRssi = -1;
  }
}

int iridium_getSignalQuality() {
  return s_lastRssi;
}

bool iridium_isPresent() {
  return s_present;
}
