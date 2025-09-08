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

// --- GPS externo (NMEA por UART2 del ESP32)
#define PIN_GPS_TX     33    // ESP32 TX -> RX del GPS (opcional)
#define PIN_GPS_RX     34    // ESP32 RX <- TX del GPS (OBLIGATORIO)
#define PIN_GPS_PWR    -1
#define GPS_BAUD       115200

// --- I2C (baro/mag/iridium)
#define PIN_I2C_SDA    21
#define PIN_I2C_SCL    22

// --- SD (HSPI)
#define PIN_SD_MOSI    13
#define PIN_SD_MISO    12
#define PIN_SD_SCK     14
#define PIN_SD_CS      15

// --- Botones (pull-up internos)
#define PIN_BTN1       25
#define PIN_BTN2       27
#define PIN_BTN3       35  // si usas GPIO35 como entrada analógica/solo input, ajusta a tu wiring

// --- Dirección I2C del Qwiic Iridium 9603N
#ifndef IRIDIUM_I2C_ADDR
  #define IRIDIUM_I2C_ADDR 0x63
#endif
