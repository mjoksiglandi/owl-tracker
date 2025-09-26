#pragma once
#include <Arduino.h>

namespace cfg {
  inline constexpr uint32_t SERIAL_BAUD_USB   = 115200;
  inline constexpr uint32_t SERIAL_BAUD_MODEM = 115200;

  inline constexpr uint32_t BOOT_WAIT_MS  = 500;   // respiro de arranque
  inline constexpr uint32_t LOOP_DELAY_MS = 50;    // loop yield
} // namespace cfg
