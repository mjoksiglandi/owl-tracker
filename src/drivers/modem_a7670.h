#pragma once
#include <Arduino.h>

class ModemA7670 {
public:
  // Inicializa UART y pin de power (NO enciende el módem todavía)
  bool begin(HardwareSerial& uart, int8_t pinPwr, int pinRx, int pinTx, uint32_t baud);

  // Pulso de encendido típico del A7670 (power key)
  void powerPulse(uint16_t pressMs = 1200, uint16_t settleMs = 2000);

  // Envía "AT" y espera "OK" (timeout en ms). Devuelve true si responde.
  bool ping(uint32_t timeoutMs = 2000);

private:
  HardwareSerial* _uart = nullptr;
  int8_t _pwrPin = -1;

  // Lee la UART hasta timeout. Devuelve el texto recibido (sin \r\n finales)
  String readUntil(uint32_t timeoutMs);
  // Limpia el buffer de entrada de la UART
  void flushInput();
};
