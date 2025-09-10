// src/crypto.cpp
#include "crypto.h"
#include <mbedtls/gcm.h>
#include <mbedtls/base64.h>
#include <esp_system.h>   // esp_random
#include "secrets.h"
#include <string.h>
#include <vector>         // <- usamos vector en lugar de unique_ptr

// Base64 helpers
static bool b64_encode(const uint8_t* in, size_t inlen, String& out) {
  if (!in || inlen == 0) { out = ""; return true; }
  size_t olen = 0;
  // Consulta tamaño necesario
  (void) mbedtls_base64_encode(nullptr, 0, &olen, in, inlen);

  std::vector<uint8_t> buf; buf.resize(olen + 1);
  if (mbedtls_base64_encode(buf.data(), olen, &olen, in, inlen) != 0) return false;
  buf[olen] = 0;
  out = String((const char*)buf.data());
  return true;
}

static bool b64_decode(const char* s, std::vector<uint8_t>& out) {
  if (!s) { out.clear(); return true; }
  size_t slen = strlen(s);
  size_t olen = 0;
  (void) mbedtls_base64_decode(nullptr, 0, &olen, (const uint8_t*)s, slen);
  out.resize(olen);
  if (mbedtls_base64_decode(out.data(), olen, &olen, (const uint8_t*)s, slen) != 0) return false;
  out.resize(olen);
  return true;
}

String owl_encrypt_aes256gcm_base64(const uint8_t* key,
                                    const uint8_t* plaintext, size_t plen,
                                    const uint8_t* aad, size_t aad_len)
{
  if (!key || !plaintext || plen==0) return String();

  // IV aleatorio por mensaje (12B recomendado)
  uint8_t iv[16] = {0};
  const size_t iv_len = OWL_GCM_IV_LEN; // 12
  for (size_t i=0;i<iv_len;i++) iv[i] = (uint8_t) (esp_random() & 0xFF);

  // Salida cifrada
  std::vector<uint8_t> cipher; cipher.resize(plen);
  uint8_t tag[OWL_GCM_TAG_LEN] = {0};

  // GCM
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (rc != 0) { mbedtls_gcm_free(&gcm); return String(); }

  rc = mbedtls_gcm_crypt_and_tag(&gcm,
                                 MBEDTLS_GCM_ENCRYPT,
                                 plen,
                                 iv, iv_len,
                                 aad, aad_len,
                                 plaintext, cipher.data(),
                                 OWL_GCM_TAG_LEN, tag);

  mbedtls_gcm_free(&gcm);
  if (rc != 0) return String();

  // Empaquetar en Base64: gcm://b64(iv)|b64(tag)|b64(cipher)
  String iv64, tag64, ct64;
  if (!b64_encode(iv, iv_len, iv64)) return String();
  if (!b64_encode(tag, OWL_GCM_TAG_LEN, tag64)) return String();
  if (!b64_encode(cipher.data(), cipher.size(), ct64)) return String();

  String out = String("gcm://") + iv64 + "|" + tag64 + "|" + ct64;
  return out;
}

String owl_decrypt_aes256gcm_base64(const uint8_t* key,
                                    const String& payload,
                                    const uint8_t* aad, size_t aad_len)
{
  if (!key || payload.length() < 6) return String();
  if (!payload.startsWith("gcm://")) return String();

  // split por '|'
  int p1 = payload.indexOf('|', 6);
  int p2 = payload.indexOf('|', p1+1);
  if (p1<0 || p2<0) return String();
  String sIv  = payload.substring(6, p1);
  String sTag = payload.substring(p1+1, p2);
  String sCt  = payload.substring(p2+1);

  // decode
  std::vector<uint8_t> iv, tag, ct;
  if (!b64_decode(sIv.c_str(), iv))   return String();
  if (!b64_decode(sTag.c_str(), tag)) return String();
  if (!b64_decode(sCt.c_str(), ct))   return String();
  if (tag.size() != OWL_GCM_TAG_LEN)  return String();

  std::vector<uint8_t> plain; plain.resize(ct.size());

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (rc != 0) { mbedtls_gcm_free(&gcm); return String(); }

  rc = mbedtls_gcm_auth_decrypt(&gcm,
                                ct.size(),
                                iv.data(), iv.size(),
                                aad, aad_len,
                                tag.data(), tag.size(),
                                ct.data(), plain.data());
  mbedtls_gcm_free(&gcm);
  if (rc != 0) return String();

  // convertir a String
  plain.push_back(0); // null-terminate
  return String((const char*)plain.data());
}
