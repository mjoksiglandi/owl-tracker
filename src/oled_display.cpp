#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "board_pins.h"
#include "oled_display.h"

// HW SPI
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(
  U8G2_R0, /*cs=*/OLED_PIN_CS, /*dc=*/OLED_PIN_DC, /*reset=*/OLED_PIN_RST
);

static inline int clamp(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

// ---------------- Iconos (versión compacta) ----------------
// Altura total ~10 px; pensados para y=12..14 (status bar)
static void drawAntennaSmall(int x, int y){
  // mástil 4 px + base 4 px ancho
  u8g2.drawBox(x,   y-5, 1, 5);  // mástil
  u8g2.drawBox(x-2, y-1, 5, 1);  // base
}
static void drawBarsSmall(int x, int y, uint8_t level, uint8_t maxBars){
  // barras de 1..4 px de alto, paso 1 px
  for (uint8_t i=0;i<maxBars;i++){
    int h = 1 + i;                 // 1,2,3,4 px
    int yy = y - h;
    int xx = x + i*3;              // 2 px ancho + 1 px espacio
    if (i < level) u8g2.drawBox(xx, yy, 2, h);
    else           u8g2.drawFrame(xx, yy, 2, h);
  }
}
static void drawSatelliteSmall(int x, int y){
  // círculo 4 px radio + cruces
  u8g2.drawCircle(x+4, y-4, 4, U8G2_DRAW_ALL);
  u8g2.drawLine(x+1, y-4, x+7, y-4);
  u8g2.drawLine(x+4, y-7, x+4, y-1);
}
static void drawGlobeSmall(int x, int y){
  u8g2.drawCircle(x+4, y-4, 4, U8G2_DRAW_ALL);
  u8g2.drawLine(x+1, y-4, x+7, y-4);
  u8g2.drawLine(x+4, y-7, x+4, y-1);
  u8g2.drawEllipse(x+4, y-4, 3, 2, U8G2_DRAW_ALL);
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

// ---------------- Strings ----------------
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
static String fmtMsgs(uint32_t n){
  char buf[24]; snprintf(buf, sizeof(buf), "MSG RX: %lu", (unsigned long)n);
  return String(buf);
}
static String fmtUtc(const String& s){
  return s.length() ? s : String("--:--:--Z");
}

// ---------------- PDOP -> letra (rango) ----------------
static char pdopGrade(float pdop) {
  if (pdop < 0)      return '-';  // sin datos
  if (pdop < 1.5f)   return 'E';  // Excelente
  if (pdop < 4.0f)   return 'B';  // Muy bueno / Suficiente
  if (pdop < 6.0f)   return 'M';  // Bueno / Moderado
  if (pdop < 10.0f)  return 'A';  // Moderado / Aceptable
  return 'D';                      // Deficiente
}

// ---------------- Init ----------------
bool oled_init(){
  SPI.begin(OLED_PIN_SCK, /*MISO*/-1, OLED_PIN_MOSI, OLED_PIN_CS);
  pinMode(OLED_PIN_RST, OUTPUT);
  digitalWrite(OLED_PIN_RST, LOW);  delay(10);
  digitalWrite(OLED_PIN_RST, HIGH); delay(10);

  u8g2.begin();
  u8g2.setContrast(210);
  return true;
}

// ---------------- Dashboard ----------------
// 256x64
// Superior (y=12): CEL | GPS | IR  (iconos compactos)
// Centro   (y=34): Lat ...   Lon ...   Alt ...
// Inferior (y=62): PDOP letra (izq)    UTC (centro)    MSG RX (der)
void oled_draw_dashboard(const OwlUiData& d){
  u8g2.clearBuffer();

  // ---------- Superior (compacto) ----------
  int topY = 12;

  // Fuente pequeña para etiquetas
  u8g2.setFont(u8g2_font_5x8_mf);

  // CEL (izquierda)
  drawAntennaSmall(4, topY);
  uint8_t bars = csq_to_bars(d.csq);
  drawBarsSmall(10, topY, clamp(bars,0,5), 5);
  u8g2.drawStr(10 + 5*3 + 3, topY, "CEL"); // 5 barras * 3px + margen

  // GPS (centro)
  int xgps = 100;
  drawSatelliteSmall(xgps, topY);
  char gpsBuf[12];
  if (d.sats >= 0) snprintf(gpsBuf, sizeof(gpsBuf), "GPS %02d", d.sats);
  else             snprintf(gpsBuf, sizeof(gpsBuf), "GPS --");
  u8g2.drawStr(xgps + 12, topY, gpsBuf);

  // IR (derecha)
  int xir = 188;
  drawGlobeSmall(xir, topY);
  if (d.iridiumLvl < 0) {
    u8g2.drawStr(xir + 12, topY, "IR --");
  } else {
    uint8_t irBars = clamp(d.iridiumLvl, 0, 5);
    drawBarsSmall(xir + 12, topY, irBars, 5);
    u8g2.drawStr(xir + 12 + 5*3 + 3, topY, "IR");
  }

  // ---------- Centro ----------
  u8g2.setFont(u8g2_font_6x12_tf);
  String sLat = fmtCoord(d.lat, true);
  String sLon = fmtCoord(d.lon, false);
  String sAlt = fmtAlt(d.alt);

  u8g2.drawStr(6, 34,  sLat.c_str());
  u8g2.drawStr(90, 34, sLon.c_str());
  int wAlt = u8g2.getStrWidth(sAlt.c_str());
  u8g2.drawStr(256 - 6 - wAlt, 34, sAlt.c_str());

  // ---------- Inferior ----------
  // PDOP como letra
  char grade = pdopGrade(d.pdop);
  char pdopStr[12];
  snprintf(pdopStr, sizeof(pdopStr), "PDOP: %c", grade);

  String sMsg = fmtMsgs(d.msgRx);
  String sUtc = fmtUtc(d.utc);

  // PDOP izquierda
  u8g2.drawStr(6, 62, pdopStr);
  // MSG derecha
  int wMsg = u8g2.getStrWidth(sMsg.c_str());
  u8g2.drawStr(256 - 6 - wMsg, 62, sMsg.c_str());
  // UTC centro
  int wUtc = u8g2.getStrWidth(sUtc.c_str());
  int xUtc = (256 - wUtc) / 2;
  u8g2.drawStr(xUtc, 62, sUtc.c_str());

  u8g2.sendBuffer();
}
