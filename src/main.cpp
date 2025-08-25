#include "board_pins.h"
#include <Arduino.h>
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

// SD (HSPI)
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ------------------------------------------------------
// UARTs
HardwareSerial SerialAT(1);   // Módem A7670G
HardwareSerial SerialGPS(2);  // GNSS externo

// Objetos principales
TinyGsm modem(SerialAT);
OwlHttpClient http;
SPIClass hspi(HSPI);

// Estado SD
static bool sd_ok = false;

// Indicadores de red actualizados por la tarea background
static volatile bool g_net_registered = false;
static volatile bool g_pdp_up        = false;

// ------------------------------------------------------
// Utilidades SD
static bool sd_begin_hspi() {
  hspi.end();
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, hspi)) return false;
  if (SD.cardType() == CARD_NONE)  return false;

  // Si no hay FAT válida, mejor no habilitar SD
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

// AT crudo para comprobar vida del módem
static bool rawAtOK(Stream& s, uint8_t tries=15, uint16_t gap_ms=200) {
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

// ------------------------------------------------------
// Acciones UI
static void sos_trigger() {
  Serial.println("[Owl] SOS TRIGGERED!");
  // TODO: enviar alerta (HTTP/Iridium)
  sd_logln(String("SOS,") + String(millis()));
}

static void save_poi() {
  GpsFix fx = gps_last_fix();
  if (!fx.valid) {
    Serial.println("[Owl] POI cancelado: sin fix");
    return;
  }
  String s = "POI," + fx.utc + "," +
             String(fx.lat, 6) + "," + String(fx.lon, 6) + "," + String(fx.alt, 1);
  Serial.println("[Owl] " + s);
  sd_logln(s);
}

// ------------------------------------------------------
// Máquina de estados de pantalla
static UiScreen g_screen = UiScreen::HOME;
static void next_screen() {
  uint8_t v = static_cast<uint8_t>(g_screen);
  v = (v + 1) % 5; // HOME, GPS_DETAIL, IRIDIUM_DETAIL, SYS_CONFIG, MESSAGES
  g_screen = static_cast<UiScreen>(v);
}

// ------------------------------------------------------
// Tarea de red (no bloquea la UI)
static void net_task(void* arg) {
  for (;;) {
    // Registro a red
    if (!modem.isNetworkConnected()) {
      g_net_registered = false;
      (void)modem_wait_for_network(modem, 5000);   // bloqueante, pero en otra tarea
      g_net_registered = modem.isNetworkConnected();
    }

    // PDP
    if (g_net_registered) {
      #ifdef TINY_GSM_MODEM_SIM7600
      bool pdp_now = modem.isGprsConnected();
      #else
      bool pdp_now = (modem.localIP() != IPAddress(0,0,0,0));
      #endif

      if (!pdp_now) {
        g_pdp_up = false;
        (void)modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
        #ifdef TINY_GSM_MODEM_SIM7600
        g_pdp_up = modem.isGprsConnected();
        #else
        g_pdp_up = (modem.localIP() != IPAddress(0,0,0,0));
        #endif
      } else {
        g_pdp_up = true;
      }
    } else {
      g_pdp_up = false;
    }

    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}

// ======================================================
//                        SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("\n[Owl] Boot");

  // --- Power & UART módem ---
  modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true /*activeHigh*/, 1200, 6000);
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
  delay(50);

  if (!rawAtOK(SerialAT, 20, 200)) {
    Serial.println("[Owl] AT no responde, power-cycle");
    digitalWrite(PIN_MODEM_EN, LOW); delay(600);
    modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1500, 7000);
    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
    delay(100);
    (void)rawAtOK(SerialAT, 20, 250);
  }

  // --- TinyGSM bring-up ---
  if (!modem_init(modem)) Serial.println("[Owl] TinyGSM init falló");
  if (!modem_set_lte(modem)) { Serial.println("[Owl] LTE only falló -> AUTO"); modem_set_auto(modem); }
  (void)modem_wait_for_network(modem, 30000);
  (void)modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
  http.begin(modem, netcfg::HOST, netcfg::PORT, nullptr); // sin TLS por ahora

  // --- GNSS UART2 ---
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
  gps_set_stale_timeout(10000);

  // --- OLED ---
  oled_init();

  // --- I2C: Baro + Mag ---
  baro_begin();
  mag_begin();

  // --- SD en HSPI ---
  sd_ok = sd_begin_hspi();
  Serial.printf("[Owl] SD %s\n", sd_ok ? "OK" : "NO");

  // --- Botonera ---
  buttons_begin();

  // --- Tarea de red en background ---
  xTaskCreatePinnedToCore(net_task, "net_task", 4096, nullptr, 1, nullptr, 0);
}

// ======================================================
//                         LOOP
// ======================================================
void loop() {
  // --------- Sensores no bloqueantes ----------
  gps_poll(SerialGPS);
  baro_poll();
  mag_poll();

  // --------- Botones (cada 10 ms) --------------
  static uint32_t t_btn = 0;
  if (millis() - t_btn >= 10) {
    t_btn = millis();
    buttons_poll();

    // BTN1: corto -> HOME ; largo -> cambiar menú
    if (auto e = btn1_get(); e.shortPress) {
      g_screen = UiScreen::HOME;
      Serial.println("[Owl] Volver a HOME");
    } else if (e.longPress) {
      next_screen();
      Serial.println("[Owl] Cambiar pantalla");
    }

    // BTN2: corto -> MESSAGES ; largo -> POI
    if (auto e = btn2_get(); e.shortPress) {
      g_screen = UiScreen::MESSAGES;
      Serial.println("[Owl] Ir a MESSAGES");
    } else if (e.longPress) {
      save_poi();
    }

    // BTN3: largo -> SOS
    if (auto e = btn3_get(); e.longPress) {
      sos_trigger();
    }
  }

  // --------- UI (1 Hz) -------------------------
  static uint32_t t_ui = 0;
  if (millis() - t_ui >= 1000) {
    t_ui = millis();

    // Recopilar datos
    GpsFix   fx = gps_last_fix();
    BaroData bd = baro_last();

    OwlUiData ui;
    ui.csq          = modem.getSignalQuality();
    ui.iridiumLvl   = -1;                     // IR no implementado aún
    ui.lat          = fx.valid ? fx.lat : NAN;
    ui.lon          = fx.valid ? fx.lon : NAN;
    ui.alt          = fx.valid ? fx.alt : NAN;
    ui.sats         = fx.sats;
    ui.pdop         = fx.pdop;
    ui.speed_mps    = fx.speed_mps;
    ui.course_deg   = fx.course_deg;
    ui.pressure_hpa = bd.pressure_hpa;
    ui.msgRx        = 0;                      // contadores reales luego
    ui.utc          = fx.utc.length() ? (fx.utc + "Z") : String("");  // muestra "--" si vacío

    // Render por pantalla (por ahora todas usan el dashboard)
    switch (g_screen) {
      case UiScreen::HOME:
      case UiScreen::GPS_DETAIL:
      case UiScreen::IRIDIUM_DETAIL:
      case UiScreen::SYS_CONFIG:
      case UiScreen::MESSAGES:
        oled_draw_dashboard(ui);
        break;
    }

    Serial.println("[Owl] UI refrescada");
  }

  // --------- Log SD (cada 30 s) ----------------
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
