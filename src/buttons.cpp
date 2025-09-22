#include <Arduino.h>
#include "board_pins.h"
#include "buttons.h"

// ---------- Parámetros ----------
static const uint16_t DEBOUNCE_MS   = 20;    // anti-rebote
static const uint16_t SHORT_MIN_MS  = 80;    // mínimo para short
static const uint16_t SHORT_MAX_MS  = 250;   // máximo para short
static const uint16_t LONG_MS       = 600;   // umbral long
static const uint16_t REPEAT_MS     = 700;   // periodo auto-repeat tras long
static const uint32_t HOLD_MAX_MS   = 60000; // cap: 60 s

// Si tus botones son activos en HIGH, cambia a true:
static const bool BTN_ACTIVE_HIGH = false;   // false => activo LOW (pull-up)

// ---------- Estado interno ----------
struct BtnState {
  uint8_t pin;
  bool reading;          // lectura instantánea
  bool stable;           // lectura estable (tras debounce)
  uint32_t lastEdgeMs;   // última vez que cambió 'reading'
  uint32_t pressedAtMs;  // instante de presión estable
  bool longEmitted;      // ya se emitió longPress
  uint32_t lastRepeatMs; // última repetición emitida
  BtnEvent ev;           // evento a devolver este tick
};

static BtnState B[4];

static inline bool isActiveLevel(bool raw) {
  return BTN_ACTIVE_HIGH ? raw : !raw;
}

static void clearEv(BtnState& s) {
  s.ev.shortPress = false;
  s.ev.longPress  = false;
  s.ev.repeat     = false;
  s.ev.heldMs     = 0;
}

static void setupBtn(BtnState& s, uint8_t pin) {
  s.pin = pin;
  pinMode(pin, BTN_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  bool raw = digitalRead(pin);
  s.reading = raw;
  s.stable = raw;
  s.lastEdgeMs = millis();
  s.pressedAtMs = 0;
  s.longEmitted = false;
  s.lastRepeatMs = 0;
  clearEv(s);
}

void buttons_begin(){
  setupBtn(B[0], PIN_BTN1);
  setupBtn(B[1], PIN_BTN2);
  setupBtn(B[2], PIN_BTN3);
  setupBtn(B[3], PIN_BTN4);
}

static void pollOne(BtnState& s){
  clearEv(s);
  uint32_t now = millis();

  bool raw = digitalRead(s.pin);
  if (raw != s.reading) {          // flanco inmediato (no estable aún)
    s.reading = raw;
    s.lastEdgeMs = now;            // arranca ventana de debounce
  }

  // Debounce: si pasó DEBOUNCE_MS sin nuevas variaciones -> fijar estable
  if ((now - s.lastEdgeMs) >= DEBOUNCE_MS && s.stable != s.reading) {
    s.stable = s.reading;

    if (isActiveLevel(s.stable)) {
      // PRESSED estable
      s.pressedAtMs = now;
      s.longEmitted = false;
      s.lastRepeatMs = now;
    } else {
      // RELEASE estable
      if (s.pressedAtMs) {
        uint32_t held = now - s.pressedAtMs;
        if (held > HOLD_MAX_MS) held = HOLD_MAX_MS;
        s.ev.heldMs = held;
        if (held >= SHORT_MIN_MS && held <= SHORT_MAX_MS) {
          s.ev.shortPress = true;
        }
      }
      s.pressedAtMs = 0;
    }
  }

  // Mantención (hold)
  if (isActiveLevel(s.stable) && s.pressedAtMs) {
    uint32_t held = now - s.pressedAtMs;
    if (held > HOLD_MAX_MS) held = HOLD_MAX_MS;
    s.ev.heldMs = held;

    // Long (una sola vez)
    if (!s.longEmitted && held >= LONG_MS) {
      s.ev.longPress = true;
      s.longEmitted = true;
      s.lastRepeatMs = now;
    }

    // Auto-repeat mientras sigue presionado tras long
    if (s.longEmitted && held < HOLD_MAX_MS && (now - s.lastRepeatMs) >= REPEAT_MS) {
      s.ev.repeat = true;
      s.lastRepeatMs = now;
    }
  }
}

void buttons_poll(){
  pollOne(B[0]);
  pollOne(B[1]);
  pollOne(B[2]);
  pollOne(B[3]);
}

static BtnEvent pop(BtnState& s){
  BtnEvent e = s.ev;
  s.ev.shortPress = false;
  s.ev.longPress  = false;
  s.ev.repeat     = false;
  // heldMs se recalcula en el próximo poll si sigue presionado
  return e;
}

BtnEvent btn1_get(){ return pop(B[0]); }
BtnEvent btn2_get(){ return pop(B[1]); }
BtnEvent btn3_get(){ return pop(B[2]); }
BtnEvent btn4_get(){ return pop(B[3]); }
