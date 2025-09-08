#include "iridium.h"
#include <Wire.h>
#include <IridiumSBD.h>   // SparkFun Iridium 9603N (I2C)

// Instancia I2C del módem (usa TwoWire por defecto: Wire)
static IridiumSBD irid(Wire);

static IridiumInfo  g_info;
static uint32_t     g_lastPollMs = 0;

static bool getIMEI_String(String &out) {
  char buf[32] = {0};
  int r = irid.getIMEI(buf, sizeof(buf));          // ISBD_SUCCESS (0) si OK
  if (r == ISBD_SUCCESS && buf[0] != '\0') {
    out = String(buf);
    return true;
  }
  return false;
}

bool iridium_begin() {
  // Asegúrate de que Wire.begin(...) ya fue llamado en setup
  int r = irid.begin();
  if (r == ISBD_SUCCESS) {
    g_info.present = true;
    (void)getIMEI_String(g_info.imei);
  } else {
    g_info = IridiumInfo{}; // limpia estado
  }
  return g_info.present;
}

void iridium_poll() {
  if (!g_info.present) return;
  uint32_t now = millis();
  if (now - g_lastPollMs < 2000) return;   // refresco cada ~2 s
  g_lastPollMs = now;

  // Señal (0..5)
  int sig = -1;
  if (irid.getSignalQuality(sig) == ISBD_SUCCESS) {
    if (sig < 0) sig = 0;
    if (sig > 5) sig = 5;
    g_info.sig = sig;
  }

  // Cantidad de mensajes MT esperando (retorna int, -ve = error)
  int mtCount = irid.getWaitingMessageCount();
  if (mtCount >= 0) {
    g_info.waiting = (mtCount > 0);
  }

  // Reintento para IMEI si aún no se obtuvo
  if (g_info.imei.length() == 0) {
    String tmp;
    if (getIMEI_String(tmp)) g_info.imei = tmp;
  }
}

IridiumInfo iridium_status() {
  return g_info;
}
