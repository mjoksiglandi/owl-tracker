#pragma once
#include <Arduino.h>

// Inicializa el módulo Iridium en el bus I2C
bool iridium_begin(uint8_t addr = 0x43);

// Actualiza el estado (llamado en loop si se necesita)
void iridium_poll();

// Devuelve el último nivel de señal leído (0..5)
int iridium_getSignalQuality();

// Devuelve si el módulo está presente
bool iridium_isPresent();
