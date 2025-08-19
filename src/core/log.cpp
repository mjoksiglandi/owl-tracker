#include <Arduino.h>
#include <stdarg.h>
#include "log.h"

// printf básico con buffer local
void log_printf_(char level, const char* tag, const char* fmt, ...) {
  static char buf[256];

  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  // Asegúrate de haber hecho Serial.begin() en setup()
  Serial.printf("[%c] %s: %s\n", level, tag, buf);
}
