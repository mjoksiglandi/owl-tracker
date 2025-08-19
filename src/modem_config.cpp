#include "modem_config.h"

// --- Helpers de AT directos sobre TinyGSM ---
static bool at_ok(TinyGsm& m, const char* cmd, uint32_t tout = 3000) {
  m.sendAT(cmd);
  int r = m.waitResponse(tout);
  Serial.print("[Owl][AT] "); Serial.print(cmd); Serial.print(" -> ");
  Serial.println(r == 1 ? "OK" : "ERR");
  return r == 1;
}

// --- Power management ---
void modem_power_on(int pinEN, int pinPWR, bool activeHigh,
                    uint16_t pressMs, uint16_t settleMs) {
  pinMode(pinEN, OUTPUT);
  digitalWrite(pinEN, HIGH);   // habilita el módulo

  pinMode(pinPWR, OUTPUT);
  // nivel de reposo: opuesto al pulso
  digitalWrite(pinPWR, activeHigh ? LOW : HIGH);
  delay(100);

  // pulso de encendido
  digitalWrite(pinPWR, activeHigh ? HIGH : LOW);
  delay(pressMs);
  digitalWrite(pinPWR, activeHigh ? LOW : HIGH);
  delay(settleMs);

  Serial.printf("[Owl] Power: EN=HIGH, PWRKEY pulse %s (%ums + %ums)\n",
                activeHigh ? "HIGH" : "LOW", pressMs, settleMs);
}

// --- TinyGSM Bring-up ---
bool modem_init(TinyGsm &m) {
  Serial.println("[Owl] Inicializando módem...");
  if (!m.init()) {
    Serial.println("[Owl] init() falló, probando restart()");
    if (!m.restart()) {
      Serial.println("[Owl] ERROR: no se pudo inicializar el módem");
      return false;
    }
  }
  return true;
}

bool modem_set_lte(TinyGsm &m) {
  if (!at_ok(m, "+CNMP=38")) return false;
  m.sendAT("+CNMP?");
  if (m.waitResponse(3000, "+CNMP: 38") == 1) {
    Serial.println("[Owl] RAT = LTE only confirmado");
    at_ok(m, "&W"); // persistir si el firmware lo soporta
    return true;
  }
  Serial.println("[Owl] Aviso: no se pudo confirmar LTE only");
  return false;
}

bool modem_set_auto(TinyGsm &m) {
  if (!at_ok(m, "+CNMP=2")) return false;
  m.sendAT("+CNMP?");
  if (m.waitResponse(3000, "+CNMP: 2") == 1) {
    Serial.println("[Owl] RAT = AUTO confirmado");
    return true;
  }
  return false;
}

// ...

bool modem_wait_for_network(TinyGsm &m, uint32_t timeout_ms) {
  Serial.println("[Owl] Esperando registro de red...");
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (m.isNetworkConnected()) {
      Serial.println("[Owl] Red conectada ✅");
      modem_print_status(m);
      return true;
    }
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n[Owl] Timeout de registro.");
  return false;
}

void modem_print_status(TinyGsm &m) {
  String op = m.getOperator();
  int rssi = m.getSignalQuality();
  Serial.printf("[Owl] Operador: %s\n", op.c_str());
  Serial.printf("[Owl] RSSI: %d dBm\n", rssi);

  // Consultar RAT (AT+CNMP?)
  m.sendAT("+CNMP?");
  if (m.waitResponse(3000, "+CNMP:") == 1) {
    String resp = m.stream.readStringUntil('\n');
    resp.trim();
    Serial.printf("[Owl] RAT: %s\n", resp.c_str());
  }
}

