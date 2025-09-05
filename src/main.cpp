#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <TinyGsmClient.h>

// Núcleo
#include "modem_config.h"
#include "http_client.h"
#include "net_config.h"

// Sensores / UI
#include "oled_display.h"
#include "gps.h"
#include "baro.h"
#include "mag.h"
#include "ui_state.h"
#include "buttons.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//==============================
// UARTs
//==============================
HardwareSerial SerialAT(1);   // Módem A7670G
HardwareSerial SerialGPS(2);  // GNSS externo

// Objetos principales
TinyGsm modem(SerialAT);
OwlHttpClient http;
SPIClass hspi(HSPI);

//==============================
// Estado global
//==============================
static bool sd_ok     = false;
static bool g_baro_ok = false;
static bool g_mag_ok  = false;

static volatile bool g_net_registered = false;
static volatile bool g_pdp_up        = false;

//==============================
// Utils
//==============================
static inline void LOG(const char* s){ if (Serial) Serial.println(s); }
static inline void LOGF(const char* fmt, ...) {
  if (!Serial) return;
  char buf[192];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
}

static inline int clamp_int(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

//==============================
// SD helpers (HSPI)
//==============================
static bool sd_begin_hspi() {
  hspi.end();
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, hspi)) return false;
  if (SD.cardType() == CARD_NONE)  return false;

  File root = SD.open("/");
  if (!root) return false;
  root.close();
  return true;
}

static void sd_logln(const String& line) {
  if (!sd_ok) return;
  File f = SD.open("/logs/track.csv", FILE_APPEND);
  if (!f) {
    SD.mkdir("/logs");
    f = SD.open("/logs/track.csv", FILE_APPEND);
  }
  if (f) { f.println(line); f.close(); }
}

//==============================
// I2C seguro (SDA=21, SCL=22)
//==============================
static void i2c_init_safe() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000); // 400 kHz
  Wire.setTimeOut(20);                          // 20 ms por transacción
}

static bool i2c_probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t e = Wire.endTransmission();
  return (e == 0);
}

//==============================
// Tarea de red (no bloquea la UI)
//==============================
static void net_task(void* arg) {
  enum class NState { ST_POWER, ST_AT_PROBE, ST_SET_LTE, ST_REG_WAIT, ST_PDP_UP, ST_RUN, ST_SLEEP };
  NState st = NState::ST_POWER;

  uint32_t backoff_ms = 2000;
  const uint32_t backoff_max = 30000;

  for (;;) {
    switch (st) {
      case NState::ST_POWER: {
        modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1200, 6000);
        LOG("[NET] power on");
        st = NState::ST_AT_PROBE;
        break;
      }
      case NState::ST_AT_PROBE: {
        // Prueba de vida AT (≤3s)
        bool ok = false;
        uint32_t t0 = millis();
        for (int i=0; i<5 && millis()-t0<3000 && !ok; ++i) {
          SerialAT.print("AT\r");
          delay(200);
          String buf;
          while (SerialAT.available()) buf += (char)SerialAT.read();
          if (buf.indexOf("OK") >= 0) ok = true;
        }
        if (!ok) { LOG("[NET] no AT, backoff"); st = NState::ST_SLEEP; break; }
        st = NState::ST_SET_LTE; break;
      }
      case NState::ST_SET_LTE: {
        if (!modem_set_lte(modem)) { LOG("[NET] LTE only falló -> AUTO"); modem_set_auto(modem); }
        st = NState::ST_REG_WAIT; break;
      }
      case NState::ST_REG_WAIT: {
        bool reg = modem_wait_for_network(modem, 5000);
        g_net_registered = reg;
        if (!reg) { LOG("[NET] sin registro, backoff"); st = NState::ST_SLEEP; break; }
        st = NState::ST_PDP_UP; break;
      }
      case NState::ST_PDP_UP: {
        bool up = modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
        g_pdp_up = up;
        LOGF("[NET] PDP %s", up ? "UP" : "DOWN");
        st = up ? NState::ST_RUN : NState::ST_SLEEP;
        break;
      }
      case NState::ST_RUN: {
        vTaskDelay(pdMS_TO_TICKS(3000));
        bool reg = modem.isNetworkConnected();
        g_net_registered = reg;
        #ifdef TINY_GSM_MODEM_SIM7600
          bool pdp = modem.isGprsConnected();
        #else
          bool pdp = (modem.localIP() != IPAddress(0,0,0,0));
        #endif
        g_pdp_up = pdp;
        if (!reg || !pdp) { LOG("[NET] RUN->SLEEP"); st = NState::ST_SLEEP; }
        break;
      }
      case NState::ST_SLEEP: {
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        backoff_ms = (backoff_ms < backoff_max) ? (backoff_ms * 2) : backoff_max;
        st = NState::ST_POWER;
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

//==============================
// Acciones UI
//==============================
static void save_poi() {
  GpsFix fx = gps_last_fix();
  if (!fx.valid) { LOG("[Owl] POI cancelado: sin fix"); return; }
  String s = "POI," + fx.utc + "," +
             String(fx.lat, 6) + "," + String(fx.lon, 6) + "," + String(fx.alt, 1);
  LOG(("[Owl] " + s).c_str());
  sd_logln(s);
}

static void sos_trigger() {
  LOG("[Owl] SOS TRIGGERED!");
  sd_logln(String("SOS,") + String(millis()));
}

//==============================
// Máquina de pantallas
//==============================
static UiScreen g_screen = UiScreen::HOME;
static void next_screen() {
  uint8_t v = static_cast<uint8_t>(g_screen);
  v = (v + 1) % 5; // HOME, GPS_DETAIL, IRIDIUM_DETAIL, SYS_CONFIG, MESSAGES
  g_screen = static_cast<UiScreen>(v);
}

//==============================
// SETUP
//==============================
void setup() {
  Serial.begin(115200);
  delay(50);
  LOG("\n[Owl] Boot");

  // OLED primero (feedback inmediato)
  oled_init();
  oled_splash("Owl Tracker");
  delay(600);

  // Botonera (UI siempre viva)
  buttons_begin();

  // UART módem (no bloquea, net_task hará el resto)
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);

  // GNSS
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
  gps_set_stale_timeout(10000);

  // I2C seguro + detección de baro y mag del GEP-M1025-MI
  i2c_init_safe();
  // MS5611: 0x76/0x77
  if (i2c_probe(0x76) || i2c_probe(0x77)) {
    g_baro_ok = baro_begin();
  } else {
    LOG("[I2C] MS5611 no detectado (0x76/0x77)");
  }
  // IST8310: 0x0E
  if (i2c_probe(0x0E)) {
    g_mag_ok = mag_begin();
  } else {
    LOG("[I2C] IST8310 no detectado (0x0E)");
  }

  // SD (HSPI)
  sd_ok = sd_begin_hspi();
  LOGF("[Owl] SD %s", sd_ok ? "OK" : "NO");

  // Tarea de red
  xTaskCreatePinnedToCore(net_task, "net_task", 4096, nullptr, 1, nullptr, 0);
}

//==============================
// LOOP
//==============================
void loop() {
  // Sensores no bloqueantes
  gps_poll(SerialGPS);
  if (g_baro_ok) baro_poll();
  if (g_mag_ok)  mag_poll();

  // Botones (10 ms)
  static uint32_t t_btn = 0;
  if (millis() - t_btn >= 10) {
    t_btn = millis();
    buttons_poll();

    // BTN1: corto -> HOME ; largo -> cambiar menú
    if (auto e = btn1_get(); e.shortPress) {
      g_screen = UiScreen::HOME;
      LOG("[Owl] HOME");
    } else if (e.longPress) {
      next_screen();
      LOG("[Owl] Cambiar pantalla");
    }

    // BTN2: corto -> MESSAGES ; largo -> POI
    if (auto e = btn2_get(); e.shortPress) {
      g_screen = UiScreen::MESSAGES;
      LOG("[Owl] MESSAGES");
    } else if (e.longPress) {
      save_poi();
    }

    // BTN3: largo -> SOS
    if (auto e = btn3_get(); e.longPress) {
      sos_trigger();
    }
  }

  // UI (1 Hz)
  static uint32_t t_ui = 0;
  if (millis() - t_ui >= 1000) {
    t_ui = millis();

    GpsFix   fx = gps_last_fix();
    BaroData bd = baro_last();

    OwlUiData ui;
    ui.csq          = modem.getSignalQuality();
    ui.iridiumLvl   = -1;                     // sin iridium todavía
    ui.lat          = fx.valid ? fx.lat : NAN;
    ui.lon          = fx.valid ? fx.lon : NAN;
    ui.alt          = fx.valid ? fx.alt : NAN;
    ui.sats         = fx.sats;
    ui.pdop         = fx.pdop;
    ui.speed_mps    = fx.speed_mps;
    ui.course_deg   = fx.course_deg;
    ui.pressure_hpa = bd.pressure_hpa;
    ui.msgRx        = 0;
    ui.utc          = fx.utc.length() ? (fx.utc + "Z") : String("");

    switch (g_screen) {
      case UiScreen::HOME:
        oled_draw_dashboard(ui);
        break;
      case UiScreen::GPS_DETAIL:
        oled_draw_gps_detail(ui);
        break;
      case UiScreen::IRIDIUM_DETAIL:
        // por ahora mostramos stub de iridium en 0 barras
        oled_draw_iridium_detail(false, -1, 0, "");
        break;
      case UiScreen::SYS_CONFIG: {
        const String ipStr = modem.localIP().toString();
        const bool   i2cOk = (g_baro_ok || g_mag_ok);
        const char*  fwStr = "fw 1.0.0";
        oled_draw_sys_config(ui, g_net_registered, g_pdp_up, ipStr, sd_ok, i2cOk, fwStr);
        break;
      }
      case UiScreen::MESSAGES:
        oled_draw_messages(0, "");
        break;
    }

    LOG("[Owl] UI refrescada");
  }

  // Log SD (30s)
  static uint32_t t_log = 0;
  if (sd_ok && millis() - t_log > 30000) {
    t_log = millis();
    GpsFix fx = gps_last_fix();
    String s = String(fx.utc) + "," +
               (fx.valid ? String(fx.lat,6) : "NaN") + "," +
               (fx.valid ? String(fx.lon,6) : "NaN") + "," +
               (fx.valid ? String(fx.alt,1) : "NaN");
    sd_logln(s);
  }
}
