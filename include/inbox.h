#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Bandeja de entrada simple (ring buffer) thread-safe
namespace inbox {

struct Msg {
  uint32_t ts_ms;       // millis() al recibir
  char     source[8];   // "GSM","IR","BLE","DBG"
  String   body;        // payload (texto/JSON corto)
  bool     unread;      // flag de no leído
};

// API
void begin(uint8_t capacity = 16);          // crea ring buffer/mutex
void clear();                                // borra todo

// Encolado (thread-safe). Devuelve true si se encoló.
bool push(const char* source, const String& body);

// Métricas
uint16_t size();                             // total (<=cap)
uint16_t unread_count();                     // no leídos

// Lectura “conveniente”
bool      peek_last(Msg& out);               // último mensaje (sin alterar flags)
String    last_body();                        // cuerpo del último ("" si vacío)

// Marcado de leído
void mark_all_read();
void mark_last_read();

// Debug
void dump_to_serial();                        // imprime últimos N

} // namespace inbox
