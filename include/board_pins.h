#pragma once

// =======================
//  LILYGO T-A7670G (R2)
// =======================

// --- MODEM (A7670G)
#define PIN_MODEM_TX   26
#define PIN_MODEM_RX   27
#define PIN_MODEM_PWR   4
#define PIN_MODEM_EN   12
#define MODEM_BAUD     115200

// --- OLED SSD1322 256x64 (VSPI)
#define PIN_OLED_SCK   18
#define PIN_OLED_MOSI  23
#define PIN_OLED_CS     5
#define PIN_OLED_DC    32
#define PIN_OLED_RST   33

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

// --- I2C (baro, mag, futuros)
#define PIN_I2C_SDA    21
#define PIN_I2C_SCL    22
#define I2C_FREQ_HZ    400000

// --- SD (HSPI)
#define PIN_SD_MISO     2
#define PIN_SD_MOSI    15
#define PIN_SD_SCK     14
#define PIN_SD_CS      13

// --- GPS (UART2)
#define PIN_GPS_TX     -1     // no conectado
#define PIN_GPS_RX     34     // RX desde GPS
#define PIN_GPS_PWR    -1
#define GPS_BAUD       115200

// --- BOTONES (INPUT_PULLUP, activos LOW)
// ⚠ BTN3 en GPIO12: no presionar durante boot/reset
#define PIN_BTN1       25
#define PIN_BTN2       19
#define PIN_BTN3       12
