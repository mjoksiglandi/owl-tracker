#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "board_pins.h"
#include "oled_display.h"

// ==== U8g2 en modo PAGINADO (menos RAM) ====
U8G2_SSD1322_NHD_256X64_1_4W_HW_SPI u8g2(
  U8G2_R0, /*cs=*/OLED_PIN_CS, /*dc=*/OLED_PIN_DC, /*reset=*/OLED_PIN_RST
);

static inline int clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

// Render genérico en paginado
template <typename DrawFn>
static void renderPaged(DrawFn&& fn) {
  u8g2.firstPage();
  do { fn(); } while (u8g2.nextPage());
}

// ---------- Iconos mini ----------
static void drawAntennaMini(int x, int baseY){
  u8g2.drawBox(x, baseY-4, 1, 4);
  u8g2.drawBox(x-1, baseY-1, 3, 1);
}
static void drawSatelliteMini(int x, int baseY){
  u8g2.drawCircle(x+4, baseY-4, 4, U8G2_DRAW_ALL);
  u8g2.drawLine(x+2, baseY-4, x+6, baseY-4);
  u8g2.drawLine(x+4, baseY-6, x+4, baseY-2);
}
static void drawGlobeMini(int x, int baseY){
  u8g2.drawCircle(x+4, baseY-4, 4, U8G2_DRAW_ALL);
  u8g2.drawLine(x+1, baseY-4, x+7, baseY-4);
  u8g2.drawLine(x+4, baseY-7, x+4, baseY-1);
  u8g2.drawEllipse(x+4, baseY-4, 3, 2, U8G2_DRAW_ALL);
}
// barras mini 0..4
static void drawBarsMini(int x, int baseY, uint8_t level){
  const uint8_t maxBars = 4;
  level = (level > maxBars) ? maxBars : level;
  for (uint8_t i=0;i<maxBars;i++){
    int h  = 1 + i;                // 1..4 px
    int yy = baseY - h;
    int xx = x + i*(1+1);
    if (i < level) u8g2.drawBox(xx, yy, 1, h);
    else           u8g2.drawFrame(xx, yy, 1, h);
  }
}

// ---------- Mapas / Strings ----------
static uint8_t csq_to_bars4(int csq){
  if (csq<0 || csq==99) return 0;
  if (csq<=6)  return 1;
  if (csq<=12) return 2;
  if (csq<=20) return 3;
  return 4;
}
static const char* pdop_code(float p){
  if (!(p >= 0)) return "--";
  if (p < 1.5f)  return "EXC";
  if (p < 4.0f)  return "GUD";
  if (p < 6.0f)  return "MOD";
  if (p < 10.0f) return "ACC";
  return "POR";
}
static String fmtCoord(double v, bool isLat){
  if (isnan(v)) return isLat? "Lat: --.--" : "Lon: --.--";
  char buf[28];
  snprintf(buf, sizeof(buf), "%s%.6f",
           (isLat?(v<0?"S ":"N "):(v<0?"W ":"E ")), fabs(v));
  return String(buf);
}
static String fmtAlt(double a){
  if (isnan(a)) return "Alt: -- m";
  char buf[24]; snprintf(buf, sizeof(buf), "Alt: %.1f m", a);
  return String(buf);
}
static String fmtMsgs(uint32_t n){
  char buf[24]; snprintf(buf, sizeof(buf), "MSG %lu", (unsigned long)n);
  return String(buf);
}
static String fmtUtc(const String& s){
  return s.length() ? s : String("--:--:--Z");
}

// ---------- Init & splash ----------
bool oled_init(){
  SPI.begin(OLED_PIN_SCK, /*MISO*/-1, OLED_PIN_MOSI, OLED_PIN_CS);
  pinMode(OLED_PIN_RST, OUTPUT);
  digitalWrite(OLED_PIN_RST, LOW);  delay(10);
  digitalWrite(OLED_PIN_RST, HIGH); delay(10);
  // (Opcional) mayor velocidad SPI si el cableado lo permite:
  SPI.setFrequency(26000000);
  u8g2.begin();
  u8g2.setContrast(200);
  return true;
}

void oled_splash(const char* title){
  renderPaged([&](){
    u8g2.setFont(u8g2_font_profont29_mf);
    int w = u8g2.getStrWidth(title);
    u8g2.drawStr((256 - w)/2, 36, title);
  });
}

// ---------- HOME ----------
void oled_draw_dashboard(const OwlUiData& d){
  renderPaged([&](){
    // separadores
    u8g2.drawHLine(0, 16, 256);
    u8g2.drawHLine(0, 48, 256);

    // TOP
    u8g2.setFont(u8g2_font_5x8_tf);
    const int baseY = 14;
    int xCEL = 6, xGPS = 72, xIR = 140;

    drawAntennaMini(xCEL, baseY);
    drawBarsMini(xCEL+7, baseY, clampi(csq_to_bars4(d.csq),0,4));
    u8g2.drawStr(xCEL+7 + 4*(1+1) + 4, baseY, "CEL");

    drawSatelliteMini(xGPS, baseY);
    char gpsBuf[12];
    if (d.sats >= 0) snprintf(gpsBuf, sizeof(gpsBuf), "GPS %02d", d.sats);
    else             snprintf(gpsBuf, sizeof(gpsBuf), "GPS --");
    u8g2.drawStr(xGPS + 12, baseY, gpsBuf);

    drawGlobeMini(xIR, baseY);
    if (d.iridiumLvl < 0) u8g2.drawStr(xIR + 12, baseY, "IR --");
    else { drawBarsMini(xIR + 12, baseY, clampi(d.iridiumLvl,0,4));
           u8g2.drawStr(xIR + 12 + 4*(1+1) + 3, baseY, "IR"); }

    String sMsgTop = fmtMsgs(d.msgRx);
    int wMsgTop = u8g2.getStrWidth(sMsgTop.c_str());
    u8g2.drawStr(256 - 6 - wMsgTop, baseY, sMsgTop.c_str());

    // MIDDLE
    u8g2.setFont(u8g2_font_6x12_tf);
    String sLat = fmtCoord(d.lat, true);
    String sLon = fmtCoord(d.lon, false);
    String sAlt = fmtAlt(d.alt);
    u8g2.drawStr(6, 34,  sLat.c_str());
    u8g2.drawStr(92, 34, sLon.c_str());
    int wAlt = u8g2.getStrWidth(sAlt.c_str());
    u8g2.drawStr(256 - 6 - wAlt, 34, sAlt.c_str());

    // BOTTOM
    char precBuf[16]; snprintf(precBuf, sizeof(precBuf), "PREC: %s", pdop_code(d.pdop));
    u8g2.drawStr(6, 62, precBuf);
    String sUtc = fmtUtc(d.utc);
    int wUtc = u8g2.getStrWidth(sUtc.c_str());
    u8g2.drawStr((256 - wUtc)/2, 62, sUtc.c_str());
  });
}

// ---------- Stubs simples para otras pantallas ----------
void oled_draw_gps_detail(const OwlUiData& d){
  renderPaged([&](){
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(6, 14, "GPS DETAIL");
    char ln[40]; snprintf(ln, sizeof(ln), "Sats:%d  PDOP:%.1f", d.sats, d.pdop);
    u8g2.drawStr(6, 30, ln);
    String sLat = String("Lat: ") + (isnan(d.lat)?"--":String(d.lat,6));
    String sLon = String("Lon: ") + (isnan(d.lon)?"--":String(d.lon,6));
    String sAlt = String("Alt: ") + (isnan(d.alt)?"--":String(d.alt,1)) + " m";
    u8g2.drawStr(6, 46, sLat.c_str());
    u8g2.drawStr(6, 58, sLon.c_str());
    int wAlt = u8g2.getStrWidth(sAlt.c_str());
    u8g2.drawStr(256 - 6 - wAlt, 58, sAlt.c_str());
  });
}

void oled_draw_iridium_detail(bool present, int sigLevel, int unread, const String& imei){
  renderPaged([&](){
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(6, 14, "IRIDIUM DETAIL");
    u8g2.drawStr(6, 30, present ? "Module: PRESENT" : "Module: --");
    char ln[40]; snprintf(ln, sizeof(ln), "Sig:%d  Unread:%d", sigLevel, unread);
    u8g2.drawStr(6, 46, ln);
    u8g2.drawStr(6, 62, ("IMEI: " + imei).c_str());
  });
}

void oled_draw_sys_config(const OwlUiData& d, bool netReg, bool pdpUp,
                          const String& ip, bool sdOk, bool i2cOk, const char* fw){
  renderPaged([&](){
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(6, 14, "SYS CONFIG");
    char ln1[48]; snprintf(ln1, sizeof(ln1), "Net:%s  PDP:%s", netReg?"REG":"NO", pdpUp?"UP":"DOWN");
    u8g2.drawStr(6, 30, ln1);
    u8g2.drawStr(6, 44, ("IP: " + ip).c_str());
    char ln2[48]; snprintf(ln2, sizeof(ln2), "SD:%s  I2C:%s  FW:%s", sdOk?"OK":"NO", i2cOk?"OK":"NO", fw);
    u8g2.drawStr(6, 58, ln2);
  });
}

void oled_draw_messages(uint16_t unread, const String& last){
  renderPaged([&](){
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(6, 14, "MESSAGES");
    char ln[32]; snprintf(ln, sizeof(ln), "Unread: %u", unread);
    u8g2.drawStr(6, 30, ln);
    u8g2.drawStr(6, 48, "Last:");
    int w = u8g2.getStrWidth("Last:");
    u8g2.drawStr(6 + w + 6, 48, last.c_str());
  });
}
