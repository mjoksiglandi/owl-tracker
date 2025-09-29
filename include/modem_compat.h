#pragma once

// Marca si el stack TinyGSM expone API tipo SIM7600/A7670 (isGprsConnected(), etc.)
#if defined(TINY_GSM_MODEM_A7670) || defined(TINY_GSM_MODEM_SIM7600)
  #define TINYGSM_HAS_SIM76XX_API 1
#else
  #define TINYGSM_HAS_SIM76XX_API 0
#endif
