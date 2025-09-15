#include <Arduino.h>
#include <Wire.h>
#include <IridiumSBD.h>   // SparkFun_IridiumSBD_I2C
#include "iridium.h"

// Estado
static uint8_t    s_addr    = IRIDIUM_I2C_ADDR;
static bool       s_present = false;
static String     s_imei    = "";
static int        s_sig     = -1;
static bool       s_waiting = false;

static IridiumSBD* s_irid   = nullptr;

static uint32_t t_lastSig  = 0;
static uint32_t t_lastWait = 0;
static const uint32_t PERIOD_SIG_MS  = 2000;
static const uint32_t PERIOD_WAIT_MS = 3000;

static bool i2c_check_ack(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

bool iridium_begin(uint8_t i2c_addr)
{
  s_addr = i2c_addr;

  if (!i2c_check_ack(s_addr)) {
    s_present = false; s_imei = ""; s_sig = -1; s_waiting = false;
    if (s_irid) { delete s_irid; s_irid = nullptr; }
    return false;
  }

  if (s_irid) { delete s_irid; s_irid = nullptr; }
  s_irid = new IridiumSBD(Wire, (uint8_t)s_addr);

  int r = s_irid->begin();
  if (r != ISBD_SUCCESS) {
    s_present = false; s_imei = ""; s_sig = -1; s_waiting = false;
    return false;
  }

  // IMEI
  char buf[32] = {0};
  if (s_irid->getIMEI(buf, sizeof(buf)) == ISBD_SUCCESS) s_imei = String(buf);
  else s_imei = "";

  // Señal inicial
  int s = -1;
  if (s_irid->getSignalQuality(s) == ISBD_SUCCESS) s_sig = s; else s_sig = -1;

  // Pendientes
  int mt = s_irid->getWaitingMessageCount();
  s_waiting = (mt > 0);

  s_present = true;
  t_lastSig = t_lastWait = millis();
  return true;
}

void iridium_poll()
{
  if (!s_present || !s_irid) return;
  uint32_t now = millis();

  if (now - t_lastSig >= PERIOD_SIG_MS) {
    t_lastSig = now;
    int s = -1;
    if (s_irid->getSignalQuality(s) == ISBD_SUCCESS) s_sig = s;
  }

  if (now - t_lastWait >= PERIOD_WAIT_MS) {
    t_lastWait = now;
    int mt = s_irid->getWaitingMessageCount();
    s_waiting = (mt > 0);
  }
}

IridiumInfo iridium_status()
{
  IridiumInfo info;
  info.present = s_present;
  info.sig     = s_sig;
  info.waiting = s_waiting;
  info.imei    = s_imei;
  return info;
}

bool iridium_send_cipher_b64(const String& cipher)
{
  if (!s_present || !s_irid) return false;
  if (cipher.length() > (int)IRIDIUM_MAX_TX) return false; // ≤270
  int r = s_irid->sendSBDText(cipher.c_str());
  return (r == ISBD_SUCCESS);
}

// RX (stub por limitación de API I2C)
bool iridium_fetch_next(String& outCipher)
{
  outCipher = "";
  if (!s_present || !s_irid) return false;
  int mt = s_irid->getWaitingMessageCount();
  s_waiting = (mt > 0);
  return false;
}
