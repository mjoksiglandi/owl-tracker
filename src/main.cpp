#include <Arduino.h>
#include <TinyGsmClient.h>
#include "oled_display.h"
#include "modem_config.h"
#include "http_client.h"
#include "net_config.h"

// UART para el módem (T-A7670G)
#define MODEM_TX  26
#define MODEM_RX  27
#define MODEM_PWR 4
#define MODEM_EN  12   //   EN activo-ALTO

HardwareSerial SerialAT(1);
TinyGsm modem(SerialAT);
OwlHttpClient http;

const bool ENABLE_PUT = false;

void setup() {
  Serial.begin(115200);
  delay(200);
  oled_init();
  oled_splash("Owl Tracker");

  // UART al módem
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  // *** ENCENDER EL MÓDEM ***
  // (para esta placa: pulso activo-ALTO, y mejor un settle largo)
  modem_power_on(MODEM_EN, MODEM_PWR, /*activeHigh=*/true, /*pressMs=*/1200, /*settleMs=*/4000);
  delay(500); // pequeño respiro

  // Bring-up TinyGSM
  if (!modem_init(modem)) {
    Serial.println("[Owl] No se pudo inicializar modem");
    while (true) delay(1000);
  }

  // Preferir LTE
  if (!modem_set_lte(modem)) {
    Serial.println("[Owl] ⚠ LTE no confirmado, probando fallback a AUTO");
    modem_set_auto(modem);
  }

  // Registro de red + prints de operador/RSSI/RAT
  if (!modem_wait_for_network(modem, 60000)) {
    Serial.println("[Owl] Sin registro de red");
    while (true) delay(1000);
  }

  // PDP con WOM
  bool gprs_up = modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
  Serial.printf("[Owl] PDP %s\n", gprs_up ? "ACTIVO" : "FALLÓ");
  if (gprs_up) {
    IPAddress ip = modem.localIP();
    Serial.print("[Owl] IP: "); Serial.println(ip);
  }

  // HTTP client (ahora mismo sin TLS)
  http.begin(modem, netcfg::HOST, netcfg::PORT, netcfg::ROOT_CA);

  OwlUiData ui;
    ui.csq = modem.getSignalQuality();
    ui.ip  = modem.localIP().toString();         // "0.0.0.0" o "" => mostrará SIN PDP
    ui.iridiumLvl = -1;                          // 0..5 cuando lo integres
    ui.lat = NAN; ui.lon = NAN;                  // rellena cuando integres GNSS
    ui.sats = -1; ui.pdop = -1.0f;
    ui.msgRx = 0;
    ui.utc   = "2025-08-20 12:34:56Z";           // luego lo alimentamos con NTP/RTC

    oled_draw_dashboard(ui);
}

void loop() {
  // refresco cada 1s (ejemplo)
  static uint32_t t0 = 0;
  if (millis() - t0 >= 1000) {
    t0 = millis();

    OwlUiData ui;
    ui.csq = modem.getSignalQuality();
    ui.ip  = modem.localIP().toString();
    ui.iridiumLvl = -1;
    // GNSS futuro:
    // ui.lat = <tu_lat>; ui.lon = <tu_lon>;
    // ui.sats = <n_sats>; ui.pdop = <pdop>;
    ui.msgRx = /* tu contador real */ 0;
    ui.utc   = /* tu cadena UTC */ "2025-08-20 12:34:56Z";

    oled_draw_dashboard(ui);
  }
}