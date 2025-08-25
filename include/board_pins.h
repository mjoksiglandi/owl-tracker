#pragma once

// =======================
//  LILYGO T-A7670G (R2)
// =======================

// --- UART del módem (SIMCom A7670G)  -> UART1 del ESP32
#define PIN_MODEM_TX   26
#define PIN_MODEM_RX   27
#define PIN_MODEM_PWR   4
#define PIN_MODEM_EN   12

// --- OLED SSD1322 256x64 SPI (VSPI)
#define PIN_OLED_SCK   18
#define PIN_OLED_MOSI  23
#define PIN_OLED_CS     5
#define PIN_OLED_DC    32   // <— movido (libera pines I2C)
#define PIN_OLED_RST   33   // <— movido (libera pines I2C)

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

// --- GPS externo (UART2 del ESP32)
#define PIN_GPS_TX     25    // ESP32 TX -> RX del GPS (opcional)
#define PIN_GPS_RX     34    // ESP32 RX <- TX del GPS (obligatorio)
#define PIN_GPS_PWR    -1
#define GPS_BAUD       115200

// --- I2C bus (MS5611 baro + IST8310 mag + Iridium I2C futuro)
#define PIN_I2C_SDA    21
#define PIN_I2C_SCL    22
#define I2C_FREQ_HZ    400000

// --- SD card (HSPI dedicado)
#define PIN_SD_MISO     2
#define PIN_SD_MOSI    15
#define PIN_SD_SCK     14
#define PIN_SD_CS      13

// --- BOTONES (input-only, requieren pull-up externo, activos en LOW)
#define PIN_BTN1   39   // Home / Menu (corto/largo)
#define PIN_BTN2   36   // Mensajes / POI (corto/largo)
#define PIN_BTN3   35   // SOS (largo 2s)
