#pragma once
#include <Arduino.h>

// Estructura canónica de reporte (BLE/HTTP)
struct OwlReport {
  int     tipoReporte = 1;          // 1=Normal, 2=Emergencia, 3=Mensaje libre, 4=POI
  String  IMEI = "";                // IMEI del equipo (prioriza GSM; fallback Iridium)
  double  latitud = 0.0;            // grados decimales
  double  longitud = 0.0;           // grados decimales
  double  altitud = 0.0;            // metros
  float   rumbo = 0.0f;             // grados
  float   velocidad = 0.0f;         // m/s
  String  fechaHora = "";           // "YYYY-MM-DD HH:MM:SSZ"
};
