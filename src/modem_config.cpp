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

String modem_rat_label(TinyGsm &m) {
  // Pregunta al módem su estado de portadora / RAT
  // SIM7600: AT+CPSI? -> contiene "GSM"/"WCDMA"/"LTE"/"NR5G"
  m.sendAT("+CPSI?");
  if (m.waitResponse(3000, "+CPSI:") == 1) {
    String line = m.stream.readStringUntil('\n');
    line.toUpperCase();
    // Heurística simple -> etiqueta corta tipo celulares
    if (line.indexOf("NR5G")   >= 0) return "5G";
    if (line.indexOf("LTE")    >= 0) return "4G";
    if (line.indexOf("WCDMA")  >= 0) return "3G";
    if (line.indexOf("HSPA")   >= 0) return "3G";
    if (line.indexOf("UMTS")   >= 0) return "3G";
    if (line.indexOf("GSM")    >= 0) return "2G";
  }
  return "--";
}

bool modem_radio_tune(TinyGsm &m) {
  // RAT AUTO (ya lo haces en tu bring-up)
  (void)modem_set_auto(m); // ignora si falla

  // Desactivar PSM y eDRX (algunas celdas los habilitan y “duermen” el módem)
  // SIMCom acepta CPSMS=0 y CEDRXS=0
  m.sendAT("+CPSMS=0"); m.waitResponse(3000);
  m.sendAT("+CEDRXS=0"); m.waitResponse(3000);

  // Asegurar operador automático (por si quedó forzado)
  m.sendAT("+COPS=0"); m.waitResponse(10000);

  return true;
}

bool modem_pdp_hard_reset(TinyGsm &m, const char* apn, const char* user, const char* pass) {
  // 1) Desconecta GPRS si está arriba
  m.gprsDisconnect();

  // 2) Desatacha de PS y vuelve a atachar
  m.sendAT("+CGATT=0"); m.waitResponse(5000);
  delay(1000);
  m.sendAT("+CGATT=1"); m.waitResponse(10000);

  // 3) Reconfigura el contexto (CID=1) por si quedó “sucio”
  //    NB: TinyGSM también lo hace, pero re-declararlo ayuda en SIMCom
  m.sendAT("+CGDCONT=1,\"IP\",\"", apn, "\"");
  m.waitResponse(3000);

  // 4) Reconecta PDP
  bool up = m.gprsConnect(apn, user, pass);
  return up;
}

bool modem_radio_recover(TinyGsm &m, const char* apn, const char* user, const char* pass) {
  // Secuencia más pesada si hay flapping
  // a) Tune básico
  modem_radio_tune(m);

  // b) Ciclar CFUN si sigue mal (opcional, varias redes lo requieren)
  m.sendAT("+CFUN=0"); m.waitResponse(10000);
  delay(1500);
  m.sendAT("+CFUN=1"); m.waitResponse(10000);

  // c) Esperar registro con ventana realista (hasta 45–60 s)
  if (!modem_wait_for_network(m, 45000)) {
    Serial.println("[Owl] Recover: no network after CFUN cycle");
    return false;
  }

  // d) Hard reset PDP
  bool ok = modem_pdp_hard_reset(m, apn, user, pass);
  Serial.printf("[Owl] Recover: PDP %s\n", ok ? "UP" : "DOWN");
  return ok;
}

void modem_dump_regs(TinyGsm &m) {
  // Estado de registro CS/PS para 2G/3G/LTE/NR
  m.sendAT("+CREG?");  if (m.waitResponse(3000, "+CREG:")  == 1)  Serial.println(m.stream.readStringUntil('\n'));
  m.sendAT("+CGREG?"); if (m.waitResponse(3000, "+CGREG:") == 1)  Serial.println(m.stream.readStringUntil('\n'));
  m.sendAT("+CEREG?"); if (m.waitResponse(3000, "+CEREG:") == 1)  Serial.println(m.stream.readStringUntil('\n'));
  // RAT/portadora
  m.sendAT("+CPSI?");  if (m.waitResponse(3000, "+CPSI:")  == 1)  Serial.println(m.stream.readStringUntil('\n'));
  // Operador y CSQ
  Serial.printf("[Owl] Operador=%s\n", m.getOperator().c_str());
  Serial.printf("[Owl] CSQ=%d\n", m.getSignalQuality());
}
