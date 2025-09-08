#pragma once
#include <Arduino.h>

struct IridiumInfo {
  bool   present = false;   // Detectado en I2C
  int    sig     = -1;      // 0..5  (-1 = sin dato)
  bool   waiting = false;   // ¿hay MT pendiente?
  String imei    = "";      // IMEI del 9603N
};

// Inicializa el módem (I2C). Devuelve true si está presente.
bool iridium_begin();

// Refresco periódico (no bloqueante). Llamar cada 1–5 s.
void iridium_poll();

// Último estado cacheado.
IridiumInfo iridium_status();
