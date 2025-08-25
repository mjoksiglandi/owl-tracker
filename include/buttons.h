#pragma once
#include <Arduino.h>

struct ButtonEvent {
  bool shortPress = false;
  bool longPress  = false;
};

void buttons_begin();               // config de pines
void buttons_poll();                // leer + debouncing (llamar cada ~10ms)

// Eventos latched (se consumen al leer)
ButtonEvent btn1_get();             // Home/Menu
ButtonEvent btn2_get();             // Msg/POI
ButtonEvent btn3_get();             // SOS (solo largo)
