#include "board_pins.h"
#include <Arduino.h>
#include <TinyGsmClient.h>

#include "modem_config.h"
#include "http_client.h"
#include "net_config.h"
#include "oled_display.h"
#include "gps.h"

// UART1: módem A7670G
HardwareSerial SerialAT(1);
// UART2: GPS GEPRC GEP-M1025-MI (u-blox M10)
HardwareSerial SerialGPS(2);

TinyGsm modem(SerialAT);
OwlHttpClient http;

const bool ENABLE_PUT = false;  // futuro: envío periódico HTTP

// ---- Diagnóstico AT crudo antes de TinyGSM (útil si el módem está dormido) ----
static bool rawAtOK(Stream& s, uint8_t tries = 15, uint16_t gap_ms = 200) {
  while (tries--) {
    s.print("AT\r");
    uint32_t t0 = millis();
    String buf;
    while (millis() - t0 < 300) {
      while (s.available()) buf += (char)s.read();
    }
    if (buf.indexOf("OK") >= 0) return true;
    delay(gap_ms);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(150);

    // --- OLED ---
  oled_init();
  oled_splash("Owl Tracker");

  // --- POWER ON del módem (EN HIGH + pulso PWRKEY) ---
  Serial.println("[Owl] Powering A7670G...");
  modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, /*activeHigh=*/true, /*pressMs=*/1200, /*settleMs=*/6000);

  // --- UART hacia el módem ---
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
  delay(50);

  // --- Diagnóstico AT crudo ---
  if (!rawAtOK(SerialAT, 20, 200)) {
    Serial.println("[Owl] AT no responde. Power cycle…");
    digitalWrite(PIN_MODEM_EN, LOW); delay(500);
    modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1500, 7000);
    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
    delay(100);
    if (!rawAtOK(SerialAT, 20, 250)) {
      Serial.println("[Owl] ERROR: el módem no responde AT.");
      while (true) delay(1000);
    }
  }

  // --- TinyGSM init ---
  Serial.println("[Owl] Inicializando módem (TinyGSM)...");
  if (!modem_init(modem)) {
    Serial.println("[Owl] No se pudo inicializar modem");
    while (true) delay(1000);
  }

  // --- Preferir LTE only ---
  if (!modem_set_lte(modem)) {
    Serial.println("[Owl] ⚠ LTE no confirmado, fallback AUTO");
    modem_set_auto(modem);
  }

  // --- Registro de red ---
  if (!modem_wait_for_network(modem, 60000)) {
    Serial.println("[Owl] Sin registro de red");
    while (true) delay(1000);
  }

  // --- PDP con WOM ---
  bool gprs_up = modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
  Serial.printf("[Owl] PDP %s\n", gprs_up ? "ACTIVO" : "FALLÓ");
  if (gprs_up) {
    IPAddress ip = modem.localIP();
    Serial.print("[Owl] IP: "); Serial.println(ip);
  }

  // --- HTTP (por ahora sin TLS) ---
  http.begin(modem, netcfg::HOST, netcfg::PORT, /*root_ca=*/nullptr);

  // --- GPS (u-blox M10) por UART2 ---
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);


}

void loop() {
  // Alimenta el parser de GPS continuamente
  gps_poll(SerialGPS);

  // Refresco de UI cada 1s
  static uint32_t lastUi = 0;
  if (millis() - lastUi >= 1000) {
    lastUi = millis();

    // Datos del módem
    int csq = modem.getSignalQuality();
    String ip = modem.localIP().toString(); // ""/0.0.0.0 -> OLED muestra "SIN PDP"

    // Datos del GPS (NMEA)
    GpsFix fix = gps_last_fix();

    // Construir UI
    OwlUiData ui;
    ui.csq  = csq;
    ui.ip   = ip;
    ui.iridiumLvl = -1;                 // reservado para Iridium
    ui.lat  = fix.valid ? fix.lat : NAN;
    ui.lon  = fix.valid ? fix.lon : NAN;
    ui.sats = fix.sats;
    ui.pdop = fix.pdop;                 // -1 si aún no llegó GSA
    ui.msgRx = 0;                       // pon aquí tu contador real de mensajes
    // Hora UTC desde GPS (por ahora HH:MM:SS de GGA). Cuando integremos RMC/NTP, traerá fecha completa.
    ui.utc = fix.utc.length() ? (fix.utc + "Z") : String("--:--:--Z");

    // Pintar OLED
    oled_draw_dashboard(ui);
    Serial.println("gps: " + String(fix.lat, 6) + ", " + String(fix.lon, 6));
    Serial.println("csq: " + String(csq) + ", ip: " + ip);
    Serial.println("[Owl] UI refrescada");
  }

  // (Opcional) envío periódico HTTP
  // if (ENABLE_PUT) {
  //   // Construye tu JSON y llama http.putJson(netcfg::PATH, payload, netcfg::AUTH_BEARER);
  //   delay(netcfg::PUT_PERIOD_MS);
  // }
}
