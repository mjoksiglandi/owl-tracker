#pragma once
#include <Arduino.h>

// (Si no incluyes settings.h aquí, forward-declare)
enum class LinkPref : uint8_t;

struct OwlUiData {
  int    csq = 99;           // 0..31 (99=desconocido)
  int    iridiumLvl = -1;    // 0..5  (-1 = sin dato)
  int    sats = -1;          // satélites en uso
  float  pdop = -1.0f;       // PDOP
  double lat  = NAN;         // grados decimales
  double lon  = NAN;         // grados decimales
  double alt  = NAN;         // metros MSL
  uint32_t msgRx = 0;        // contador de mensajes recibidos
  String utc = "";           // "YYYY-MM-DD HH:MM:SSZ"

  // Vector GNSS + Baro (baro opcional)
  float  speed_mps   = NAN;  // m/s
  float  course_deg  = NAN;  // grados
  float  pressure_hpa= NAN;  // hPa
};

bool oled_init();
void oled_splash(const char* title);

// HOME con etiqueta RAT (2G/3G/4G/5G)
void oled_draw_dashboard(const OwlUiData& d, const char* ratLabel);

// Pantallas info
void oled_draw_gps_detail(const OwlUiData& d);
void oled_draw_iridium_detail(bool present, int sigLevel, int unread, const String& imei);
void oled_draw_sys_config(const OwlUiData& d, bool netReg, bool pdpUp,
                          const String& ip, bool sdOk, bool i2cOk, const char* fw);
void oled_draw_messages(uint16_t unread, const String& last);

// Detalles GSM/BLE
void oled_draw_gsm_detail(const OwlUiData& d,
                          const String& imei,
                          bool netReg,
                          bool pdpUp,
                          int rssiDbm);
void oled_draw_ble_detail(bool connected, const String& devName);

// Testing simple (compat)
void oled_draw_testing(LinkPref pref, const char* lastResult, bool busy);

// === NUEVO: Testing avanzado por páginas (no ejecuta acciones) ===
void oled_draw_testing_adv(
  // Página 1: Red móvil
  const char* modeName, bool netReg, bool pdpUp,
  const char* ratLabel, int csq, int rssiDbm, const char* ipStr,
  // Página 2: Radios/IDs/Sistema
  const char* imeiGsm,
  bool irPresent, int irSig, const char* irImei, int irUnread,
  bool bleConn, uint32_t freeHeap,
  // Común
  const char* lastResult, bool busy, uint8_t page /*0..1*/
);
