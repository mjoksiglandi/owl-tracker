#include "board_pins.h"
#include <Arduino.h>
#include <stdarg.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <Client.h>
#include <TinyGsmClient.h>

// Conmutador de portador si lo usas (opcional)
#include "comms_mode.h"

// Núcleo
#include "modem_config.h"
#include "http_client.h"
#include "net_config.h"

// OLED/UI/GPS/IR/Buttons
#include "oled_display.h"
#include "ui_state.h"
#include "gps.h"
#include "buttons.h"
#include "iridium.h"

// BLE + Reporte
#include "ble.h"
#include "report.h"

// Cifrado (para HTTP GCM)
#include "crypto.h"    // owl_encrypt_aes256gcm_base64(...)
#include "secrets.h"   // OWL_AES256_KEY[32], OWL_GCM_IV_LEN

// SdFat o SD simple (usa la que tengas; aquí SD simple con HSPI)
#include <FS.h>
#include <SD.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// UARTs
HardwareSerial SerialAT(1);
HardwareSerial SerialGPS(2);

// TinyGSM + HTTP (sin TLS por ahora)
TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
OwlHttpClient http(modem, gsmClient, netcfg::HOST, netcfg::PORT);

// ------- Estado global base -------
static bool sd_ok = false;
static bool g_mag_ok = false;
static bool ble_ok = false;

static volatile bool g_net_registered = false;
static volatile bool g_pdp_up = false;

static String g_gsm_imei = "";
static int g_next_report_type = 1; // 1=Normal, 2=Emergencia, 3=Libre, 4=POI

// UI
static UiScreen g_screen = UiScreen::HOME;
static int g_csq_cache = 99;
static char g_rat_label[6] = "--"; // etiqueta simple para la barra superior
static String g_ip_cache = "";
static bool g_pref_iridium = false; // fallback dinámico

// ======== LOG helpers ========
static inline void LOG(const char *s){
  if (Serial) Serial.println(s);
}
static inline void LOGF(const char *fmt, ...){
  if (!Serial) return;
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);   // <-- corregido: incluye fmt
  va_end(ap);
  Serial.println(buf);
}

// ===================== SD (HSPI simple) =====================
SPIClass hspi(HSPI);
static bool sd_begin_hspi(){
  hspi.end();
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, hspi)) return false;
  if (SD.cardType() == CARD_NONE) return false;
  return true;
}
static void sd_logln(const String& line){
  if (!sd_ok) return;
  File f = SD.open("/logs/track.csv", FILE_APPEND);
  if (!f) {
    SD.mkdir("/logs");
    f = SD.open("/logs/track.csv", FILE_APPEND);
  }
  if (f){ f.println(line); f.close(); }
}

// ===================== I2C ============================
static void i2c_init_safe(){
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(20);
}
static bool i2c_probe(uint8_t addr){
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

// ===== Iridium enqueue (stub por ahora) =====
static bool iridium_send_json_stub(const String& s) {
  Serial.printf("[IR] (plain queued) %s\n", s.c_str());
  return false;
}

// ===================== Helpers de reporte ====================
static String make_json_from_report(const OwlReport& rpt) {
  char buf[384];
  auto nz  = [](double v){ return std::isnan(v) ? 0.0 : v; };
  auto nzf = [](float  v){ return std::isnan(v) ? 0.0f : v; };
  snprintf(buf, sizeof(buf),
    "{"
      "\"tipoReporte\":%d,"
      "\"IMEI\":\"%s\","
      "\"latitud\":%.6f,"
      "\"longitud\":%.6f,"
      "\"altitud\":%.1f,"
      "\"rumbo\":%.1f,"
      "\"velocidad\":%.2f,"
      "\"fechaHora\":\"%s\""
    "}",
    rpt.tipoReporte,
    rpt.IMEI.c_str(),
    nz(rpt.latitud), nz(rpt.longitud), nz(rpt.altitud),
    nzf(rpt.rumbo), nzf(rpt.velocidad),
    rpt.fechaHora.c_str()
  );
  return String(buf);
}

static String make_gcm_b64_from_json(const String& jsonPlain) {
  uint8_t iv[OWL_GCM_IV_LEN];
  for (size_t i = 0; i < OWL_GCM_IV_LEN; ++i) iv[i] = (uint8_t)(esp_random() & 0xFF);
  String b64 = owl_encrypt_aes256gcm_base64(
    OWL_AES256_KEY,
    (const uint8_t*)jsonPlain.c_str(), jsonPlain.length(),
    iv, sizeof(iv)
  );
  return b64; // sin prefijo; ya tiene '|' separando AD/IV/CIPH
}

// ===================== Acciones =======================
static void save_poi(){
  GpsFix fx = gps_last_fix();
  if (!fx.valid) {
    LOG("[Owl] POI cancelado: sin fix");
    return;
  }
  String s = "POI," + fx.utc + "," + String(fx.lat,6) + "," + String(fx.lon,6) + "," + String(fx.alt,1);
  LOG(("[Owl] " + s).c_str());
  sd_logln(s);
  g_next_report_type = 4;
}
static void sos_trigger(){
  LOG("[Owl] SOS TRIGGERED!");
  sd_logln(String("SOS,") + String(millis()));
  g_next_report_type = 2;
}

// ===================== net_task (FSM estable) =======================
static void net_task(void *){
  enum class NState { ST_POWER, ST_AT, ST_REG, ST_PDP, ST_RUN, ST_SLEEP };
  NState st = NState::ST_POWER;
  uint32_t backoff = 2000, backoff_max = 30000;

  for(;;){
    switch(st){
      case NState::ST_POWER:{
        modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1200, 6000);
        LOG("[NET] power on");
        st = NState::ST_AT;
      } break;

      case NState::ST_AT:{
        bool ok = modem.testAT();
        if (!ok){ LOG("[NET] no AT → SLEEP"); st = NState::ST_SLEEP; break; }
        modem_set_auto(modem); // AT+CNMP=2 (AUTO)
        st = NState::ST_REG;
      } break;

      case NState::ST_REG:{
        bool reg = modem.waitForNetwork(10000);
        g_net_registered = reg;
        if (!reg){ LOG("[NET] sin registro → SLEEP"); st = NState::ST_SLEEP; break; }
        LOG("[NET] registrado");
        st = NState::ST_PDP;
      } break;

      case NState::ST_PDP:{
        bool up = modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
        g_pdp_up = up;
        LOGF("[NET] PDP %s", up ? "UP" : "DOWN");
        st = NState::ST_RUN;
      } break;

      case NState::ST_RUN:{
        vTaskDelay(pdMS_TO_TICKS(3000));
        bool reg = modem.isNetworkConnected();
        g_net_registered = reg;
        bool pdp = modem.isGprsConnected();  // ✅ SIM76XX/A7670 confiable
        g_pdp_up = pdp;

        // caches para UI
        g_csq_cache = modem.getSignalQuality();
        if (pdp) {
          g_ip_cache = modem.localIP().toString();
          strncpy(g_rat_label, "4G", sizeof(g_rat_label)); // marcador simple
          g_pref_iridium = false; // prioriza GSM si PDP ok
        } else {
          g_ip_cache = "";
          strncpy(g_rat_label, "--", sizeof(g_rat_label));
          // NO resetea: deja fallback a Iridium desde loop
        }

        if (!reg){ LOG("[NET] sin registro → SLEEP"); st = NState::ST_SLEEP; }
      } break;

      case NState::ST_SLEEP:{
        vTaskDelay(pdMS_TO_TICKS(backoff));
        backoff = (backoff < backoff_max) ? (backoff * 2) : backoff_max;
        st = NState::ST_POWER;
      } break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ===================== SETUP ==========================
void setup(){
  Serial.begin(115200);
  delay(50);
  LOG("\n[Owl] Boot");

  WiFi.mode(WIFI_OFF);

  oled_init();
  oled_splash("Owl Tracker");
  delay(400);

  buttons_begin();

  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);

  SerialGPS.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
  gps_set_stale_timeout(10000);

  i2c_init_safe();

  if (i2c_probe(0x0E)) {
    g_mag_ok = true; // mag_begin() si lo tienes
    LOGF("[I2C] MAG %s", g_mag_ok ? "OK" : "NO");
  } else {
    LOG("[I2C] IST8310 no detectado");
  }

  if (i2c_probe(IRIDIUM_I2C_ADDR)) {
    bool ir_ok = iridium_begin();
    LOGF("[IR] presente=%s", ir_ok ? "SI" : "NO");
  } else {
    LOG("[IR] sin ACK 0x63");
  }

  sd_ok = sd_begin_hspi();
  LOGF("[Owl] SD %s", sd_ok ? "OK" : "NO");

  ble_ok = ble_begin("OwlTracker");
  LOGF("[BLE] %s", ble_ok ? "advertising" : "init fail");

  xTaskCreatePinnedToCore(net_task, "net_task", 8192, nullptr, 1, nullptr, 0);
}

// ===================== LOOP ==========================
void loop(){
  gps_poll(SerialGPS);
  iridium_poll();
  ble_poll();

  // --------- Botones ---------
  static uint32_t t_btn = 0;
  if (millis() - t_btn >= 10){
    t_btn = millis();
    buttons_poll();

    if (auto e = btn1_get(); e.shortPress){
      g_screen = UiScreen::HOME;
    } else if (e.longPress){
      g_screen = UiScreen::GPS_DETAIL;
    }

    if (auto e2 = btn2_get(); e2.shortPress){
      g_screen = UiScreen::MESSAGES;
    } else if (e2.longPress){
      save_poi();
    }

    if (auto e3 = btn3_get(); e3.longPress){
      sos_trigger();
    }

    // BTN4: puedes rotar modo o abrir testing si tienes esa pantalla
    if (auto e4 = btn4_get(); e4.shortPress){
      // rotación de modo si tienes conmutador
      // auto m = comms_mode_get();
      // m = comms_mode_next(m);
      // comms_mode_set(m);
    } else if (e4.longPress){
      g_screen = UiScreen::TESTING;
    }
  }

  // ---------- Uplink cada 10 s ----------
  static uint32_t t_uplink = 0;
  const uint32_t UPLINK_PERIOD_MS = 10000; // o netcfg::UPLINK_PERIOD_MS si lo tienes
  if (millis() - t_uplink >= UPLINK_PERIOD_MS) {
    t_uplink = millis();

    GpsFix fx = gps_last_fix();
    IridiumInfo ir = iridium_status();

    // Cachea IMEI GSM cuando esté registrado
    if (g_gsm_imei.length() == 0 && g_net_registered){
      String imeiTry = modem.getIMEI();
      if (imeiTry.length()) g_gsm_imei = imeiTry;
    }

    OwlReport rpt;
    rpt.tipoReporte = g_next_report_type;
    if (g_gsm_imei.length())      rpt.IMEI = g_gsm_imei;
    else if (ir.imei.length())    rpt.IMEI = ir.imei;
    else                          rpt.IMEI = "";

    rpt.latitud   = fx.valid ? fx.lat : NAN;
    rpt.longitud  = fx.valid ? fx.lon : NAN;
    rpt.altitud   = fx.valid ? fx.alt : NAN;
    rpt.rumbo     = fx.course_deg;
    rpt.velocidad = fx.speed_mps;
    rpt.fechaHora = fx.utc.length() ? (fx.utc + "Z") : String("");

    // BLE (tu ble_update acepta OwlReport)
    ble_update(rpt);

    // Serial debug: plaintext y GCM
    String jsonPlain = make_json_from_report(rpt);
    String gcmB64    = make_gcm_b64_from_json(jsonPlain);
    Serial.println("[BLE] JSON plaintext:");
    Serial.println(jsonPlain);
    Serial.println("[BLE] Base64 ciphertext (GCM):");
    Serial.println(gcmB64);

    // HTTP (si PDP) o Iridium
    if (g_net_registered && g_pdp_up) {
      int code_plain = http.postJson(netcfg::PATH_PLAIN, jsonPlain.c_str(), netcfg::AUTH_BEARER);
      Serial.printf("[HTTP] POST %s -> %d\n", netcfg::PATH_PLAIN, code_plain);

      String body = String("{\"gcm_b64\":\"") + gcmB64 + "\"}";
      int code_gcm = http.postJson(netcfg::PATH_GCM, body.c_str(), netcfg::AUTH_BEARER);
      Serial.printf("[HTTP] POST %s -> %d\n", netcfg::PATH_GCM, code_gcm);
    } else {
      iridium_send_json_stub(jsonPlain);
    }

    g_next_report_type = 1;
  }

  // ---------- UI cada 1 s ----------
  static uint32_t t_ui = 0;
  if (millis() - t_ui >= 1000){
    t_ui = millis();

    GpsFix fx = gps_last_fix();
    IridiumInfo ir = iridium_status();

    OwlUiData ui;
    ui.csq = g_csq_cache;
    ui.iridiumLvl = (ir.sig >= 0) ? ir.sig : -1;
    ui.lat = fx.valid ? fx.lat : NAN;
    ui.lon = fx.valid ? fx.lon : NAN;
    ui.alt = fx.valid ? fx.alt : NAN;
    ui.sats = fx.sats;
    ui.pdop = fx.pdop;
    ui.speed_mps = fx.speed_mps;
    ui.course_deg = fx.course_deg;
    ui.pressure_hpa = NAN;
    ui.msgRx = ir.waiting ? 1 : 0;
    ui.utc = fx.utc.length() ? (fx.utc + "Z") : String("");

    static UiScreen lastScreen = UiScreen::HOME;
    static OwlUiData last = {};
    auto neq = [](double x, double y, double eps) { return !(fabs(x-y) <= eps); };
    auto changed = [&](const OwlUiData &a, const OwlUiData &b){
      return a.csq != b.csq || a.iridiumLvl != b.iridiumLvl || a.sats != b.sats ||
             neq(a.pdop, b.pdop, 0.1) || neq(a.lat, b.lat, 1e-7) ||
             neq(a.lon, b.lon, 1e-7) || neq(a.alt, b.alt, 0.3) ||
             a.msgRx != b.msgRx || a.utc != b.utc ||
             neq(a.speed_mps, b.speed_mps, 0.1) || neq(a.course_deg, b.course_deg, 0.5);
    };

    bool needRedraw = (g_screen != lastScreen) || changed(ui, last);
    if (needRedraw){
      switch (g_screen){
        case UiScreen::HOME:
          oled_draw_dashboard(ui, g_rat_label); // <- firma actual
          break;
        case UiScreen::GPS_DETAIL:
          oled_draw_gps_detail(ui);
          break;
        case UiScreen::IRIDIUM_DETAIL:
          oled_draw_iridium_detail(ir.present, ir.sig, ir.waiting ? 1 : 0, ir.imei);
          break;
        case UiScreen::SYS_CONFIG: {
          const String ipStr = g_ip_cache;
          const bool i2cOk = (g_mag_ok || ir.present);
          String fwStr = String("fw 1.0.0 ") + (ble_ok ? "BLE:OK" : "BLE:NO");
          oled_draw_sys_config(ui, g_net_registered, g_pdp_up, ipStr, sd_ok, i2cOk, fwStr.c_str());
        } break;
        case UiScreen::MESSAGES:
          oled_draw_messages(ir.waiting ? 1 : 0, "");
          break;
        case UiScreen::GSM_DETAIL: {
          auto csq_to_dbm = [](int csq)->int {
            if (csq <= 0)  return -113;
            if (csq == 1)  return -111;
            if (csq >= 31) return -51;
            return -113 + 2 * csq;
          };
          int rssiDbm = csq_to_dbm(ui.csq);
          oled_draw_gsm_detail(ui, g_gsm_imei, g_net_registered, g_pdp_up, rssiDbm);
        } break;
        case UiScreen::BLE_DETAIL: {
          extern bool ble_is_connected();
          bool bleConn = ble_is_connected();
          oled_draw_ble_detail(bleConn, String("OwlTracker"));
        } break;
        case UiScreen::TESTING: {
          // Si tu firma requiere LinkPref y valores, castea 0/1
          oled_draw_testing((LinkPref)(g_pref_iridium ? 1 : 0), "OK", false);
        } break;
      }
      last = ui;
      lastScreen = g_screen;
    }
  }

  // -------- Log SD (30 s) --------
  static uint32_t t_log = 0;
  if (sd_ok && millis() - t_log > 30000){
    t_log = millis();
    GpsFix fx = gps_last_fix();
    String s = String(fx.utc) + "," +
               (fx.valid ? String(fx.lat, 6) : "NaN") + "," +
               (fx.valid ? String(fx.lon, 6) : "NaN") + "," +
               (fx.valid ? String(fx.alt, 1) : "NaN");
    sd_logln(s);
  }
}
