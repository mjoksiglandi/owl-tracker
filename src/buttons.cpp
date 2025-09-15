#include "buttons.h"

// Tiempos
static const uint16_t DEBOUNCE_MS  = 20;
static const uint16_t LONGPRESS_MS = 900;

// Estado interno por botón
struct BtnState {
  uint8_t  pin;
  bool     hasInternalPullup; // true si usamos INPUT_PULLUP
  bool     lastLevel;         // último nivel leído (true=HIGH)
  uint32_t tStable;           // t del último cambio estable (debounced)
  bool     pressed;           // estado estable (true=presionado)
  bool     longFired;         // ya se emitió longPress
  BtnEvent pending;           // evento pendiente a entregar
};

static BtnState s_b1, s_b2, s_b3, s_b4;

// Devuelve true si ese GPIO soporta INPUT_PULLUP y queremos usarlo
static bool wants_internal_pullup(uint8_t pin) {
  // En ESP32, GPIO34/35/36/39 no tienen pull-ups internos.
  // BTN1=25 sí lo tiene.
  if (pin == 34 || pin == 35 || pin == 36 || pin == 39) return false;
  return true;
}

static void init_btn(BtnState& b, uint8_t pin) {
  b.pin = pin;
  b.hasInternalPullup = wants_internal_pullup(pin);

  if (b.hasInternalPullup) pinMode(pin, INPUT_PULLUP);
  else                     pinMode(pin, INPUT);          // requiere pull-up externo a 3V3

  b.lastLevel = digitalRead(pin);
  b.tStable   = millis();
  b.pressed   = false;
  b.longFired = false;
  b.pending   = {};
}

void buttons_begin() {
  init_btn(s_b1, PIN_BTN1); // HOME/NEXT
  init_btn(s_b2, PIN_BTN2); // MSG/POI
  init_btn(s_b3, PIN_BTN3); // SOS
  init_btn(s_b4, PIN_BTN4); // TESTING
}

// Debounce y detección de eventos para un botón
static void poll_one(BtnState& b) {
  bool lvl = digitalRead(b.pin);

  // Debounce: esperar estabilidad DEBOUNCE_MS
  if (lvl != b.lastLevel) {
    b.lastLevel = lvl;
    b.tStable   = millis();      // reinicia ventana
    return;
  }
  if (millis() - b.tStable < DEBOUNCE_MS) return;

  // Con pulsadores a GND: PRESSED = nivel LOW
  bool isPressedLevel = (lvl == LOW);

  // Transiciones
  if (isPressedLevel && !b.pressed) {
    // Flanco de caída (inicio de pulsación)
    b.pressed   = true;
    b.longFired = false;
    // arrancamos cronómetro en tStable (ya debounced)
  } else if (!isPressedLevel && b.pressed) {
    // Flanco de subida (fin de pulsación)
    uint32_t heldMs = millis() - b.tStable; // tiempo desde el último cambio estable
    if (!b.longFired) {
      // Si no disparó long, es short
      b.pending.shortPress = true;
    }
    b.pressed   = false;
    b.longFired = false;
  } else if (isPressedLevel && b.pressed && !b.longFired) {
    // Pulsación sostenida: comprobar long
    if (millis() - b.tStable >= LONGPRESS_MS) {
      b.longFired = true;
      b.pending.longPress = true;
    }
  }
}

void buttons_poll() {
  poll_one(s_b1);
  poll_one(s_b2);
  poll_one(s_b3);
  poll_one(s_b4);
}

static BtnEvent take(BtnState& b) {
  BtnEvent e = b.pending;
  b.pending = {};
  return e;
}

BtnEvent btn1_get() { return take(s_b1); }
BtnEvent btn2_get() { return take(s_b2); }
BtnEvent btn3_get() { return take(s_b3); }
BtnEvent btn4_get() { return take(s_b4); }
