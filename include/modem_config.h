#pragma once
#include <Arduino.h>
#include <TinyGsmClient.h>

// Baudios del puerto AT
#define MODEM_BAUD 115200

// RAT
#define LTE_ONLY 38   // AT+CNMP=38
#define AUTO_MODE 2   // AT+CNMP=2

// ---- API ----

// Enciende el módem: pone EN=HIGH y hace pulso en PWRKEY.
// activeHigh=true => pulso ALTO (revisión más común en T-A7670G)
// activeHigh=false => pulso BAJO
void modem_power_on(int pinEN, int pinPWR, bool activeHigh = true,
                    uint16_t pressMs = 1200, uint16_t settleMs = 2500);

// Inicializa el módem vía TinyGSM (init → si falla restart)
bool modem_init(TinyGsm &m);

// Fijar LTE only (AT+CNMP=38). Devuelve true si quedó en LTE.
bool modem_set_lte(TinyGsm &m);

// Fallback a modo automático (AT+CNMP=2).
bool modem_set_auto(TinyGsm &m);

// ...
// Espera registro de red (CS/PS) hasta timeout_ms.
bool modem_wait_for_network(TinyGsm &m, uint32_t timeout_ms = 60000);

// Dump de estado: operador, RAT, RSSI
void modem_print_status(TinyGsm &m);

