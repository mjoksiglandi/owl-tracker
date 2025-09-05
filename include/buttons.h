#pragma once
#include <Arduino.h>

struct ButtonEvent {
  bool shortPress = false;   // flanco de liberación si duró < longThreshold
  bool longPress  = false;   // flanco cuando excede longThreshold
};

// init / polling
void buttons_begin();
void buttons_poll();

// lectura de eventos (latched: se limpian al leer)
ButtonEvent btn1_get();
ButtonEvent btn2_get();
ButtonEvent btn3_get();
