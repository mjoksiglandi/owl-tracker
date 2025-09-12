#pragma once
#include <Arduino.h>

struct InboxItem {
  uint32_t ts;
  String   src;   // "GSM" / "IR" / "BLE"
  String   data;  // el mensaje (cifrado gcm://... o texto)
};

void      inbox_clear();
void      inbox_push(const char* src, const String& data);
uint16_t  inbox_count();
bool      inbox_get(uint16_t idx, InboxItem& out);   // 0 = más reciente
