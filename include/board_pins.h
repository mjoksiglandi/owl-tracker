#pragma once

// =======================
//  LILYGO T-A7670G (R2)
// =======================

// --- UART del módem (SIMCom A7670G)
#define PIN_MODEM_TX   26
#define PIN_MODEM_RX   27
#define PIN_MODEM_PWR   4
#define PIN_MODEM_EN   12

// --- OLED SSD1322 256x64 SPI
#define PIN_OLED_SCK   18
#define PIN_OLED_MOSI  23
#define PIN_OLED_CS     5
#define PIN_OLED_DC    21
#define PIN_OLED_RST   22

#ifndef OLED_PIN_SCK
  #define OLED_PIN_SCK   PIN_OLED_SCK
#endif
#ifndef OLED_PIN_MOSI
  #define OLED_PIN_MOSI  PIN_OLED_MOSI
#endif
#ifndef OLED_PIN_CS
  #define OLED_PIN_CS    PIN_OLED_CS
#endif
#ifndef OLED_PIN_DC
  #define OLED_PIN_DC    PIN_OLED_DC
#endif
#ifndef OLED_PIN_RST
  #define OLED_PIN_RST   PIN_OLED_RST
#endif

// --- GPS externo (NMEA por UART2 del ESP32)
#define PIN_GPS_TX     33    // ESP32 TX -> RX del GPS (opcional)
#define PIN_GPS_RX     34    // ESP32 RX <- TX del GPS (OBLIGATORIO)
#define PIN_GPS_PWR    -1    // si tu módulo trae enable, pon el pin aquí; si no, deja -1
#define GPS_BAUD       115200  // la mayoría de módulos NMEA por defecto
