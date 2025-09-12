#include "buttons.h"

// PINS (ajusta si ya los tenías):
#ifndef PIN_BTN1
#define PIN_BTN1 13
#endif
#ifndef PIN_BTN2
#define PIN_BTN2 14
#endif
#ifndef PIN_BTN3
#define PIN_BTN3 27
#endif
#ifndef PIN_BTN4
#define PIN_BTN4 15   // NUEVO: Testing
#endif

// ... reutiliza tu implementación existente …
// duplica el patrón de tu 1..3 para 4

// ejemplo simple (si usabas este estilo):
static BtnEvent e1,e2,e3,e4;
static uint32_t t1,t2,t3,t4;
static bool     s1,s2,s3,s4;

static void poll_one(uint8_t pin, uint32_t& t0, bool& st, BtnEvent& ev) {
  bool v = (digitalRead(pin)==LOW);
  ev = {};
  static const uint16_t LONG_MS=800, DEBOUNCE=20;
  static uint32_t last=0; // (compartido es suficiente aquí)

  if (millis()-last < DEBOUNCE) return;
  last=millis();

  if (v && !st) { st=true; t0=millis(); }
  else if (!v && st) {
    st=false;
    uint32_t dt = millis()-t0;
    if (dt > LONG_MS) ev.longPress=true;
    else              ev.shortPress=true;
  }
}

void buttons_begin(){
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_BTN3, INPUT_PULLUP);
  pinMode(PIN_BTN4, INPUT_PULLUP); // NUEVO
}

void buttons_poll(){
  poll_one(PIN_BTN1,t1,s1,e1);
  poll_one(PIN_BTN2,t2,s2,e2);
  poll_one(PIN_BTN3,t3,s3,e3);
  poll_one(PIN_BTN4,t4,s4,e4); // NUEVO
}

BtnEvent btn1_get(){ BtnEvent t=e1; e1={}; return t; }
BtnEvent btn2_get(){ BtnEvent t=e2; e2={}; return t; }
BtnEvent btn3_get(){ BtnEvent t=e3; e3={}; return t; }
BtnEvent btn4_get(){ BtnEvent t=e4; e4={}; return t; } // NUEVO
