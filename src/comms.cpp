#include "comms.h"
#include "net_config.h"
#include "modem_config.h"
#include <TinyGsmClient.h>

// Instancias globales (definidas en main.cpp)
extern TinyGsm modem;
extern OwlHttpClient http;

// -------- Helpers modem locales --------
static inline bool modem_is_registered() {
  return modem.isNetworkConnected();
}
static inline bool modem_is_pdp_up() {
#ifdef TINY_GSM_MODEM_SIM7600
  return modem.isGprsConnected();
#else
  return modem.localIP() != IPAddress(0,0,0,0);
#endif
}
static inline bool modem_try_pdp(const char* apn, const char* user, const char* pass) {
  return modem.gprsConnect(apn, user, pass);
}

// -------- Estado Comms --------
static CommsMode   g_mode = CommsMode::AUTO;
static CommsStatus g_stat;

static uint32_t t_gsm_chk = 0;
static uint32_t t_ir_chk  = 0;
static uint32_t t_ble_hb  = 0;

// -------- Formatos --------
String make_report_json(const OwlReport& r) {
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"tipoReporte\":%u,\"IMEI\":\"%s\",\"latitud\":%.6f,\"longitud\":%.6f,"
    "\"altitud\":%.1f,\"rumbo\":%.1f,\"velocidad\":%.2f,\"fechaHora\":\"%s\"}",
    r.tipoReporte, r.IMEI.c_str(),
    r.latitud, r.longitud, r.altitud,
    r.rumbo, r.velocidad, r.fechaHora.c_str());
  return String(buf);
}

String make_report_cipher_b64(const OwlReport& r) {
  extern const uint8_t OWL_AES256_KEY[32];
  extern const size_t  OWL_GCM_IV_LEN; // 12
  String plain = make_report_json(r);
  return owl_encrypt_aes256gcm_base64(
    OWL_AES256_KEY,
    (const uint8_t*)plain.c_str(), plain.length(),
    nullptr, 0   // IV interno (ajusta si tu crypto exige IV explícito)
  );
}

void comms_begin() {
  g_stat = {};
  g_stat.mode = g_mode;
}

void comms_set_mode(CommsMode m) { g_mode = m; g_stat.mode = m; }
CommsMode comms_get_mode()       { return g_mode; }

// -------- Canales --------
static bool send_via_gsm(const String& payloadCipher) {
  if (!modem_is_pdp_up()) return false;
  // Enviamos JSON { "data": "<base64>" } vía PUT al PATH
  String body = String("{\"data\":\"") + payloadCipher + "\"}";
  int code = http.putJson(netcfg::PATH, body.c_str(), netcfg::AUTH_BEARER);
  return (code >= 200 && code < 300);
}

static bool send_via_iridium(const String& payloadCipher) {
  return iridium_send_cipher_b64(payloadCipher);
}

static void notify_via_ble(const String& payloadCipher) {
  ble_notify_report_json(payloadCipher);
}

bool comms_send_report(const OwlReport& rpt) {
  String cipher = make_report_cipher_b64(rpt);
  bool ok_gsm=false, ok_ir=false;

  switch (g_mode) {
    case CommsMode::GSM_ONLY:
      ok_gsm = send_via_gsm(cipher); break;
    case CommsMode::IR_ONLY:
      ok_ir  = send_via_iridium(cipher); break;
    case CommsMode::BOTH:
      ok_gsm = send_via_gsm(cipher);
      ok_ir  = send_via_iridium(cipher);
      break;
    case CommsMode::AUTO:
    default:
      if (modem_is_pdp_up()) {
        ok_gsm = send_via_gsm(cipher);
        if (!ok_gsm) ok_ir = send_via_iridium(cipher);
      } else {
        ok_ir = send_via_iridium(cipher);
      }
      break;
  }

  notify_via_ble(cipher);
  return ok_gsm || ok_ir;
}

void comms_poll() {
  // GSM check (2 s)
  if (millis() - t_gsm_chk > 2000) {
    t_gsm_chk = millis();
    bool reg = modem_is_registered();
    if (reg && !modem_is_pdp_up()) {
      if (g_mode != CommsMode::IR_ONLY) {
        (void)modem_try_pdp(netcfg::APN, netcfg::APN_USER, netcfg::APN_PASS);
      }
    }
    g_stat.gsm_ready = modem_is_pdp_up();
  }

  // Iridium check + ingest (2~3 s)
  if (millis() - t_ir_chk > 2000) {
    t_ir_chk = millis();
    IridiumInfo ir = iridium_status();
    g_stat.ir_present = ir.present;
    g_stat.ir_rssi    = ir.sig;

    if (ir.present && ir.waiting) {
      String cipher;
      // RX queda pendiente (stub); mantén inbox como contador si quieres.
      // while (iridium_fetch_next(cipher)) { inbox_push("IR", cipher); }
    }
  }

  // BLE link (1 s)
  if (millis() - t_ble_hb > 1000) {
    t_ble_hb = millis();
    g_stat.ble_link = ble_is_connected();
  }

  g_stat.inbox = inbox_count();
}

CommsStatus comms_status() { return g_stat; }
