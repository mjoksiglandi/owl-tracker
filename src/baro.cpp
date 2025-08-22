#include "baro.h"
#include "board_pins.h"

#include <Wire.h>
#include <MS5611.h>   // RobTillaart/MS5611

static MS5611   g_baro;
static bool     g_ok   = false;
static BaroData g_last;

// Filtro exponencial simple
static float lp(float prev, float cur, float alpha) {
  if (isnan(prev)) return cur;
  return prev + alpha * (cur - prev);
}

bool baro_begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  delay(10);

  // begin() devuelve bool en esta librería
  if (!g_baro.begin()) {
    g_ok = false;
    return false;
  }

  // reset() existe pero el valor de retorno no es estándar; no lo comparamos
  g_baro.reset();
  delay(50);

  // Oversampling (constantes de esta lib)
  g_baro.setOversampling(OSR_ULTRA_HIGH);

  g_ok   = true;
  g_last = BaroData{};
  return true;
}

void baro_poll() {
  if (!g_ok) return;

  // Dispara conversión y actualiza internos
  g_baro.read();

  // La librería puede devolver presión en hPa o Pa según versión.
  // Normalizamos a hPa: si es muy grande (>2000), asumimos Pa y dividimos.
  double tempC = g_baro.getTemperature();  // °C
  double pres  = g_baro.getPressure();     // hPa o Pa

  if (pres > 2000.0) pres /= 100.0;        // Pa -> hPa

  g_last.temperature_c = lp(g_last.temperature_c, (float)tempC, 0.2f);
  g_last.pressure_hpa  = lp(g_last.pressure_hpa,  (float)pres,  0.2f);
}

BaroData baro_last() {
  return g_last;
}
