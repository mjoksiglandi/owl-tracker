#pragma once
#include <Arduino.h>

// Evento estándar para cada botón
struct BtnEvent {
  bool shortPress;   // true en el tick en que se suelta tras 80..250 ms
  bool longPress;    // true una sola vez cuando se cruza el umbral de long (>= 600 ms)
  bool repeat;       // true cada 700 ms mientras se mantiene tras el longPress (auto-repeat)
  uint32_t heldMs;   // ms que lleva presionado (cap a 60000 ms)
};

// Inicialización (configura pines y estado interno)
void buttons_begin();

// Debounce + FSM, llámalo ~cada 2–10 ms
void buttons_poll();

// Getters por botón (devuelven y limpian el evento del tick)
BtnEvent btn1_get();
BtnEvent btn2_get();
BtnEvent btn3_get();
BtnEvent btn4_get();
