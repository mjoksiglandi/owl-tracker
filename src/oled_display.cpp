// src/oled_display.cpp
#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "board_pins.h"
#include "oled_display.h"

// Controlador OLED (SSD1322 256x64) por HW SPI
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(
  U8G2_R0, /*cs=*/OLED_PIN_CS, /*dc=*/OLED_PIN_DC, /*reset=*/OLED_PIN_RST
);

// Helpers
static inline int  clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }
static inline bool isfinitef(float v){ return !isnan(v) && !isinf(v); }

// ---------------- Iconos (compactos) ----------------
static void drawAntennaXS(int x, int y){
  u8g2.drawBox(x,   y-3, 1, 3);
  u8g2.drawBox(x-1, y-1, 3, 1);
}
static void drawBarsXS(int x, int y, uint8_t level, uint8_t maxBars){
  for (uint8_t i=0;i<maxBars;i++){
    int h = 1 + i;               // 1..5
    int yy = y - h;
    int xx = x + i*2;            // 1px ancho + 1px gap
    if (i < level) u8g2.drawBox(xx, yy, 1, h);
    else           u8g2.drawFrame(xx, yy, 1, h);
  }
}
static void drawSatelliteXS(int x, int y){ u8g2.drawCircle(x+2, y-2, 2, U8G2_DRAW_ALL); }
static void drawGlobeXS(int x, int y){ u8g2.drawCircle(x+2, y-2, 2, U8G2_DRAW_ALL); u8g2.drawPixel(x+2, y-2); }
static void drawMsgXS(int x, int y){
  u8g2.drawFrame(x, y-4, 6, 4);
  u8g2.drawLine(x,   y-4, x+3, y-1);
  u8g2.drawLine(x+6, y-4, x+3, y-1);
}

// ---------------- CSQ->barras (0..5) ----------------
static uint8_t csq_to_bars(int csq){
  if (csq<0 || csq==99) return 0;
  if (csq<=3)  return 1;
  if (csq<=9)  return 2;
  if (csq<=15) return 3;
  if (csq<=22) return 4;
  return 5;
}

// ---------------- Formatters ----------------
static String fmtCoord(double v, bool isLat){
  if (isnan(v)) return isLat? "Lat: --.--" : "Lon: --.--";
  char buf[28];
  snprintf(buf, sizeof(buf), "%s%.6f", (isLat?(v<0?"S ":"N "):(v<0?"W ":"E ")), fabs(v));
  return String(buf);
}
static String fmtAlt(double a){
  if (isnan(a)) return "Alt: -- m";
  char buf[24];
  snprintf(buf, sizeof(buf), "Alt: %.1f m", a);
  return String(buf);
}
static String fmtUtc(const String& s){ return s.length() ? s : String("--:--:--Z"); }
static String fmtSpeed(float mps){
  if (!isfinitef(mps)) return "SPD: --.-";
  char b[24]; snprintf(b, sizeof(b), "SPD: %.1f m/s", mps);
  return String(b);
}
static String fmtTrackDeg(float deg){
  if (!isfinitef(deg)) return "TRK: ---";
  while (deg < 0)   deg += 360.f;
  while (deg >=360) deg -= 360.f;
  char b[20]; snprintf(b, sizeof(b), "TRK: %03d°", (int)lroundf(deg));
  return String(b);
}
static const char* cardinalFromDeg(float deg){
  if (!isfinitef(deg)) return "--";
  while (deg < 0)   deg += 360.f;
  while (deg >=360) deg -= 360.f;
  // 8 vientos (45° por sector)
  static const char* C[8] = {"N","NE","E","SE","S","SW","W","NW"};
  int idx = (int)floorf((deg + 22.5f)/45.0f) & 7;
  return C[idx];
}

// ---------------- Init/Splash ----------------
bool oled_init(){
  SPI.begin(OLED_PIN_SCK, /*MISO*/-1, OLED_PIN_MOSI, OLED_PIN_CS);
  pinMode(OLED_PIN_RST, OUTPUT);
  digitalWrite(OLED_PIN_RST, LOW);  delay(10);
  digitalWrite(OLED_PIN_RST, HIGH); delay(10);
  u8g2.begin();
  u8g2.setContrast(210);
  return true;
}
void oled_splash(const char* title){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso18_tf);
  int w = u8g2.getStrWidth(title);
  u8g2.drawStr((256-w)/2, 36, title);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(86, 56, "booting...");
  u8g2.sendBuffer();
}

// ---------------- Barra superior compacta (SOLO HOME) --------
static void draw_top_bar_compact(int csq, int sats, int iriLvl, uint32_t msgRx){
  const int topY = 10;
  u8g2.setFont(u8g2_font_5x8_tf);

  // CEL
  drawAntennaXS(4, topY);
  drawBarsXS(8, topY, csq_to_bars(csq), 5);

  // GPS
  drawSatelliteXS(54, topY);
  char gbuf[10];
  if (sats >= 0) snprintf(gbuf, sizeof(gbuf), "G:%02d", sats);
  else           snprintf(gbuf, sizeof(gbuf), "G:--");
  u8g2.drawStr(60, topY, gbuf);

  // IR
  drawGlobeXS(104, topY);
  char ibuf[10];
  if (iriLvl >= 0) snprintf(ibuf, sizeof(ibuf), "IR:%d", clampi(iriLvl,0,5));
  else             snprintf(ibuf, sizeof(ibuf), "IR:--");
  u8g2.drawStr(110, topY, ibuf);

  // MSG
  drawMsgXS(200, topY);
  char mbuf[16]; snprintf(mbuf, sizeof(mbuf), "M:%lu", (unsigned long)msgRx);
  u8g2.drawStr(208, topY, mbuf);

  // línea separadora
  u8g2.drawHLine(0, 14, 256);
}

// ---------------- HOME ----------------
// SPD debajo de ALT (lado derecho). UTC centrado abajo.
// Barra superior compacta solo aquí.
void oled_draw_dashboard(const OwlUiData& d){
  u8g2.clearBuffer();

  // TOP (compact)
  draw_top_bar_compact(d.csq, d.sats, d.iridiumLvl, d.msgRx);

  // MIDDLE
  u8g2.setFont(u8g2_font_6x12_tf);
  String sLat = fmtCoord(d.lat, true);
  String sLon = fmtCoord(d.lon, false);
  String sAlt = fmtAlt(d.alt);
  String sSpd = fmtSpeed(d.speed_mps);

  // Lat/Lon a la izquierda
  u8g2.drawStr(6, 30,  sLat.c_str());
  u8g2.drawStr(6, 42,  sLon.c_str());

  // ALT a la derecha (fila 30)
  int wAlt = u8g2.getStrWidth(sAlt.c_str());
  u8g2.drawStr(256 - 6 - wAlt, 30, sAlt.c_str());

  // SPD DEBAJO de ALT (fila 42, a la derecha)
  int wSpd = u8g2.getStrWidth(sSpd.c_str());
  u8g2.drawStr(256 - 6 - wSpd, 42, sSpd.c_str());

  // Separador fino encima de la franja inferior
  u8g2.drawHLine(0, 58, 256);

  // BOTTOM: UTC centrado
  String sUtc = fmtUtc(d.utc);
  int wUtc = u8g2.getStrWidth(sUtc.c_str());
  u8g2.drawStr((256 - wUtc)/2, 62, sUtc.c_str());

  u8g2.sendBuffer();
}

// ---------------- GPS DETAIL ----------------
// Sin barra superior. Muestra Lat/Lon/Alt y TRK con cardinal + SPD.
void oled_draw_gps_detail(const OwlUiData& d){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  // Título
  u8g2.drawStr(6, 14, "GPS DETAIL");
  u8g2.drawHLine(0, 18, 256);

  // Posición (izquierda)
  String sLat = fmtCoord(d.lat, true);
  String sLon = fmtCoord(d.lon, false);
  u8g2.drawStr(6, 32, sLat.c_str());
  u8g2.drawStr(6, 46, sLon.c_str());

  // Altura (derecha, misma fila de Lat)
  String sAlt = fmtAlt(d.alt);
  int wAlt = u8g2.getStrWidth(sAlt.c_str());
  u8g2.drawStr(256 - 6 - wAlt, 32, sAlt.c_str());

  // SPD (derecha, fila 46)
  String sSpd = fmtSpeed(d.speed_mps);
  int wSpd = u8g2.getStrWidth(sSpd.c_str());
  u8g2.drawStr(256 - 6 - wSpd, 46, sSpd.c_str());

  // TRK con cardinal (derecha, fila 60)
  String sTrkDeg = fmtTrackDeg(d.course_deg);
  const char* card = cardinalFromDeg(d.course_deg);
  char trkBuf[32];
  if (isfinitef(d.course_deg)) snprintf(trkBuf, sizeof(trkBuf), "%s %s", sTrkDeg.c_str(), card);
  else                         snprintf(trkBuf, sizeof(trkBuf), "TRK: ---  --");
  int wTrk = u8g2.getStrWidth(trkBuf);
  u8g2.drawStr(256 - 6 - wTrk, 60, trkBuf);

  u8g2.sendBuffer();
}

// ---------------- IRIDIUM DETAIL ----------------
// Sin barra superior
void oled_draw_iridium_detail(bool present, int sigLevel, int unread, const String& imei){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(6, 14, "IRIDIUM DETAIL");
  u8g2.drawHLine(0, 18, 256);

  // IMEI (no depende de señal)
  String sImei = imei.length() ? String("IMEI: ") + imei : String("IMEI: --");
  u8g2.drawStr(6, 34, sImei.c_str());

  // Presencia / Señal / Mensajes
  char sPr[20]; snprintf(sPr, sizeof(sPr), "PRESENT: %s", present ? "YES":"NO");
  u8g2.drawStr(6, 48, sPr);

  char sSig[16];
  if (sigLevel >= 0) snprintf(sSig, sizeof(sSig), "SIG: %d/5", clampi(sigLevel,0,5));
  else               snprintf(sSig, sizeof(sSig), "SIG: --");
  int wSig = u8g2.getStrWidth(sSig);
  u8g2.drawStr(256 - 6 - wSig, 48, sSig);

  char sMt[20]; snprintf(sMt, sizeof(sMt), "MT waiting: %d", unread);
  int wMt = u8g2.getStrWidth(sMt);
  u8g2.drawStr(256 - 6 - wMt, 60, sMt);

  u8g2.sendBuffer();
}

// ---------------- SYS CONFIG ----------------
// Sin barra superior
void oled_draw_sys_config(const OwlUiData& d, bool netReg, bool pdpUp,
                          const String& ip, bool sdOk, bool i2cOk, const char* fw){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(6, 14, "SYS CONFIG");
  u8g2.drawHLine(0, 18, 256);

  // NET / PDP
  char l1[48];
  snprintf(l1, sizeof(l1), "NET:%s  PDP:%s", netReg?"ON":"OFF", pdpUp?"UP":"DOWN");
  u8g2.drawStr(6, 32, l1);

  // IP
  String ipStr = String("IP: ") + (ip.length()? ip : String("--"));
  u8g2.drawStr(6, 46, ipStr.c_str());

  // SD / I2C
  char l3[48];
  snprintf(l3, sizeof(l3), "SD:%s  I2C:%s", sdOk?"OK":"NO", i2cOk?"OK":"NO");
  int wL3 = u8g2.getStrWidth(l3);
  u8g2.drawStr(256 - 6 - wL3, 46, l3);

  // FW/Notas (ej: “fw 1.0.0 BLE:OK”)
  if (fw && *fw) {
    int w = u8g2.getStrWidth(fw);
    u8g2.drawStr(256 - 6 - w, 32, fw);
  }

  u8g2.sendBuffer();
}

// ---------------- MESSAGES ----------------
// Sin barra superior
void oled_draw_messages(uint16_t unread, const String& last){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(6, 14, "MESSAGES");
  u8g2.drawHLine(0, 18, 256);

  char mBuf[24]; snprintf(mBuf, sizeof(mBuf), "UNREAD: %u", unread);
  u8g2.drawStr(6, 32, mBuf);

  String s = last.length()? last : String("(no messages)");
  int w = u8g2.getStrWidth(s.c_str());
  if (w > 244) {
    String cut = s.substring(0, 30) + "...";
    u8g2.drawStr(6, 46, cut.c_str());
  } else {
    u8g2.drawStr(6, 46, s.c_str());
  }

  u8g2.sendBuffer();
}
