#pragma once
#include <Arduino.h>

// Dibuja pantalla de detalle de Iridium 256x64
// present:     módulo detectado por I2C
// sigLevel:    0..5 (barras). -1 => sin dato
// unread:      mensajes MT pendientes
// imei:        IMEI si está disponible
void oled_draw_iridium_detail(bool present, int sigLevel, int unread, const String& imei);
