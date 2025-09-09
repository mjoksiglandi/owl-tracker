#include "iridium.h"
#include <Wire.h>
#include <IridiumSBD.h>

static IridiumSBD irid(Wire);
static IridiumInfo g_info;
static uint32_t    g_lastPollMs = 0;

bool iridium_begin() {
  int r = irid.begin();
  if (r == ISBD_SUCCESS) {
    g_info.present = true;

    // leer IMEI al arranque, aunque no haya red
    char buf[32] = {0};
    if (irid.getIMEI(buf, sizeof(buf)) == ISBD_SUCCESS) {
      g_info.imei = String(buf);
    } else {
      g_info.imei = String("NO_IMEI");
    }
  } else {
    g_info = IridiumInfo{}; // limpio
  }
  return g_info.present;
}

void iridium_poll() {
  if (!g_info.present) return;
  uint32_t now = millis();
  if (now - g_lastPollMs < 2000) return;
  g_lastPollMs = now;

  // Señal (0..5), -1 si falla
  int sig = -1;
  if (irid.getSignalQuality(sig) == ISBD_SUCCESS) {
    g_info.sig = constrain(sig, 0, 5);
  } else {
    g_info.sig = -1;
  }

  // Mensajes esperando
  int mtCount = irid.getWaitingMessageCount();
  g_info.waiting = (mtCount > 0);

  // Si no tenemos IMEI aún, reintenta
  if (g_info.imei.length() == 0 || g_info.imei == "NO_IMEI") {
    char buf[32] = {0};
    if (irid.getIMEI(buf, sizeof(buf)) == ISBD_SUCCESS) {
      g_info.imei = String(buf);
    }
  }
}

IridiumInfo iridium_status() {
  return g_info;
}
