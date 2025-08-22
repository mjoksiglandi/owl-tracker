#pragma once
#include <Arduino.h>

struct OwlUiData {
  int    csq = 99;          // 0..31 (99=desconocido)
  int    iridiumLvl = -1;   // 0..5 (futuro)  -1 = sin dato
  int    sats = -1;         // satélites en uso
  float  pdop = -1.0f;      // PDOP
  double lat  = NAN;        // grados decimales
  double lon  = NAN;        // grados decimales
  double alt  = NAN;        // metros MSL
  uint32_t msgRx = 0;       // contador de mensajes recibidos
  String utc = "";          // "YYYY-MM-DD HH:MM:SSZ"

  // Vector GNSS + Baro
  float  speed_mps = NAN;   // m/s (RMC)
  float  course_deg = NAN;  // grados (RMC)
  float  pressure_hpa = NAN;// hPa (MS5611)
};

bool oled_init();
void oled_draw_dashboard(const OwlUiData& d);
