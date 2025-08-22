#include "board_pins.h"
#include <Arduino.h>
#include <TinyGsmClient.h>

#include "modem_config.h"
#include "http_client.h"
#include "net_config.h"
#include "oled_display.h"
#include "gps.h"
#include "baro.h"
#include "mag.h"

// SD (HSPI)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
SPIClass hspi(HSPI);
static bool sd_ok=false;

static bool sd_begin_hspi(){
  hspi.end();
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if(!SD.begin(PIN_SD_CS, hspi)) return false;
  return SD.cardType()!=CARD_NONE;
}
static void sd_logln(const String& line){
  if(!sd_ok) return;
  File f = SD.open("/logs/track.csv", FILE_APPEND);
  if(!f){ SD.mkdir("/logs"); f = SD.open("/logs/track.csv", FILE_APPEND); }
  if(f){ f.println(line); f.close(); }
}

// UARTs
HardwareSerial SerialAT(1);   // módem
HardwareSerial SerialGPS(2);  // GNSS
TinyGsm modem(SerialAT);
OwlHttpClient http;

// AT crudo ping
static bool rawAtOK(Stream& s, uint8_t tries=15, uint16_t gap_ms=200){
  while(tries--){
    s.print("AT\r");
    uint32_t t0=millis(); String buf;
    while(millis()-t0<300){ while(s.available()) buf+=(char)s.read(); }
    if(buf.indexOf("OK")>=0) return true;
    delay(gap_ms);
  }
  return false;
}

void setup(){
  Serial.begin(115200);
  delay(150);
  Serial.println("\n[Owl] Boot");

  // Power on módem
  modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1200, 6000);

  // UART módem
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
  delay(50);
  if(!rawAtOK(SerialAT,20,200)){
    Serial.println("[Owl] AT no responde, power-cycle");
    digitalWrite(PIN_MODEM_EN, LOW); delay(600);
    modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1500, 7000);
    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
    delay(100);
    (void)rawAtOK(SerialAT,20,250);
  }

  // TinyGSM
  if(!modem_init(modem)) Serial.println("[Owl] TinyGSM init falló");
  if(!modem_set_lte(modem)){ Serial.println("[Owl] LTE only falló -> AUTO"); modem_set_auto(modem); }
  (void)modem_wait_for_network(modem, 30000);
  (void)modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
  http.begin(modem, netcfg::HOST, netcfg::PORT, nullptr);

  // GNSS
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
  gps_set_stale_timeout(10000);

  // OLED
  oled_init();

  // I2C: Baro + Mag
  baro_begin();
  mag_begin();

  // SD
  sd_ok = sd_begin_hspi();
  Serial.printf("[Owl] SD %s\n", sd_ok? "OK":"NO");
}

void loop(){
  // Sensores no bloqueantes
  gps_poll(SerialGPS);
  baro_poll();
  mag_poll();

  // Supervisor de red cada 10 s
  static uint32_t t_net=0;
  if(millis()-t_net>10000){
    t_net=millis();
    if(!modem.isNetworkConnected()) (void)modem_wait_for_network(modem, 10000);
    if(modem.localIP()==IPAddress(0,0,0,0)) (void)modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
  }

  // UI 1 Hz
  static uint32_t t_ui=0;
  if(millis()-t_ui>=1000){
    t_ui=millis();
    GpsFix fx = gps_last_fix();
    BaroData bd = baro_last();

    OwlUiData ui;
    ui.csq = modem.getSignalQuality();
    ui.iridiumLvl = -1;                 // IR no implementado
    ui.lat = fx.valid? fx.lat : NAN;
    ui.lon = fx.valid? fx.lon : NAN;
    ui.alt = fx.valid? fx.alt : NAN;
    ui.sats = fx.sats;
    ui.pdop = fx.pdop;
    ui.speed_mps  = fx.speed_mps;
    ui.course_deg = fx.course_deg;
    ui.pressure_hpa = bd.pressure_hpa;
    ui.msgRx = 0;
    ui.utc = fx.utc.length()? (fx.utc+"Z") : String("");

    oled_draw_dashboard(ui);
    Serial.println("[Owl] UI refrescada");
  }

  // Log simple a SD cada 30 s (opcional)
  static uint32_t t_log=0;
  if(sd_ok && millis()-t_log>30000){
    t_log=millis();
    GpsFix fx = gps_last_fix();
    String s = String(fx.utc)+","+
               (fx.valid? String(fx.lat,6):"NaN")+","+
               (fx.valid? String(fx.lon,6):"NaN")+","+
               (fx.valid? String(fx.alt,1):"NaN");
    sd_logln(s);
  }
}
