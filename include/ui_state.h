#pragma once
#include <Arduino.h>

enum class UiScreen : uint8_t {
  HOME = 0,
  GPS_DETAIL,
  IRIDIUM_DETAIL,
  GSM_DETAIL,     // nuevo
  BLE_DETAIL,     // nuevo
  SYS_CONFIG,
  MESSAGES
};

// Helper opcional para iterar pantallas
inline UiScreen ui_next(UiScreen s) {
  uint8_t v = static_cast<uint8_t>(s);
  v = (v + 1) % 7; // actualiza si agregas/eliminas pantallas
  return static_cast<UiScreen>(v);
}
