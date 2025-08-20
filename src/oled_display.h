#pragma once
#include <Arduino.h>

// ==== Pines SPI por defecto (puedes cambiarlos o moverlos a board_pins.h) ====
#ifndef OLED_PIN_CS
  #define OLED_PIN_CS    5
#endif
#ifndef OLED_PIN_DC
  #define OLED_PIN_DC    21
#endif
#ifndef OLED_PIN_RST
  #define OLED_PIN_RST   22
#endif
#ifndef OLED_PIN_SCK
  #define OLED_PIN_SCK   18
#endif
#ifndef OLED_PIN_MOSI
  #define OLED_PIN_MOSI  23
#endif

// ==== Datos a renderizar ====
struct OwlUiData {
  // Celular
  int     csq        = 99;           // 0..31, 99 = desconocido
  String  ip         = "";           // "a.b.c.d" o vacío/"0.0.0.0" => SIN PDP
  // Iridium (futuro)
  int     iridiumLvl = -1;           // -1 = N/A, 0..5 = barras
  // GNSS (futuro)
  double  lat        = NAN;          // grados
  double  lon        = NAN;
  int     sats       = -1;           // -1 = N/A
  float   pdop       = -1.0f;        // <0 = N/A
  // Mensajería
  uint32_t msgRx     = 0;            // contador
  // Tiempo (UTC)
  String  utc        = "--:--:--Z";  // ejemplo: "2025-08-20 12:34:56Z"
};

// ==== API OLED ====
bool oled_init();                                // inicia HW SPI + OLED
void oled_splash(const char* title = "Owl");     // splash simple
void oled_draw_dashboard(const OwlUiData& d);    // UI estilo celular

// ==== Helpers expuestos (por si los usas fuera) ====
int  csq_to_dbm(int csq);   // CSQ -> dBm aprox.
int  csq_to_bars(int csq);  // CSQ -> 0..5 barras
