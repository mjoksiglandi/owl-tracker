#pragma once
#include <Arduino.h>
#include "report.h"

bool ble_begin(const char* devName);

// Publica el estado por BLE en formato JSON (según OwlReport)
void ble_update(const OwlReport& rpt);

// Poll no-bloqueante para futuras RX/comandos
void ble_poll();
