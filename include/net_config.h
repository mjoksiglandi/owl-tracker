#pragma once
#include <Arduino.h>

namespace netcfg {

// ≡ Datos de red (ajusta a tu operador/backend)
inline const char* APN       = "bam.entelpcs.cl"; //internet.wom.cl
inline const char* APN_USER  = "";
inline const char* APN_PASS  = "";

inline const char* HOST      = "v4.ident.me";
inline const uint16_t PORT   = 80;
// Endpoints opcionales (si usas distintos para claro/cifrado)
inline const char* PATH_PLAIN = "/plain";
inline const char* PATH_GCM   = "/gcm";

inline const char* AUTH_BEARER = "";    // "Bearer xxx" si aplica
inline const char* ROOT_CA     = R"( )";

// ≡ Períodos
inline const uint32_t UPLINK_PERIOD_MS = 10000;  // ← 10 s para GSM, Iridium y BLE

} // namespace netcfg
