#pragma once
#include <Arduino.h>

/** Datos comunes para todas las pantallas */
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
  float  speed_mps   = NAN;  // m/s (RMC)
  float  course_deg  = NAN;  // grados (RMC)
  float  pressure_hpa= NAN;  // hPa
};

/** Inicialización del OLED (SSD1322 256x64 SPI) */
bool oled_init();

/** Pantalla de arranque/splash simple (opcional) */
void oled_splash(const char* title);

/** HOME / Dashboard (tres secciones superiores con líneas, lat/lon/alt/vel, pdop/utc/msg) */
void oled_draw_dashboard(const OwlUiData& d);

/** Detalle de GPS (fix, sats, pdop, lat/lon/alt, speed/course/track) */
void oled_draw_gps_detail(const OwlUiData& d);

/** Detalle de Iridium */
void oled_draw_iridium_detail(bool present, int sigLevel, int unread, const String& imei);

/** Configuración del sistema */
void oled_draw_sys_config(const OwlUiData& d, bool netReg, bool pdpUp,
                          const String& ip, bool sdOk, bool i2cOk, const char* fw);

/** Bandeja de mensajes (stub) */
void oled_draw_messages(uint16_t unread, const String& last);

// ===== NUEVOS =====

/** Detalle GSM/LTE: IMEI, estado red/PDP, RSSI (dBm) */
void oled_draw_gsm_detail(const OwlUiData& d,
                          const String& imei,
                          bool netReg,
                          bool pdpUp,
                          int rssiDbm);

/** Detalle BLE: estado de enlace / nombre publicitario */
void oled_draw_ble_detail(bool connected,
                          const String& devName);
