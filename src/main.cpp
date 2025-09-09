#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <TinyGsmClient.h>

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

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// UARTs
HardwareSerial SerialAT(1);
HardwareSerial SerialGPS(2);

TinyGsm modem(SerialAT);
OwlHttpClient http;
SPIClass hspi(HSPI);

// Estado global
static bool sd_ok = false;
static bool g_mag_ok = false;
static bool ble_ok = false;

static volatile bool g_net_registered = false;
static volatile bool g_pdp_up = false;

// IMEI GSM (cache) y tipo de reporte “one-shot”
static String g_gsm_imei = "";
static int g_next_report_type = 1; // 1=Normal, 2=Emergencia, 3=Libre, 4=POI

// Utils
static inline void LOG(const char *s)
{
  if (Serial)
    Serial.println(s);
}
static inline void LOGF(const char *fmt, ...)
{
  if (!Serial)
    return;
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
}

// ===================== SD helpers (HSPI) =====================
static bool sd_begin_hspi()
{
  hspi.end();
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, hspi))
    return false;
  if (SD.cardType() == CARD_NONE)
    return false;
  File root = SD.open("/");
  if (!root)
    return false;
  root.close();
  return true;
}
static void sd_logln(const String &line)
{
  if (!sd_ok)
    return;
  File f = SD.open("/logs/track.csv", FILE_APPEND);
  if (!f)
  {
    SD.mkdir("/logs");
    f = SD.open("/logs/track.csv", FILE_APPEND);
  }
  if (f)
  {
    f.println(line);
    f.close();
  }
}

// ===================== I2C seguro ============================
static void i2c_init_safe()
{
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(20);
}
static bool i2c_probe(uint8_t addr)
{
  Wire.beginTransmission(addr);
  uint8_t e = Wire.endTransmission();
  return (e == 0);
}

// ===================== Red (tarea) ===========================
static void net_task(void *arg)
{
  enum class NState
  {
    ST_POWER,
    ST_AT_PROBE,
    ST_SET_LTE,
    ST_REG_WAIT,
    ST_PDP_UP,
    ST_RUN,
    ST_SLEEP
  };
  NState st = NState::ST_POWER;

  uint32_t backoff_ms = 2000;
  const uint32_t backoff_max = 30000;

  for (;;)
  {
    switch (st)
    {
    case NState::ST_POWER:
    {
      modem_power_on(PIN_MODEM_EN, PIN_MODEM_PWR, true, 1200, 6000);
      LOG("[NET] power on");
      st = NState::ST_AT_PROBE;
    }
    break;

    case NState::ST_AT_PROBE:
    {
      bool ok = false;
      uint32_t t0 = millis();
      for (int i = 0; i < 5 && millis() - t0 < 3000 && !ok; ++i)
      {
        SerialAT.print("AT\r");
        delay(200);
        String buf;
        while (SerialAT.available())
          buf += (char)SerialAT.read();
        if (buf.indexOf("OK") >= 0)
          ok = true;
      }
      if (!ok)
      {
        LOG("[NET] no AT, backoff");
        st = NState::ST_SLEEP;
        break;
      }
      st = NState::ST_SET_LTE;
    }
    break;

    case NState::ST_SET_LTE:
    {
      if (!modem_set_lte(modem))
      {
        LOG("[NET] LTE only falló -> AUTO");
        modem_set_auto(modem);
      }
      st = NState::ST_REG_WAIT;
    }
    break;

    case NState::ST_REG_WAIT:
    {
      bool reg = modem_wait_for_network(modem, 5000);
      g_net_registered = reg;
      if (!reg)
      {
        LOG("[NET] sin registro, backoff");
        st = NState::ST_SLEEP;
        break;
      }
      st = NState::ST_PDP_UP;
    }
    break;

    case NState::ST_PDP_UP:
    {
      bool up = modem.gprsConnect(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
      g_pdp_up = up;
      LOGF("[NET] PDP %s", up ? "UP" : "DOWN");
      st = up ? NState::ST_RUN : NState::ST_SLEEP;
    }
    break;

    case NState::ST_RUN:
    {
      vTaskDelay(pdMS_TO_TICKS(3000));
      bool reg = modem.isNetworkConnected();
      g_net_registered = reg;
#ifdef TINY_GSM_MODEM_SIM7600
      bool pdp = modem.isGprsConnected();
#else
      bool pdp = (modem.localIP() != IPAddress(0, 0, 0, 0));
#endif
      g_pdp_up = pdp;
      if (!reg || !pdp)
      {
        LOG("[NET] RUN->SLEEP");
        st = NState::ST_SLEEP;
      }
    }
    break;

    case NState::ST_SLEEP:
    {
      vTaskDelay(pdMS_TO_TICKS(backoff_ms));
      backoff_ms = (backoff_ms < backoff_max) ? (backoff_ms * 2) : backoff_max;
      st = NState::ST_POWER;
    }
    break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ===================== Acciones UI ===========================
static void save_poi()
{
  GpsFix fx = gps_last_fix();
  if (!fx.valid)
  {
    LOG("[Owl] POI cancelado: sin fix");
    return;
  }
  String s = "POI," + fx.utc + "," + String(fx.lat, 6) + "," + String(fx.lon, 6) + "," + String(fx.alt, 1);
  LOG(("[Owl] " + s).c_str());
  sd_logln(s);
  g_next_report_type = 4; // POI
}
static void sos_trigger()
{
  LOG("[Owl] SOS TRIGGERED!");
  sd_logln(String("SOS,") + String(millis()));
  g_next_report_type = 2; // Emergencia
}

// ===================== Pantallas =============================
static UiScreen g_screen = UiScreen::HOME;
static void next_screen()
{
  uint8_t v = static_cast<uint8_t>(g_screen);
  v = (v + 1) % 5;
  g_screen = static_cast<UiScreen>(v);
}

// ===================== SETUP =================================
void setup()
{
  Serial.begin(115200);
  delay(50);
  LOG("\n[Owl] Boot");

  // RF: solo BLE (Wi-Fi OFF)
  WiFi.mode(WIFI_OFF);

  // OLED primero (feedback inmediato)
  oled_init();
  oled_splash("Owl Tracker");
  delay(400);

  // HOME “en vacío” para confirmar vida
  {
    OwlUiData bootUi;
    bootUi.csq = 99;
    bootUi.iridiumLvl = -1;
    bootUi.sats = -1;
    bootUi.pdop = -1;
    bootUi.lat = bootUi.lon = bootUi.alt = NAN;
    bootUi.utc = "";
    bootUi.msgRx = 0;
    oled_draw_dashboard(bootUi);
  }

  // Botonera
  buttons_begin();

  // UART módem (net_task hará el trabajo de red)
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);

  // GNSS
  SerialGPS.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  gps_begin_uart(SerialGPS, PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD);
  gps_set_stale_timeout(10000);

  // I2C
  i2c_init_safe();

  // MAG (IST8310 en 0x0E)
  if (i2c_probe(0x0E))
  {
    g_mag_ok = mag_begin();
    LOGF("[I2C] MAG %s", g_mag_ok ? "OK" : "NO");
  }
  else
  {
    LOG("[I2C] IST8310 no detectado");
  }

  // **Iridium por I2C** (solo si hay ACK @0x63)
  bool ir_ok = false;
  if (i2c_probe(IRIDIUM_I2C_ADDR))
  {
    ir_ok = iridium_begin();
    LOGF("[IR] presente=%s", ir_ok ? "SI" : "NO");
  }
  else
  {
    LOG("[IR] sin ACK en 0x63; se omite init (no bloqueante)");
  }

  // SD
  sd_ok = sd_begin_hspi();
  LOGF("[Owl] SD %s", sd_ok ? "OK" : "NO");

  // BLE
  ble_ok = ble_begin("OwlTracker");
  LOGF("[BLE] %s", ble_ok ? "advertising" : "init fail");

  // Tarea de red
  xTaskCreatePinnedToCore(net_task, "net_task", 4096, nullptr, 1, nullptr, 0);
}

// ===================== LOOP =================================
void loop()
{
  // Sensores no bloqueantes
  gps_poll(SerialGPS);
  if (g_mag_ok)
    mag_poll();

  // Iridium no bloqueante
  iridium_poll();

  // BLE
  ble_poll();

  // Botones (10 ms)
  static uint32_t t_btn = 0;
  if (millis() - t_btn >= 10)
  {
    t_btn = millis();
    buttons_poll();

    if (auto e = btn1_get(); e.shortPress)
    {
      g_screen = UiScreen::HOME;
      LOG("[Owl] HOME");
    }
    else if (e.longPress)
    {
      next_screen();
      LOG("[Owl] Cambiar pantalla");
    }

    if (auto e2 = btn2_get(); e2.shortPress)
    {
      g_screen = UiScreen::MESSAGES;
      LOG("[Owl] MESSAGES");
    }
    else if (e2.longPress)
    {
      save_poi();
    }

    if (auto e3 = btn3_get(); e3.longPress)
    {
      sos_trigger();
    }
  }

  // UI (1 Hz) con redraw condicional + BLE report
  static uint32_t t_ui = 0;
  if (millis() - t_ui >= 1000)
  {
    t_ui = millis();

    GpsFix fx = gps_last_fix();
    IridiumInfo ir = iridium_status();

    // ------- UI data -------
    OwlUiData ui;
    ui.csq = modem.getSignalQuality();
    ui.iridiumLvl = (ir.sig >= 0) ? ir.sig : -1; // 0..5 o -1
    ui.lat = fx.valid ? fx.lat : NAN;
    ui.lon = fx.valid ? fx.lon : NAN;
    ui.alt = fx.valid ? fx.alt : NAN;
    ui.sats = fx.sats;
    ui.pdop = fx.pdop;
    ui.speed_mps = fx.speed_mps;
    ui.course_deg = fx.course_deg;
    ui.pressure_hpa = NAN;         // baro removido
    ui.msgRx = ir.waiting ? 1 : 0; // 1 si hay MT en red
    ui.utc = fx.utc.length() ? (fx.utc + "Z") : String("");

    // ------- IMEI prioritario GSM (cachea una vez) -------
    if (g_gsm_imei.length() == 0 && g_net_registered)
    {
      String imeiTry = modem.getIMEI();
      if (imeiTry.length() > 0)
        g_gsm_imei = imeiTry;
    }

    // ------- Reporte BLE JSON unificado -------
    OwlReport rpt;
    rpt.tipoReporte = g_next_report_type; // 1 normal salvo eventos

    if (g_gsm_imei.length())
      rpt.IMEI = g_gsm_imei;
    else if (ir.imei.length())
      rpt.IMEI = ir.imei;
    else
      rpt.IMEI = "";

    rpt.latitud = fx.valid ? fx.lat : NAN;
    rpt.longitud = fx.valid ? fx.lon : NAN;
    rpt.altitud = fx.valid ? fx.alt : NAN;
    rpt.rumbo = fx.course_deg;
    rpt.velocidad = fx.speed_mps;
    rpt.fechaHora = fx.utc.length() ? (fx.utc + "Z") : String("");

    ble_update(rpt);        // ← Envío por BLE con el formato requerido
    g_next_report_type = 1; // consumir y volver a Normal

    // ------- Render OLED si cambió -------
    static UiScreen lastScreen = UiScreen::HOME;
    static OwlUiData last = {};
    auto neq = [](double x, double y, double eps)
    { return !(fabs(x - y) <= eps); };
    auto changed = [&](const OwlUiData &a, const OwlUiData &b)
    {
      return a.csq != b.csq || a.iridiumLvl != b.iridiumLvl || a.sats != b.sats ||
             neq(a.pdop, b.pdop, 0.1) || neq(a.lat, b.lat, 1e-7) ||
             neq(a.lon, b.lon, 1e-7) || neq(a.alt, b.alt, 0.3) ||
             a.msgRx != b.msgRx || a.utc != b.utc;
    };

    bool needRedraw = (g_screen != lastScreen) || changed(ui, last);
    if (needRedraw)
    {
      switch (g_screen)
      {
      case UiScreen::HOME:
        oled_draw_dashboard(ui);
        break;

      case UiScreen::GPS_DETAIL:
        oled_draw_gps_detail(ui);
        break;

      case UiScreen::IRIDIUM_DETAIL:
        oled_draw_iridium_detail(ir.present, ir.sig, ir.waiting ? 1 : 0, ir.imei);
        break;

      case UiScreen::SYS_CONFIG:
      {
        const String ipStr = modem.localIP().toString();
        const bool i2cOk = (g_mag_ok || ir.present); // I2C OK si MAG o IR presentes
        String fwStr = String("fw 1.0.0 ") + (ble_ok ? "BLE:OK" : "BLE:NO");
        oled_draw_sys_config(ui, g_net_registered, g_pdp_up, ipStr, sd_ok, i2cOk, fwStr.c_str());
      }
      break;

      case UiScreen::MESSAGES:
        oled_draw_messages(ir.waiting ? 1 : 0, "");
        break;
      }
      last = ui;
      lastScreen = g_screen;
    }

    LOG("[Owl] UI tick");
  }

  // Log SD (30 s)
  static uint32_t t_log = 0;
  if (sd_ok && millis() - t_log > 30000)
  {
    t_log = millis();
    GpsFix fx = gps_last_fix();
    String s = String(fx.utc) + "," +
               (fx.valid ? String(fx.lat, 6) : "NaN") + "," +
               (fx.valid ? String(fx.lon, 6) : "NaN") + "," +
               (fx.valid ? String(fx.alt, 1) : "NaN");
    sd_logln(s);
  }
}
