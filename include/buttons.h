#pragma once
#include <Arduino.h>

// ---- Pines (si ya los defines en board_pins.h, puedes omitir esto) ----
#ifndef PIN_BTN1
  #define PIN_BTN1 25   // HOME / Menús (tiene pull-up interno)
#endif
#ifndef PIN_BTN2
  #define PIN_BTN2 34   // MSG / POI (solo input: requiere pull-up externo)
#endif
#ifndef PIN_BTN3
  #define PIN_BTN3 35   // SOS       (solo input: requiere pull-up externo)
#endif
#ifndef PIN_BTN4
  #define PIN_BTN4 39   // TESTING   (solo input: requiere pull-up externo)
#endif

struct BtnEvent {
  bool shortPress = false;
  bool longPress  = false;
};

// Inicialización (pone modos correctos por pin)
void buttons_begin();

// Llamar cada ~10ms desde loop()
void buttons_poll();

// Obtención y consumo de eventos (clears)
BtnEvent btn1_get();   // HOME / NEXT
BtnEvent btn2_get();   // MSG / POI
BtnEvent btn3_get();   // SOS
BtnEvent btn4_get();   // TESTING (modo)
