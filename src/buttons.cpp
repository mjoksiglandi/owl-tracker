#include "buttons.h"
#include "board_pins.h"

// ---------------- Ajustes ----------------
static const uint16_t DEBOUNCE_MS    = 35;    // anti-rebote
static const uint16_t LONG_MS        = 700;   // largo estándar (BTN1/BTN2)
static const uint16_t LONG_SOS_MS    = 2000;  // largo SOS (BTN3)
static const uint16_t IGNORE_BOOT_MS = 250;   // ignora transitorios tras reset

struct Btn {
  uint8_t  pin;
  bool     rawLevel;       // lectura instantánea
  bool     stableLevel;    // nivel estable (con debounce)
  uint32_t lastEdgeMs;     // último cambio crudo
  uint32_t lastStableMs;   // última vez que se confirmaron niveles estables
  bool     pressed;        // estado lógico (LOW=presionado) estable
  uint32_t pressStartMs;   // cuándo inició la pulsación estable
  bool     firedLong;      // ya emitió evento de long
  uint16_t longThreshold;  // umbral de long
};

static Btn s_btn1{PIN_BTN1, HIGH, HIGH, 0, 0, false, 0, false, LONG_MS};
static Btn s_btn2{PIN_BTN2, HIGH, HIGH, 0, 0, false, 0, false, LONG_MS};
static Btn s_btn3{PIN_BTN3, HIGH, HIGH, 0, 0, false, 0, false, LONG_SOS_MS};

static ButtonEvent ev1, ev2, ev3;
static uint32_t bootMs = 0;

static void initBtn(Btn &b) {
  pinMode(b.pin, INPUT_PULLUP);     // activos en LOW
  b.rawLevel    = digitalRead(b.pin);
  b.stableLevel = b.rawLevel;
  b.lastEdgeMs  = millis();
  b.lastStableMs= millis();
  b.pressed     = (b.stableLevel == LOW);
  b.pressStartMs= 0;
  b.firedLong   = false;
}

void buttons_begin() {
  bootMs = millis();
  initBtn(s_btn1);
  initBtn(s_btn2);
  initBtn(s_btn3);
}

// procesa un botón y genera eventos de short/long
static void updateBtn(Btn &b, ButtonEvent &ev) {
  // evitar glitches al boot
  if (millis() - bootMs < IGNORE_BOOT_MS) return;

  bool lvl = digitalRead(b.pin);
  uint32_t now = millis();

  // Si cambia el nivel crudo, marca tiempo de rebote
  if (lvl != b.rawLevel) {
    b.rawLevel = lvl;
    b.lastEdgeMs = now;
  }

  // Si pasó el tiempo de debounce y difiere del estable -> confírmalo
  if ((now - b.lastEdgeMs) >= DEBOUNCE_MS && b.rawLevel != b.stableLevel) {
    b.stableLevel = b.rawLevel;
    b.lastStableMs = now;

    if (b.stableLevel == LOW) {
      // PRESIONADO (flanco descendente estable)
      b.pressed      = true;
      b.pressStartMs = now;
      b.firedLong    = false;
    } else {
      // LIBERADO (flanco ascendente estable)
      if (b.pressed) {
        // si estaba presionado y NO disparó long -> short
        if (!b.firedLong) {
          uint32_t dur = now - b.pressStartMs;
          if (dur >= 10) ev.shortPress = true; // umbral mínimo simbólico
        }
      }
      b.pressed      = false;
      b.pressStartMs = 0;
      b.firedLong    = false;
    }
  }

  // LONG (por tiempo, while pressed)
  if (b.pressed && !b.firedLong && b.pressStartMs) {
    if ((now - b.pressStartMs) >= b.longThreshold) {
      b.firedLong = true;
      ev.longPress = true;
    }
  }
}

void buttons_poll() {
  updateBtn(s_btn1, ev1);
  updateBtn(s_btn2, ev2);
  updateBtn(s_btn3, ev3);
}

// ---- Lectores (latched) ----
ButtonEvent btn1_get(){ ButtonEvent r = ev1; ev1 = {}; return r; }
ButtonEvent btn2_get(){ ButtonEvent r = ev2; ev2 = {}; return r; }
ButtonEvent btn3_get(){ ButtonEvent r = ev3; ev3 = {}; return r; }
