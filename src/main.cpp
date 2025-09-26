#include "board_pins.h"
#include <Arduino.h>
#include <stdarg.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <TinyGsmClient.h>
#include "comms_mode.h"
#include "settings.h"

// Núcleo
#include "modem_config.h"
#include "http_client.h"
#include "net_config.h"

// Sensores / UI
#include "oled_display.h"
#include "gps.h"
#include "ui_state.h"
#include "buttons.h"
#include "iridium.h"
#include "mag.h"

// BLE + Reporte
#include "ble.h"
#include "report.h"

// Cifrado (para HTTP)
#include "crypto.h"    // owl_encrypt_aes256gcm_base64(...)
#include "secrets.h"   // OWL_AES256_KEY[32], OWL_GCM_IV_LEN

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// UARTs
HardwareSerial SerialAT(1);
HardwareSerial SerialGPS(2);

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
OwlHttpClient http(modem, gsmClient, netcfg::HOST, netcfg::PORT);

SPIClass hspi(HSPI);

// ======== Estado global ========
static bool sd_ok = false;
static bool g_mag_ok = false;
static bool ble_ok = false;

static volatile bool g_net_registered = false;
static volatile bool g_pdp_up = false;

// IMEI GSM (cache) y tipo de reporte “one-shot”
static String g_gsm_imei = "";
static int g_next_report_type = 1; // 1=Normal, 2=Emergencia, 3=Libre, 4=POI

// TESTING screen
static char g_last_test[64] = {0};
static volatile bool g_test_busy = false;
static uint8_t g_test_page = 0;     // 0..1

// ======== Caches de red para UI (solo net_task) ========
static volatile int     g_csq_cache = 99;
static char             g_rat_label[6] = "--";
static String           g_ip_cache = "";

// ======== Preferencia dinámica de portador ========
static bool g_pref_iridium = false;           // true si no hay PDP sostenido
static uint32_t g_no_pdp_since_ms = 0;        // cuándo empezó a faltar PDP
static uint32_t g_pdp_ok_since_ms = 0;        // cuándo volvió PDP

// ======== Mutex TinyGSM ========
static SemaphoreHandle_t g_modem_mtx = nullptr;
static inline bool modem_lock(uint32_t ms=15000){
  if (!g_modem_mtx) return false;
  return xSemaphoreTake(g_modem_mtx, pdMS_TO_TICKS(ms)) == pdTRUE;
}
static inline void modem_unlock(){
  if (g_modem_mtx) xSemaphoreGive(g_modem_mtx);
}

// ===================== LOG helpers ===========================
static inline void LOG(const char *s){
  if (Serial) Serial.println(s);
}
static inline void LOGF(const char *fmt, ...){
  if (!Serial) return;
  char buf[192];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
}

// ===================== SD helpers (HSPI) =====================
static bool sd_begin_hspi(){
  hspi.end();
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, hspi))   return false;
  if (SD.cardType() == CARD_NONE)   return false;
  File root = SD.open("/");
  if (!root)                        return false;
  root.close();
  return true;
}
static void sd_logln(const String &line){
  if (!sd_ok) return;
  File f = SD.open("/logs/track.csv", FILE_APPEND);
  if (!f){
    SD.mkdir("/logs");
    f = SD.open("/logs/track.csv", FILE_APPEND);
  }
  if (f){
    f.println(line);
    f.close();
  }
}

// ===================== I2C seguro ============================
static void i2c_init_safe(){
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(20);
}
static bool i2c_probe(uint8_t addr){
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

// ===== Iridium enqueue (stub opcional) =====
// Si ya tienes una API real para enviar por Iridium, reemplaza esta función por tu llamada.
static bool iridium_send_json_stub(const String& s) {
  Serial.printf("[IR] (send-json) %s\n", s.c_str());
  return false; // Devuelve true si tu envío real lo encola/acepta
}

// ===================== Red (tarea) ===========================
static void net_task(void *arg){
  enum class NState { ST_POWER, ST_AT_PROBE, ST_REG_WAIT, ST_PDP_UP, ST_RUN, ST_SLEEP };
  NState st = NState::ST_POWER;

  uint32_t backoff_ms = 2000;
  const uint32_t backoff_max = 30000;

  for (;;){
    switch (st){
      case NState::ST_POWER: {
        modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1200, 6000);
        LOG("[NET] power on");
        st = NState::ST_AT_PROBE;
      } break;

      case NState::ST_AT_PROBE: {
        bool ok = false;
        uint32_t t0 = millis();
        for (int i = 0; i < 5 && millis() - t0 < 3000 && !ok; ++i){
          SerialAT.print("AT\r");
          delay(200);
          String buf;
          while (SerialAT.available()) buf += (char)SerialAT.read();
          if (buf.indexOf("OK") >= 0) ok = true;
        }
        if (!ok){
          LOG("[NET] no AT, backoff");
          st = NState::ST_SLEEP;
          break;
        }
        // RAT auto + tuning (PSM/eDRX OFF, COPS=0)
        modem_set_auto(modem);       // ya no forzamos LTE-only
        modem_radio_tune(modem);
        st = NState::ST_REG_WAIT;
      } break;

      case NState::ST_REG_WAIT: {
        // Ventana realista (hasta ~45 s)
        bool reg = modem_wait_for_network(modem, 45000);
        g_net_registered = reg;
        if (!reg){
          LOG("[NET] sin registro, backoff");
          st = NState::ST_SLEEP;
          break;
        }
        st = NState::ST_PDP_UP;
      } break;

      case NState::ST_PDP_UP: {
        if (!modem_lock(20000)) { st = NState::ST_SLEEP; break; }
        bool up = modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
        modem_unlock();

        g_pdp_up = up;
        LOGF("[NET] PDP %s", up ? "UP" : "DOWN");
        if (up) {
          modem_print_status(modem);
          modem_dump_regs(modem); // diagnóstico adicional
          // Reset timers de preferencia
          g_no_pdp_since_ms = 0;
          g_pdp_ok_since_ms = millis();
          g_pref_iridium = false;
          st = NState::ST_RUN;
        } else {
          // No hay PDP: entra a RUN igualmente (no reiniciamos), preferencia a Iridium tras 30s
          g_no_pdp_since_ms = millis();
          g_pdp_ok_since_ms = 0;
          g_pref_iridium = true;
          st = NState::ST_RUN;
        }
      } break;

      case NState::ST_RUN: {
        vTaskDelay(pdMS_TO_TICKS(300));

        bool reg = modem.isNetworkConnected();
        g_net_registered = reg;
        #ifdef TINY_GSM_MODEM_SIM7600
          bool pdp = modem.isGprsConnected();
        #else
          bool pdp = (modem.localIP() != IPAddress(0,0,0,0));
        #endif
        g_pdp_up = pdp;

        // Gestionar timers de preferencia sin reiniciar el módem
        uint32_t now = millis();
        if (pdp) {
          if (g_pdp_ok_since_ms == 0) g_pdp_ok_since_ms = now;
          // si PDP estable ≥10 s, quitamos preferencia Iridium
          if (g_pdp_ok_since_ms && (now - g_pdp_ok_since_ms) >= 10000) {
            if (g_pref_iridium) {
              g_pref_iridium = false;
              Serial.println("[NET] PDP estable: prioridad normal (GSM/LTE primero)");
            }
          }
          g_no_pdp_since_ms = 0;
        } else {
          if (g_no_pdp_since_ms == 0) g_no_pdp_since_ms = now;
          // si falta PDP ≥30 s, preferir Iridium
          if (!g_pref_iridium && (now - g_no_pdp_since_ms) >= 30000) {
            g_pref_iridium = true;
            Serial.println("[NET] Sin PDP sostenido: prioridad IRIDIUM");
          }
          g_pdp_ok_since_ms = 0;
        }

        // Refrescos UI protegidos
        if (modem_lock(500)){
          int csq = modem.getSignalQuality();
          g_csq_cache = (csq < 0 ? 99 : csq);

          String r = modem_rat_label(modem);
          r.toCharArray((char*)g_rat_label, sizeof(g_rat_label));

          if (pdp){
            IPAddress ip = modem.localIP();
            g_ip_cache = ip.toString();
          } else {
            g_ip_cache = "";
          }

          if (g_gsm_imei.length() == 0 && reg){
            String imeiTry = modem.getIMEI();
            if (imeiTry.length() > 0) g_gsm_imei = imeiTry;
          }
          modem_unlock();
        }

        // Importante: NO hacemos hard reset ni recover aquí; el equipo permanece en RUN.
      } break;

      case NState::ST_SLEEP: {
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        backoff_ms = (backoff_ms < backoff_max) ? (backoff_ms * 2) : backoff_max;
        st = NState::ST_POWER;
      } break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ===================== Acciones UI ===========================
static void save_poi(){
  GpsFix fx = gps_last_fix();
  if (!fx.valid){
    LOG("[Owl] POI cancelado: sin fix");
    return;
  }
  String s = "POI," + fx.utc + "," + String(fx.lat, 6) + "," + String(fx.lon, 6) + "," + String(fx.alt, 1);
  LOG(("[Owl] " + s).c_str());
  sd_logln(s);
  g_next_report_type = 4; // POI
}
static void sos_trigger(){
  LOG("[Owl] SOS TRIGGERED!");
  sd_logln(String("SOS,") + String(millis()));
  g_next_report_type = 2; // Emergencia
}

// ===================== Pantallas =============================
static UiScreen g_screen = UiScreen::HOME;

// Carrusel SIN TESTING (para no activarlo por error)
static UiScreen next_screen_normal(UiScreen cur){
  static const UiScreen order[] = {
    UiScreen::HOME,
    UiScreen::GPS_DETAIL,
    UiScreen::IRIDIUM_DETAIL,
    UiScreen::GSM_DETAIL,
    UiScreen::BLE_DETAIL,
    UiScreen::SYS_CONFIG,
    UiScreen::MESSAGES
  };
  size_t n = sizeof(order)/sizeof(order[0]);
  size_t idx = 0;
  for (size_t i=0;i<n;i++){ if (order[i]==cur){ idx = i; break; } }
  return order[(idx+1)%n];
}

// Adaptador: CommsMode -> nombre corto
static const char* mode_name_from_comms(){
  const char* n = comms_mode_name(comms_mode_get());
  return n ? n : "AUTO";
}

// ======= Helpers JSON / GCM para uplink =======
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
  return b64;
}

// ===================== SETUP =================================
void setup(){
  Serial.begin(115200);
  delay(50);
  LOG("\n[Owl] Boot");

  g_modem_mtx = xSemaphoreCreateMutex();

  WiFi.mode(WIFI_OFF);

  oled_init();
  oled_splash("Owl Tracker");
  delay(400);

  // HOME “en vacío”
  {
    OwlUiData bootUi;
    bootUi.csq = 99;
    bootUi.iridiumLvl = -1;
    bootUi.sats = -1;
    bootUi.pdop = -1;
    bootUi.lat = bootUi.lon = bootUi.alt = NAN;
    bootUi.utc = "";
    bootUi.msgRx = 0;
    oled_draw_dashboard(bootUi, "--");
  }

  buttons_begin();

  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);

  SerialGPS.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
  gps_set_stale_timeout(10000);

  i2c_init_safe();

  if (i2c_probe(0x0E)){
    g_mag_ok = mag_begin();
    LOGF("[I2C] MAG %s", g_mag_ok ? "OK" : "NO");
  } else {
    LOG("[I2C] IST8310 no detectado");
  }

  bool ir_ok = false;
  if (i2c_probe(IRIDIUM_I2C_ADDR)){
    ir_ok = iridium_begin();
    LOGF("[IR] presente=%s", ir_ok ? "SI" : "NO");
  } else {
    LOG("[IR] sin ACK en 0x63; se omite init");
  }

  sd_ok = sd_begin_hspi();
  LOGF("[Owl] SD %s", sd_ok ? "OK" : "NO");

  ble_ok = ble_begin("OwlTracker");
  LOGF("[BLE] %s", ble_ok ? "advertising" : "init fail");

  xTaskCreatePinnedToCore(net_task, "net_task", 4096, nullptr, 1, nullptr, 0);
}

// ===================== LOOP =================================
void loop(){
  // ---------- BOTONES: prioridad máxima (cada ~2 ms) ----------
  static uint32_t t_btn = 0;
  if (millis() - t_btn >= 2){
    t_btn = millis();
    buttons_poll();

    // BTN1: short = HOME, long/repeat = ciclo info normal / pág testing
    if (auto e = btn1_get(); true){
      if (e.shortPress){
        g_screen = UiScreen::HOME;
      }
      if (e.longPress || e.repeat){
        if (g_screen == UiScreen::TESTING) {
          g_test_page = (uint8_t)((g_test_page + 1) % 2);
        } else {
          g_screen = next_screen_normal(g_screen);
        }
      }
    }

    // BTN2: short = MESSAGES, long = POI
    if (auto e2 = btn2_get(); true){
      if (e2.shortPress){
        if (g_screen != UiScreen::TESTING) {
          g_screen = UiScreen::MESSAGES;
        }
      }
      if (e2.longPress){
        if (g_screen != UiScreen::TESTING) {
          save_poi();
        }
      }
    }

    // BTN3: long = SOS
    if (auto e3 = btn3_get(); e3.longPress){
      if (g_screen != UiScreen::TESTING) {
        sos_trigger();
      }
    }

    // BTN4: short = rota modo, long = entrar/salir TESTING
    if (auto e4 = btn4_get(); true){
      if (e4.shortPress){
        if (g_screen != UiScreen::TESTING){
          auto m = comms_mode_get();
          m = comms_mode_next(m);
          comms_mode_set(m);
          const char* name = comms_mode_name(m);
          snprintf(g_last_test, sizeof(g_last_test), "Mode -> %s", name ? name : "AUTO");
        }
      }
      if (e4.longPress){
        if (g_screen == UiScreen::TESTING){
          g_screen = UiScreen::HOME;      // salir
        } else {
          g_test_page = 0;                 // entrar a testing (página 1)
          g_screen = UiScreen::TESTING;
        }
      }
    }
  }

  // Sensores no bloqueantes
  gps_poll(SerialGPS);
  if (g_mag_ok) mag_poll();
  iridium_poll();
  ble_poll();

  // ----------------- UPLINK (cada 5 s) -----------------
  static uint32_t t_uplink = 0;
  const uint32_t UPLINK_PERIOD_MS = netcfg::UPLINK_PERIOD_MS;
  if (millis() - t_uplink >= UPLINK_PERIOD_MS) {
    t_uplink = millis();

    // 1) Captura último fix y estatus Iridium
    GpsFix fx = gps_last_fix();
    IridiumInfo ir = iridium_status();

    // 2) Arma OwlReport base
    OwlReport rpt;
    rpt.tipoReporte = g_next_report_type; // 1 normal salvo eventos
    if (g_gsm_imei.length())      rpt.IMEI = g_gsm_imei;
    else if (ir.imei.length())    rpt.IMEI = ir.imei;
    else                          rpt.IMEI = "";

    rpt.latitud   = fx.valid ? fx.lat : NAN;
    rpt.longitud  = fx.valid ? fx.lon : NAN;
    rpt.altitud   = fx.valid ? fx.alt : NAN;
    rpt.rumbo     = fx.course_deg;
    rpt.velocidad = fx.speed_mps;
    rpt.fechaHora = fx.utc.length() ? (fx.utc + "Z") : String("");

    // 3) JSON plano
    String jsonPlain = make_json_from_report(rpt);

    // 4) BLE -> publicar EN CLARO (sin cifrar)
    ble_update(jsonPlain);

    // 5) Envío por portador según disponibilidad/prioridad
    if (!g_pref_iridium && g_net_registered && g_pdp_up) {
      // GSM/LTE primero
      String gcmB64 = make_gcm_b64_from_json(jsonPlain);
      // a) plano
      {
       String pathPlain = netcfg::PATH_PLAIN;
        if (modem_lock(200)) {
          int code_plain = http.postJson(pathPlain.c_str(),
                                         jsonPlain.c_str(),
                                         netcfg::AUTH_BEARER);
          modem_unlock();
          Serial.printf("[HTTP] POST %s -> %d\n", pathPlain.c_str(), code_plain);
        } else {
          Serial.println("[HTTP] skip (modem busy)");
        }
      }
      // b) cifrado
      {
        String pathGcm   = netcfg::PATH_GCM;
        String body = String("{\"gcm_b64\":\"") + gcmB64 + "\"}";
        if (modem_lock(200)) {
          int code_gcm = http.postJson(pathGcm.c_str(),
                                       body.c_str(),
                                       netcfg::AUTH_BEARER);
          modem_unlock();
          Serial.printf("[HTTP] POST %s -> %d\n", pathGcm.c_str(), code_gcm);
        } else {
          Serial.println("[HTTP] skip (modem busy gcm)");
        }
      }
    } else {
      // Prioridad Iridium (sin PDP o forzada)
      if (ir.present) {
        // TODO: reemplazar por tu API real de envío si la tienes:
        // bool ok = iridium_send_json(jsonPlain);
        bool ok = iridium_send_json_stub(jsonPlain);
        Serial.printf("[IR] send %s\n", ok ? "OK" : "FALLBACK");
      } else {
        Serial.println("[IR] no present; sólo BLE");
      }
    }

    // 6) Consumir tipo de reporte one-shot
    g_next_report_type = 1;
  }

  // ----------------- UI (1 Hz) -----------------
  static uint32_t t_ui = 0;
  if (millis() - t_ui >= 1000){
    t_ui = millis();

    GpsFix fx = gps_last_fix();
    IridiumInfo ir = iridium_status();

    // ------- UI data (desde caches + GNSS/IR) -------
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

    // ------- Render OLED si cambió -------
    static UiScreen lastScreen = UiScreen::HOME;
    static OwlUiData last = {};
    auto neq = [](double x, double y, double eps){ return !(fabs(x - y) <= eps); };
    auto changed = [&](const OwlUiData &a, const OwlUiData &b){
      return a.csq != b.csq || a.iridiumLvl != b.iridiumLvl || a.sats != b.sats ||
             neq(a.pdop, b.pdop, 0.1) || neq(a.lat, b.lat, 1e-7) ||
             neq(a.lon, b.lon, 1e-7) || neq(a.alt, b.alt, 0.3) ||
             a.msgRx != b.msgRx || a.utc != b.utc;
    };

    bool needRedraw = (g_screen != lastScreen) || changed(ui, last);
    if (needRedraw){
      switch (g_screen){
        case UiScreen::HOME:
          oled_draw_dashboard(ui, g_rat_label);
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
          auto csq_to_dbm = [](int csq)->int {
            if (csq <= 0)  return -113;
            if (csq == 1)  return -111;
            if (csq >= 31) return -51;
            return -113 + 2 * csq;
          };
          int rssiDbm = csq_to_dbm(g_csq_cache);

          extern bool ble_is_connected();
          bool bleConn = ble_is_connected();

          IridiumInfo ir2 = iridium_status();

          oled_draw_testing_adv(
            mode_name_from_comms(), g_net_registered, g_pdp_up,
            g_rat_label, g_csq_cache, rssiDbm, g_ip_cache.c_str(),
            g_gsm_imei.c_str(),
            ir2.present, ir2.sig, ir2.imei.c_str(), ir2.waiting?1:0,
            bleConn, (uint32_t)ESP.getFreeHeap(),
            g_last_test, g_test_busy, g_test_page
          );
        } break;
      }
      last = ui;
      lastScreen = g_screen;
    }
  }

  // ----------------- Log SD (30 s) -----------------
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
