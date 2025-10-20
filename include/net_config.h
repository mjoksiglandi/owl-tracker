#pragma once

namespace netcfg {

// ===== SIM/APN =====
inline constexpr const char* SIM_PIN  = "9222";             // "" si no usa PIN
inline constexpr const char* APN      = "bam.entelpcs.cl";
inline constexpr const char* APN_USER = "";
inline constexpr const char* APN_PASS = "";

// ===== HTTP destino =====
// Si usas HTTPS real, ver "USE_TLS" en modem_manager.cpp
inline constexpr const char* POST_HOST = "192.168.1.100";
inline constexpr int         POST_PORT = 3000;
inline constexpr const char* POST_PATH = "/api/ingest";

// ===== Timings =====
inline constexpr uint32_t NET_BACKOFF_MIN_MS = 5000;        // 5 s
inline constexpr uint32_t NET_BACKOFF_MAX_MS = 300000;      // 5 min
inline constexpr uint32_t POST_MIN_GAP_MS    = 200;         // gap entre POST
inline constexpr uint32_t DIAG_PERIOD_MS     = 10000;       // diag cada 10 s

// ===== Demo (opcional) =====
inline constexpr uint32_t DEMO_PERIOD_MS = 30000;           // cada 30 s

// ===== TLS =====
// Para laboratorio deja en false (HTTP). Si pasas a HTTPS, habilita en .cpp:
// - usar TinyGsmClientSecure y/o setInsecure() con cuidado.
inline constexpr bool USE_TLS = false;

} // namespace netcfg

// Overrides locales opcionales (gitignored)
#if __has_include("net_secrets.h")
  #include "net_secrets.h"
#endif
