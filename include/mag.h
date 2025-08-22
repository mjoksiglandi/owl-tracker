#pragma once
#include <Arduino.h>

struct MagData {
  float mx_uT = NAN;
  float my_uT = NAN;
  float mz_uT = NAN;
  bool  present = false;
};

bool mag_begin();  // IST8310 detect
void mag_poll();   // stub: pendiente lecturas reales
MagData mag_last();
