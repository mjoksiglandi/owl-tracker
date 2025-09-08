#pragma once
#include <Arduino.h>
#include "oled_display.h"  // para OwlUiData

// Inicializa BLE (NimBLE). Devuelve true si quedó anunciado.
bool ble_begin(const char* devName = "OwlTracker");

// Actualiza características de estado desde un snapshot de UI.
// Llamar ~1 Hz o cuando cambien datos relevantes.
void ble_update(const OwlUiData& ui);

// Poll liviano (opcional por si agregas lógica futura de RX)
void ble_poll();
