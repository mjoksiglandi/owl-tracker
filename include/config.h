#pragma once
#include <Arduino.h>

namespace cfg {
  // Serial
  inline constexpr uint32_t SERIAL_BAUD_USB   = 115200;
  inline constexpr uint32_t SERIAL_BAUD_MODEM = 115200;

  // Timings generales
  inline constexpr uint32_t BOOT_WAIT_MS  = 500;   // respiro de arranque
  inline constexpr uint32_t LOOP_DELAY_MS = 50;
}

// (Para la etapa de red/módem — placeholders sin uso aún)
namespace netcfg {
  // APN (ajústalo después según tu operador)
  inline String APN      = "bam.claro.cl";
  inline String APN_USER = "";
  inline String APN_PASS = "";
}
