#pragma once
#include <Arduino.h>

// Devuelve payload "gcm://b64(iv)|b64(tag)|b64(cipher)"
// AAD opcional (puedes pasar nullptr, 0)
// key: 32 bytes (AES-256)
// Retorna String vacío si falla.
String owl_encrypt_aes256gcm_base64(const uint8_t* key,
                                    const uint8_t* plaintext, size_t plen,
                                    const uint8_t* aad, size_t aad_len);

// Inversa: parsea "gcm://..." y devuelve plaintext en out (String vacío si falla)
String owl_decrypt_aes256gcm_base64(const uint8_t* key,
                                    const String& payload,
                                    const uint8_t* aad, size_t aad_len);
