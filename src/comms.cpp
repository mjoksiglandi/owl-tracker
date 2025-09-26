#include <Arduino.h>
#include "net_config.h"
#include "http_client.h"
#include "crypto.h"
#include "secrets.h"
#include "iridium.h"
#include "ble.h"

// Declarados en main.cpp
extern OwlHttpClient http;
extern String g_gsm_imei;

// ============ Utilidades ============

static String make_gcm_b64_from_json(const String& jsonPlain) {
  // IV aleatorio de OWL_GCM_IV_LEN bytes
  uint8_t iv[OWL_GCM_IV_LEN];
  for (size_t i = 0; i < OWL_GCM_IV_LEN; ++i) iv[i] = (uint8_t)(esp_random() & 0xFF);

  // Cifra y devuelve en Base64 (ct|tag|iv => manejado dentro de crypto)
  String b64 = owl_encrypt_aes256gcm_base64(
    OWL_AES256_KEY,
    (const uint8_t*)jsonPlain.c_str(), jsonPlain.length(),
    iv, sizeof(iv)
  );
  return b64;
}

// ============ Envíos por portador ============

// GSM/LTE (HTTP): devuelve true si el backend respondió 2xx
bool send_via_gsm(const String& jsonPlain)
{
  // 1) Plano
  int code_plain = http.postJson(netcfg::PATH_PLAIN, jsonPlain.c_str(), netcfg::AUTH_BEARER);
  Serial.printf("[HTTP] POST %s -> %d\n", netcfg::PATH_PLAIN, code_plain);

  // 2) Cifrado GCM
  String gcmB64 = make_gcm_b64_from_json(jsonPlain);
  String body   = String("{\"gcm_b64\":\"") + gcmB64 + "\"}";
  int code_gcm  = http.postJson(netcfg::PATH_GCM, body.c_str(), netcfg::AUTH_BEARER);
  Serial.printf("[HTTP] POST %s -> %d\n", netcfg::PATH_GCM, code_gcm);

  // Considera éxito si al menos uno fue 2xx
  bool ok_plain = (code_plain >= 200 && code_plain < 300);
  bool ok_gcm   = (code_gcm  >= 200 && code_gcm  < 300);
  return ok_plain || ok_gcm;
}

// Iridium: aquí sólo encola/manda en claro; ajusta si tienes tu API real
bool send_via_iridium(const String& jsonPlain)
{
  // Si ya tienes una función real, reemplaza por: return iridium_send_json(jsonPlain);
  Serial.printf("[IR] (send-json) %s\n", jsonPlain.c_str());
  // Opcional: usa iridium_queue(jsonPlain) y retorna true si se encoló
  return false;
}

// BLE: publica en claro
bool send_via_ble(const String& jsonPlain)
{
  ble_update(jsonPlain);
  Serial.printf("[BLE] advertised/plain: %s\n", jsonPlain.c_str());
  return true;
}
