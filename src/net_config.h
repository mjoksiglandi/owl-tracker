#pragma once
#include <Arduino.h>

namespace netcfg {
inline const char* APN      = "internet.wom.cl";  // WOM Chile
inline const char* APN_USER = "";
inline const char* APN_PASS = "";

// Aún sin destino API:
inline const char* HOST = "example.com";
inline const uint16_t PORT = 80;
inline const char* PATH = "/noop";
inline const char* AUTH_BEARER = "";
inline const char* ROOT_CA = R"( )";
inline const uint32_t PUT_PERIOD_MS = 5000;
}
