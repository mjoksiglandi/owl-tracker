#pragma once
#include <Arduino.h>

struct GpsFix {
  bool   valid   = false;
  double lat     = NAN;
  double lon     = NAN;
  double alt     = NAN;   // metros MSL (de GGA)
  int    sats    = -1;
  float  pdop    = -1.0f; // de GSA
  String utc     = "";    // "YYYY-MM-DD HH:MM:SS"
};

bool   gps_begin_uart(HardwareSerial& gpsSerial, int rxPin, int txPin, uint32_t baud);
void   gps_poll(HardwareSerial& gpsSerial);
GpsFix gps_last_fix();

// Invalidar fix si no llegan NMEA por X ms (default 10s)
void   gps_set_stale_timeout(uint32_t ms);
