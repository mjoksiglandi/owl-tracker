#pragma once
#include <Arduino.h>

struct BtnEvent {
  bool shortPress=false;
  bool longPress =false;
};

void   buttons_begin();
void   buttons_poll();
BtnEvent btn1_get();   // HOME / NEXT
BtnEvent btn2_get();   // MSG / POI
BtnEvent btn3_get();   // SOS
BtnEvent btn4_get();   // TESTING (modo)
