#pragma once
#include <Arduino.h>
#include <TinyGsmClient.h>
#include "net_config.h"
#include "board_pins.h"

enum class NetState : uint8_t {
  BOOT = 0,
  LTE_SET,          // set LTE-only (una vez)
  REG_CHECK,        // chequear registro periódicamente
  PDP_CHECK,        // levantar PDP si no hay IP
  READY,            // registrado + PDP activo
  BACKOFF,          // espera entre intentos
  POWER_CYCLE       // power-cycle del módem
};

struct NetFSM {
  TinyGsm*   modem = nullptr;
  NetState   st    = NetState::BOOT;
  uint32_t   next_ms = 0;
  uint16_t   backoff_ms = 2000;  // arranca corto
  uint8_t    fails = 0;
  bool       lte_set_done = false;

  void begin(TinyGsm& m) {
    modem = &m;
    st = NetState::BOOT;
    next_ms = 0;
    backoff_ms = 2000;
    fails = 0;
    lte_set_done = false;
  }

  // llama esto en loop() a alta frecuencia (no bloquea)
  void tick() {
    if (!modem) return;
    uint32_t now = millis();
    if ((int32_t)(now - next_ms) < 0) return;

    switch (st) {
      case NetState::BOOT: {
        // nada que hacer aquí: se supone que ya hiciste power_on y AT OK
        st = NetState::LTE_SET;
        next_ms = now;
      } break;

      case NetState::LTE_SET: {
        if (!lte_set_done) {
         modem_set_auto(*modem);
                lte_set_done = true;
              }
              st = NetState::REG_CHECK;
              next_ms = now;
            } break;

      case NetState::REG_CHECK: {
        // si ya está conectado a red y con IP => READY
        if (modem->isNetworkConnected()) {
          IPAddress ip = modem->localIP();
          if (ip != IPAddress(0,0,0,0)) { st = NetState::READY; next_ms = now + 2000; break; }
          else { st = NetState::PDP_CHECK; next_ms = now; break; }
        }
        // intenta registrar: simplemente consulta operador/estado para dar tiempo al módem
        (void) modem->getOperator();      // no bloquea mucho
        int csq = modem->getSignalQuality();
        (void) csq;

        // si tras varios ciclos no se registra, entra a backoff
        fails++;
        if (fails >= 6) { st = NetState::BACKOFF; next_ms = now + backoff_ms; fails = 0; }
        else            { next_ms = now + 3000; }
      } break;

      case NetState::PDP_CHECK: {
        IPAddress ip = modem->localIP();
        if (ip == IPAddress(0,0,0,0)) {
          bool ok = modem->gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
          if (!ok) { st = NetState::BACKOFF; next_ms = now + backoff_ms; break; }
        }
        st = NetState::READY;
        next_ms = now + 2000;
      } break;

      case NetState::READY: {
        // vigilar que no se caiga
        if (!modem->isNetworkConnected()) {
          st = NetState::REG_CHECK; next_ms = now; break;
        }
        IPAddress ip = modem->localIP();
        if (ip == IPAddress(0,0,0,0)) {
          st = NetState::PDP_CHECK; next_ms = now; break;
        }
        // quedarnos en READY, revisando cada cierto rato
        next_ms = now + 5000;
      } break;

      case NetState::BACKOFF: {
        // crecimiento del backoff hasta ~30s
        backoff_ms = min<uint16_t>(backoff_ms * 2, 30000);
        // si se acumulan muchos backoff consecutivos, hacer power-cycle
        fails++;
        if (fails >= 4) {
          st = NetState::POWER_CYCLE; fails = 0; next_ms = now;
        } else {
          st = NetState::REG_CHECK; next_ms = now + backoff_ms;
        }
      } break;

      case NetState::POWER_CYCLE: {
        // power-cycle suave del módem, NO bloqueante para UI
        digitalWrite(PIN_MODEM_EN, LOW); delay(600);
        modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1500, 7000);
        // reabrir UART (por si se cerró)
        // OJO: esto depende de tu main; si SerialAT es global, puedes reabrir aquí extern.
        // Mejor: deja que el main se encargue si es necesario.
        backoff_ms = 2000;
        lte_set_done = false;
        st = NetState::LTE_SET;
        next_ms = now + 500;
      } break;
    }
  }

  bool ready() const { return st == NetState::READY; }
  NetState state() const { return st; }
};
