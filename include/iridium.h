#pragma once
#include <Arduino.h>

#ifndef IRIDIUM_I2C_ADDR
#define IRIDIUM_I2C_ADDR 0x63
#endif

// Límite unificado (downlink = 270); capamos uplink también a 270
static const size_t IRIDIUM_MAX_TX = 270;
static const size_t IRIDIUM_MAX_RX = 270;

struct IridiumInfo {
  bool   present = false;
  int    sig     = -1;      // 0..5
  bool   waiting = false;   // MT pendientes
  String imei    = "";
};

bool        iridium_begin(uint8_t i2c_addr = IRIDIUM_I2C_ADDR);
void        iridium_poll();
IridiumInfo iridium_status();

bool iridium_send_cipher_b64(const String& cipher);  // ≤270 bytes
bool iridium_fetch_next(String& outCipher);          // stub no bloqueante

inline bool     iridium_present()        { return iridium_status().present; }
inline int      iridium_signal_quality() { return iridium_status().sig;     }
inline uint8_t  iridium_unread()         { return iridium_status().waiting ? 1 : 0; }
inline String   iridium_imei()           { return iridium_status().imei;    }
