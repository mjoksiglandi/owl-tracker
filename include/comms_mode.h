#pragma once
#include <Arduino.h>

// Políticas de comunicación para TESTING
enum class CommsMode : uint8_t {
  AUTO = 0,       // Prioriza GSM; fallback a Iridium; vuelve si reaparece GSM
  GSM_ONLY,       // Solo GSM
  IRIDIUM_ONLY,   // Solo Iridium
  BOTH            // Hace intento por ambos (en paralelo/turno)
};

// API mínima para el modo de pruebas
void       comms_mode_set(CommsMode m);
CommsMode  comms_mode_get();
const char* comms_mode_name(CommsMode m);
CommsMode  comms_mode_next(CommsMode m);
