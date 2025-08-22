#pragma once
#include <Arduino.h>

struct GpsFix {
  bool   valid   = false;
  double lat     = NAN;
  double lon     = NAN;
  double alt     = NAN;   // m (GGA)
  int    sats    = -1;
  float  pdop    = -1.0f; // GSA
  String utc     = "";    // "YYYY-MM-DD HH:MM:SS"

  float  speed_mps  = NAN;  // RMC (knots -> m/s)
  float  course_deg = NAN;  // RMC (deg)
};

bool   gps_begin_uart(HardwareSerial& gpsSerial, int rxPin, int txPin, uint32_t baud);
void   gps_poll(HardwareSerial& gpsSerial);
GpsFix gps_last_fix();
void   gps_set_stale_timeout(uint32_t ms);
