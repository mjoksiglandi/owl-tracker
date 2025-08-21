#pragma once
#include <Arduino.h>

// Datos que pintaremos en la UI
struct OwlUiData {
  int    csq = 99;          // 0..31 (99 = desconocido)
  int    iridiumLvl = -1;   // 0..5 (futuro)  -1 = sin dato
  int    sats = -1;         // satélites en uso
  float  pdop = -1.0f;      // PDOP
  double lat  = NAN;        // grados decimales
  double lon  = NAN;        // grados decimales
  double alt  = NAN;        // metros (MSL)
  uint32_t msgRx = 0;       // contador de mensajes recibidos
  String utc = "";          // "YYYY-MM-DD HH:MM:SSZ"
};

// Inicialización del OLED (SSD1322 256x64 SPI)
bool oled_init();

// Dibuja el dashboard (siempre visible)
void oled_draw_dashboard(const OwlUiData& d);

inline char pdopToGrade(float pdop) {
  if (pdop < 0) return '-';       // sin datos
  if (pdop < 1.5f) return 'E';    // Excelente
  if (pdop < 4.0f) return 'B';    // Bueno
  if (pdop < 6.0f) return 'M';    // Moderado
  if (pdop < 10.0f) return 'A';   // Aceptable
  return 'D';                     // Deficiente
}

