#pragma once
#include <Arduino.h>

struct GpsFix {
  bool   valid   = false;
  double lat     = NAN;
  double lon     = NAN;
  int    sats    = -1;     // satélites en uso (de GGA)
  float  pdop    = -1.0f;  // de GSA si está disponible
  String utc     = "";     // HH:MM:SS (si lo traemos de GGA)
};

// Inicializa GPS por UART externo (NMEA). Devuelve true si ok.
bool gps_begin_uart(HardwareSerial& gpsSerial, int rxPin, int txPin, uint32_t baud);

// Debe llamarse MUY seguido (loop). Lee NMEA y actualiza el fix.
void gps_poll(HardwareSerial& gpsSerial);

// Obtiene la última solución (copia por valor).
GpsFix gps_last_fix();
