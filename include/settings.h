#pragma once
#include <Arduino.h>

enum class LinkPref : uint8_t {
  AUTO = 0,
  GSM_ONLY,
  IR_ONLY,
  BOTH
};

bool settings_begin();                 // llama en setup()
LinkPref settings_get_link_pref();
void settings_set_link_pref(LinkPref p);
