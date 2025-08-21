#include "board_pins.h"
#include <Arduino.h>
#include <TinyGsmClient.h>

#include "modem_config.h"
#include "http_client.h"
#include "net_config.h"
#include "oled_display.h"
#include "gps.h"

// UART1: módem
HardwareSerial SerialAT(1);
// UART2: GPS
HardwareSerial SerialGPS(2);

TinyGsm modem(SerialAT);
OwlHttpClient http;

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

    // OLED
  oled_init();

  // POWER ON módem
  modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, /*activeHigh=*/true, /*pressMs=*/1200, /*settleMs=*/6000);

  // UART módem
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
  delay(50);

  // Diagnóstico AT
  if (!rawAtOK(SerialAT, 20, 200)) {
    Serial.println("[Owl] AT no responde. Power cycle…");
    digitalWrite(PIN_MODEM_EN, LOW); delay(500);
    modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1500, 7000);
    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
    delay(100);
    (void)rawAtOK(SerialAT, 20, 250);
  }

  // TinyGSM
  if (!modem_init(modem)) {
    Serial.println("[Owl] No se pudo inicializar modem");
  }
  if (!modem_set_lte(modem)) modem_set_auto(modem);
  (void)modem_wait_for_network(modem, 30000);
  (void)modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);

  // HTTP (sin TLS por ahora)
  http.begin(modem, netcfg::HOST, netcfg::PORT, nullptr);

  // GPS
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
  gps_set_stale_timeout(10000);

}

void loop() {
  // Feed GPS
  gps_poll(SerialGPS);

  // Refresco de UI cada 1s
  static uint32_t lastUi = 0;
  if (millis() - lastUi >= 1000) {
    lastUi = millis();

GpsFix fix = gps_last_fix();

  OwlUiData ui;
  ui.csq  = modem.getSignalQuality();
  ui.iridiumLvl = -1;                    // <- fuerza "sin dato" en IR
  ui.lat  = fix.valid ? fix.lat : NAN;
  ui.lon  = fix.valid ? fix.lon : NAN;
  ui.alt  = fix.valid ? fix.alt : NAN;
  ui.sats = fix.sats;
  ui.pdop = fix.pdop;
  ui.msgRx = 0;
  ui.utc  = fix.utc.length() ? (fix.utc + "Z") : String("");  // <- UTC visible

  oled_draw_dashboard(ui);
    Serial.println("[Owl] UI refrescada");
  }
}
