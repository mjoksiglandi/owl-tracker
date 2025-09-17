#include "settings.h"

static LinkPref g_link_pref = LinkPref::AUTO;

#if defined(ARDUINO_ARCH_ESP32)
  #include <Preferences.h>
  static Preferences prefs;
  static const char* NS = "owl";
  static const char* KEY = "linkpref";
#endif

bool settings_begin(){
#if defined(ARDUINO_ARCH_ESP32)
  if (!prefs.begin(NS, /*readOnly=*/false)) return false;
  uint8_t v = prefs.getUChar(KEY, static_cast<uint8_t>(LinkPref::AUTO));
  if (v > static_cast<uint8_t>(LinkPref::BOTH)) v = static_cast<uint8_t>(LinkPref::AUTO);
  g_link_pref = static_cast<LinkPref>(v);
  return true;
#else
  return true; // RAM only
#endif
}

LinkPref settings_get_link_pref(){
  return g_link_pref;
}

void settings_set_link_pref(LinkPref p){
  g_link_pref = p;
#if defined(ARDUINO_ARCH_ESP32)
  prefs.putUChar(KEY, static_cast<uint8_t>(p));
#endif
}
