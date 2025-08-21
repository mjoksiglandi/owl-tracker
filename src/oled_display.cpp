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

// ---------------- Iconos ----------------
static void drawAntenna(int x, int y){
  u8g2.drawBox(x,   y-6, 2, 6);
  u8g2.drawBox(x-2, y-2, 6, 2);
}
static void drawBars(int x, int y, uint8_t level, uint8_t maxBars, uint8_t step=2){
  for (uint8_t i=0;i<maxBars;i++){
    int h = 2 + i*2;
    int yy = y - h;
    int xx = x + i*(step+2);
    if (i < level) u8g2.drawBox(xx, yy, step, h);
    else           u8g2.drawFrame(xx, yy, step, h);
  }
}
static void drawSatellite(int x, int y){
  u8g2.drawCircle(x+6, y-6, 6, U8G2_DRAW_ALL);
  u8g2.drawLine(x+2,  y-6, x+10, y-6);
  u8g2.drawLine(x+6,  y-10, x+6,  y-2);
}
static void drawGlobe(int x, int y){
  u8g2.drawCircle(x+6, y-6, 6, U8G2_DRAW_ALL);
  u8g2.drawLine(x+2, y-6, x+10, y-6);
  u8g2.drawLine(x+6, y-11, x+6, y-1);
  u8g2.drawEllipse(x+6, y-6, 5, 3, U8G2_DRAW_ALL);
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
static String fmtPdop(float p){
  if (p<0) return "PDOP: --";
  char buf[20]; snprintf(buf, sizeof(buf), "PDOP: %.1f", p);
  return String(buf);
}
static String fmtMsgs(uint32_t n){
  char buf[24]; snprintf(buf, sizeof(buf), "MSG RX: %lu", (unsigned long)n);
  return String(buf);
}
static String fmtUtc(const String& s){
  return s.length() ? s : String("--:--:--Z");
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
// Superior (y=14): CEL | GPS | IR
// Centro   (y=34): Lat ...   Lon ...   Alt ...
// Inferior (y=62): PDOP (izq)        UTC (centro)        MSG RX (der)
void oled_draw_dashboard(const OwlUiData& d){
  u8g2.clearBuffer();

  // ---------- Superior ----------
  int topY = 14;

  // CEL
  drawAntenna(6, topY);
  uint8_t bars = csq_to_bars(d.csq);
  drawBars(16, topY, clamp(bars,0,5), 5, 2);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(16 + 5*(2+2) + 4, topY, "CEL");

  // GPS (centro)
  int xgps = 100;
  drawSatellite(xgps, topY);
  char gpsBuf[20];
  if (d.sats >= 0) snprintf(gpsBuf, sizeof(gpsBuf), "GPS %02d", d.sats);
  else             snprintf(gpsBuf, sizeof(gpsBuf), "GPS --");
  u8g2.drawStr(xgps + 16, topY, gpsBuf);

  // IR (derecha)
  int xir = 196;
  drawGlobe(xir, topY);
  u8g2.setFont(u8g2_font_6x12_tf);
  if (d.iridiumLvl < 0) {
    // sin dato: NO barras, solo texto "IR --"
    u8g2.drawStr(xir + 14, topY, "IR --");
  } else {
    uint8_t irBars = clamp(d.iridiumLvl, 0, 5);
    drawBars(xir + 14, topY, irBars, 5, 2);
    u8g2.drawStr(xir + 14 + 5*(2+2) + 4, topY, "IR");
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
  String sPdop = fmtPdop(d.pdop);
  String sMsg  = fmtMsgs(d.msgRx);
  String sUtc  = fmtUtc(d.utc);

  // PDOP izquierda
  u8g2.drawStr(6, 62, sPdop.c_str());
  // MSG derecha
  int wMsg = u8g2.getStrWidth(sMsg.c_str());
  u8g2.drawStr(256 - 6 - wMsg, 62, sMsg.c_str());
  // UTC centro
  int wUtc = u8g2.getStrWidth(sUtc.c_str());
  int xUtc = (256 - wUtc) / 2;
  u8g2.drawStr(xUtc, 62, sUtc.c_str());

  u8g2.sendBuffer();
}
