#include <Arduino.h>
#include "../core/log.h"
#include "modem_a7670.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MODEM"

bool ModemA7670::begin(HardwareSerial& uart, int8_t pinPwr, int pinRx, int pinTx, uint32_t baud) {
  _uart = &uart;
  _pwrPin = pinPwr;

  pinMode(_pwrPin, OUTPUT);
  digitalWrite(_pwrPin, LOW);     // reposo

  // UART del módem
  _uart->begin(baud, SERIAL_8N1, pinRx, pinTx);
  delay(50);

  LOGI("UART listo @%lu, RX=%d TX=%d PWR=%d", (unsigned long)baud, pinRx, pinTx, _pwrPin);
  return true;
}

void ModemA7670::powerPulse(uint16_t pressMs, uint16_t settleMs) {
  // Secuencia típica para "presionar" el botón PWRKEY del A7670
  digitalWrite(_pwrPin, HIGH);
  delay(pressMs);
  digitalWrite(_pwrPin, LOW);
  delay(settleMs);
  LOGI("Power pulse done (%ums + %ums)", pressMs, settleMs);
}

String ModemA7670::readUntil(uint32_t timeoutMs) {
  uint32_t start = millis();
  String out;
  while (millis() - start < timeoutMs) {
    while (_uart->available()) {
      char c = _uart->read();
      if (c == '\r') continue;
      out += c;
    }
    if (out.endsWith("OK\n") || out.indexOf("ERROR") >= 0) break;
    delay(10);
  }
  return out;
}

void ModemA7670::flushInput() {
  while (_uart->available()) _uart->read();
}

bool ModemA7670::ping(uint32_t timeoutMs) {
  if (!_uart) return false;
  flushInput();
  _uart->print("AT\r");
  String resp = readUntil(timeoutMs);
  // Opcional: log detallado
  LOGI("AT resp: %s", resp.c_str());
  return resp.indexOf("OK") >= 0;
}
