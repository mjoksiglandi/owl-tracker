#pragma once
//
// LILYGO T-A7670G R2 (ESP32-WROVER-E + SIMCom A7670G)
// Pines seguros: GSM y SD son fijos en la PCB, el resto reasignado sin conflictos.
//

// -----------------------------
//  GSM (SIMCom A7670G) - FIJOS
// -----------------------------
#define PIN_MODEM_TX   26     // ESP32 -> A7670G RX
#define PIN_MODEM_RX   27     // ESP32 <- A7670G TX
#define PIN_MODEM_PWR   4     // PWRKEY
#define PIN_MODEM_EN   12     // EN (alto = encendido)

// -----------------------------
//  SD (HSPI) - FIJOS
// -----------------------------
#define PIN_SD_MOSI    15
#define PIN_SD_MISO     2
#define PIN_SD_SCK     14
#define PIN_SD_CS      13

// -----------------------------
//  OLED SSD1322 (SPI VSPI)
//  Usa bus VSPI (SCK=18, MOSI=23)
//  Reset en GPIO33 (antes GPS_TX, ahora libre)
// -----------------------------
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

// -----------------------------
//  I2C (MAG / Iridium Qwiic)
// -----------------------------
#define PIN_I2C_SDA    21
#define PIN_I2C_SCL    22

#ifndef IRIDIUM_I2C_ADDR
  #define IRIDIUM_I2C_ADDR 0x63
#endif

// -----------------------------
//  GPS externo (UART2 / NMEA)
//  Solo RX se usa. TX queda deshabilitado.
// -----------------------------
#define PIN_GPS_TX     -1     // No usado
#define PIN_GPS_RX     34     // ESP32 RX <- TX del GPS
#define GPS_BAUD       115200
#define PIN_GPS_PWR    -1

// -----------------------------
//  Botonera
// -----------------------------
#define PIN_BTN1       25     // HOME / Menús (INPUT_PULLUP interno OK)
#define PIN_BTN2       34     // requiere pull-up externo
#define PIN_BTN3       35     // requiere pull-up externo
#define PIN_BTN4       39     // requiere pull-up externo