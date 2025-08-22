#pragma once
#include <Arduino.h>

struct BaroData {
  float pressure_hpa  = NAN;  // presión en hPa
  float temperature_c = NAN;  // °C
};

bool  baro_begin();   // inicia el MS5611 en I2C (pines definidos en board_pins.h)
void  baro_poll();    // lee y filtra los datos
BaroData baro_last(); // devuelve la última muestra
