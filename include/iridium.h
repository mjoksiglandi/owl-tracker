#pragma once
#include <Arduino.h>

#ifndef IRIDIUM_I2C_ADDR
#define IRIDIUM_I2C_ADDR 0x63
#endif

struct IridiumInfo {
  bool   present = false;   // el módulo respondió a begin()
  int    sig     = -1;      // 0..5  (-1 = sin dato)
  bool   waiting = false;   // hay MT pendientes en la red
  String imei    = "";      // IMEI cacheado
};

bool        iridium_begin(uint8_t i2c_addr = IRIDIUM_I2C_ADDR);
void        iridium_poll();
IridiumInfo iridium_status();

// Envío/recepción (por ahora sólo TX; RX queda stub porque la lib I2C no expone readSBDText)
bool   iridium_send_cipher_b64(const String& cipher);
bool   iridium_fetch_next(String& outCipher);  // ← stub no bloqueante

// Helpers inline para UI
inline bool     iridium_present()        { return iridium_status().present; }
inline int      iridium_signal_quality() { return iridium_status().sig;     }
inline uint8_t  iridium_unread()         { return iridium_status().waiting ? 1 : 0; }
inline String   iridium_imei()           { return iridium_status().imei;    }
