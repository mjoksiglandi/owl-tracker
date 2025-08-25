#include "buttons.h"
#include "board_pins.h"

// ------- Ajustes de robustez -------
static const uint16_t DEBOUNCE_MS   = 40;     // anti-rebote
static const uint16_t LONG_MS       = 700;    // largo normal
static const uint16_t LONG_SOS_MS   = 2000;   // SOS
static const uint16_t IGNORE_BOOT_MS= 800;    // ignora lecturas al inicio

struct Btn {
  uint8_t  pin;
  bool     level;           // HIGH=idle (pull-up externo), LOW=pressed
  bool     stableLevel;
  uint32_t lastChangeMs;
  uint32_t pressStartMs;
  bool     armedShort;
  bool     armedLong;
  uint16_t longThreshold;
};

static Btn s_btn1{PIN_BTN1, HIGH, HIGH, 0, 0, false, false, LONG_MS};
static Btn s_btn2{PIN_BTN2, HIGH, HIGH, 0, 0, false, false, LONG_MS};
static Btn s_btn3{PIN_BTN3, HIGH, HIGH, 0, 0, false, false, LONG_SOS_MS};

static ButtonEvent ev1, ev2, ev3;
static uint32_t bootMs = 0;

static void initBtn(Btn &b) {
  pinMode(b.pin, INPUT);            // input-only; requiere pull-up EXTERNO
  b.level = b.stableLevel = digitalRead(b.pin);
  b.lastChangeMs = millis();
  b.pressStartMs = 0;
  b.armedShort = b.armedLong = false;
}

void buttons_begin() {
  bootMs = millis();
  initBtn(s_btn1);
  initBtn(s_btn2);
  initBtn(s_btn3);
}

static bool readStable(uint8_t pin) {
  // Muestreo triple para suprimir glitch
  bool a = digitalRead(pin);
  delayMicroseconds(80);
  bool b = digitalRead(pin);
  delayMicroseconds(80);
  bool c = digitalRead(pin);
  return (a && b) || (a && c) || (b && c);
}

static void updateBtn(Btn &b, ButtonEvent &ev) {
  // Ignora ruido al inicio (cables largos / sin pull-up en boot)
  if (millis() - bootMs < IGNORE_BOOT_MS) return;

  bool lvl = readStable(b.pin);
  uint32_t now = millis();

  if (lvl != b.level) {
    b.level = lvl;
    b.lastChangeMs = now;
  }

  // Debounce
  if (now - b.lastChangeMs >= DEBOUNCE_MS && b.level != b.stableLevel) {
    b.stableLevel = b.level;

    if (b.stableLevel == LOW) {
      // PRESSED
      b.pressStartMs = now;
      b.armedShort   = false;
      b.armedLong    = false;
    } else {
      // RELEASED -> decide corto si no hubo largo
      if (b.pressStartMs && !b.armedLong) {
        uint32_t dur = now - b.pressStartMs;
        if (dur < b.longThreshold) b.armedShort = true;
      }
      b.pressStartMs = 0;
    }
  }

  // LONG PRESS por flanco (cuando supera el umbral)
  if (b.pressStartMs && !b.armedLong) {
    uint32_t dur = now - b.pressStartMs;
    if (dur >= b.longThreshold) {
      b.armedLong  = true;
      b.armedShort = false; // anula corto
    }
  }

  // Publicar eventos latched (se consumen en *_get)
  if (b.armedShort) { ev.shortPress = true; b.armedShort = false; }
  if (b.armedLong)  { ev.longPress  = true; b.armedLong  = false; }
}

void buttons_poll() {
  updateBtn(s_btn1, ev1);
  updateBtn(s_btn2, ev2);
  updateBtn(s_btn3, ev3);
}

ButtonEvent btn1_get(){ ButtonEvent r=ev1; ev1={}; return r; }
ButtonEvent btn2_get(){ ButtonEvent r=ev2; ev2={}; return r; }
ButtonEvent btn3_get(){ ButtonEvent r=ev3; ev3={}; return r; }
