#pragma once
#include <Arduino.h>

// forward-declare si no incluyes settings.h aquí
enum class LinkPref : uint8_t;

struct OwlUiData {
  int    csq = 99;
  int    iridiumLvl = -1;
  int    sats = -1;
  float  pdop = -1.0f;
  double lat  = NAN;
  double lon  = NAN;
  double alt  = NAN;
  uint32_t msgRx = 0;
  String utc = "";
  float  speed_mps   = NAN;
  float  course_deg  = NAN;
  float  pressure_hpa= NAN;
};

bool oled_init();
void oled_splash(const char* title);

// === CAMBIO AQUÍ: añadimos ratLabel ===
void oled_draw_dashboard(const OwlUiData& d, const char* ratLabel);

void oled_draw_gps_detail(const OwlUiData& d);
void oled_draw_iridium_detail(bool present, int sigLevel, int unread, const String& imei);
void oled_draw_sys_config(const OwlUiData& d, bool netReg, bool pdpUp,
                          const String& ip, bool sdOk, bool i2cOk, const char* fw);
void oled_draw_messages(uint16_t unread, const String& last);

// NUEVAS ya agregadas anteriormente:
void oled_draw_gsm_detail(const OwlUiData& d, const String& imei, bool netReg, bool pdpUp, int rssiDbm);
void oled_draw_ble_detail(bool connected, const String& devName);

// === Pantalla Testing existente ===
void oled_draw_testing(LinkPref pref, const char* lastResult, bool busy);
