#pragma once
#include <Arduino.h>
#include <TinyGsmClient.h>

// Baudios del puerto AT
#define MODEM_BAUD 115200

// RAT
// #define LTE_ONLY 38   // AT+CNMP=38
#define AUTO_MODE 2   // AT+CNMP=2

// ---- API ----
void modem_power_on(int pinEN, int pinPWR, bool activeHigh = true,
                    uint16_t pressMs = 1200, uint16_t settleMs = 2500);

bool modem_init(TinyGsm &m);

// // (seguirá existiendo, pero YA NO la usaremos)
// bool modem_set_lte(TinyGsm &m);

// Dejamos el módem en auto-rat (2G/3G/4G/5G)
bool modem_set_auto(TinyGsm &m);

// Espera registro de red (CS/PS) hasta timeout_ms.
bool modem_wait_for_network(TinyGsm &m, uint32_t timeout_ms = 60000);

// Dump de estado: operador, RAT, RSSI
void modem_print_status(TinyGsm &m);

// ===== NUEVO: obtener etiqueta "2G/3G/4G/5G" (parsea AT+CPSI?) =====
String modem_rat_label(TinyGsm &m);

// Desactiva PSM/eDRX y deja RAT en AUTO
bool modem_radio_tune(TinyGsm &m);

// Cierra PDP y vuelve a abrir APN de cero (hard reset del portador de datos)
bool modem_pdp_hard_reset(TinyGsm &m, const char* apn, const char* user, const char* pass);

// Secuencia de “recuperación completa” (detach/attach + CFUN cycling opcional)
bool modem_radio_recover(TinyGsm &m, const char* apn, const char* user, const char* pass);

// Dump concentrado de estado de registro + RAT (para diagnóstico)
void modem_dump_regs(TinyGsm &m);
