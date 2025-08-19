#include <Arduino.h>
#include <TinyGsmClient.h>
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
}

void loop() {
  // if (ENABLE_PUT) {
  //   http.putJson(netcfg::PATH, "{\"status\":\"ok\"}", netcfg::AUTH_BEARER);
  // }
  // delay(netcfg::PUT_PERIOD_MS);
}
