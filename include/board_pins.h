#pragma once

// UART hacia el módem A7670G en la LilyGO T-A7670G (revisión típica)
static constexpr int PIN_MODEM_TX  = 26;  // ESP32 -> A7670 RX
static constexpr int PIN_MODEM_RX  = 27;  // ESP32 <- A7670 TX
static constexpr int PIN_MODEM_PWR = 4;   // Power key / enable (pulsador por software)

// UART que usaremos para el módem
static constexpr int UART_NUM_MODEM = 1;  // Serial1
