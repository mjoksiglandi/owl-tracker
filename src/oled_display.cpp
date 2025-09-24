#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "board_pins.h"
#include "oled_display.h"
#include "settings.h"   // para LinkPref en oled_draw_testing

// HW SPI SSD1322 256x64
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(
  U8G2_R0, /*cs=*/OLED_PIN_CS, /*dc=*/OLED_PIN_DC, /*reset=*/OLED_PIN_RST
);

static inline int clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

// ---------------- CSQ->barras (0..5) ----------------
static uint8_t csq_to_bars(int csq){
  if (csq<0 || csq==99) return 0;
  if (csq<=3)  return 1;
  if (csq<=9)  return 2;
  if (csq<=15) return 3;
  if (csq<=22) return 4;
  return 5;
}

// ---------------- Strings helpers ----------------
static String fmtCoord(double v, bool isLat){
  if (isnan(v)) return isLat? "Lat: --.--" : "Lon: --.--";
  char buf[32];
  snprintf(buf, sizeof(buf), "%s%.6f", (isLat?(v<0?"S ":"N "):(v<0?"W ":"E ")), fabs(v));
  return String(buf);
}
static String fmtAlt(double a){
  if (isnan(a)) return "Alt: -- m";
  char buf[24]; snprintf(buf, sizeof(buf), "Alt: %.1f m", a);
  return String(buf);
}
static String fmtSpeed(float s){
  if (isnan(s)) return "Spd: --.- m/s";
  char buf[24]; snprintf(buf, sizeof(buf), "Spd: %.1f m/s", s);
  return String(buf);
}
static String fmtPdop(float p){
  if (p<0) return "PDOP: --";
  char buf[20]; snprintf(buf, sizeof(buf), "PDOP: %.1f", p);
  return String(buf);
}
static String fmtMsgs(uint32_t n){
  char buf[24]; snprintf(buf, sizeof(buf), "MSG: %lu", (unsigned long)n);
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

void oled_splash(const char* title){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  int w = u8g2.getStrWidth(title);
  u8g2.drawStr((256-w)/2, 38, title);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(70, 58, "booting...");
  u8g2.sendBuffer();
}

// ---------------- Iconos mini (10 px alto) ----------------
static void drawAntennaMini(int x, int y){
  u8g2.drawBox(x,   y-7, 2, 7);
  u8g2.drawBox(x-2, y-3, 6, 2);
}
static void drawBarsMini(int x, int y, uint8_t level, uint8_t maxBars, uint8_t w=2, uint8_t step=1){
  for (uint8_t i=0;i<maxBars;i++){
    int h  = 1 + i*2;
    int yy = y - h;
    int xx = x + i*(w+step);
    if (i < level) u8g2.drawBox(xx, yy, w, h);
    else           u8g2.drawFrame(xx, yy, w, h);
  }
}
static void drawSatelliteMini(int x, int y){
  u8g2.drawCircle(x+4, y-5, 4, U8G2_DRAW_ALL);
  u8g2.drawLine(x+1, y-5, x+7, y-5);
  u8g2.drawLine(x+4, y-8, x+4, y-2);
}
static void drawGlobeMini(int x, int y){
  u8g2.drawCircle(x+4, y-5, 4, U8G2_DRAW_ALL);
  u8g2.drawLine(x+1, y-5, x+7, y-5);
  u8g2.drawLine(x+4, y-8, x+4, y-2);
  u8g2.drawEllipse(x+4, y-5, 3, 2, U8G2_DRAW_ALL);
}

// =============================================================
//  HOME / DASHBOARD
// =============================================================
void oled_draw_dashboard(const OwlUiData& d, const char* ratLabel){
  u8g2.clearBuffer();

  // ---------- Top ----------
  const int topY = 11;
  u8g2.drawHLine(0, 15, 256);

  // CEL
  drawAntennaMini(4, topY);
  uint8_t bars = csq_to_bars(d.csq);
  drawBarsMini(12, topY, clampi(bars,0,5), 5, 2, 1);
  u8g2.setFont(u8g2_font_5x8_tf);
  const char* rlab = (ratLabel && ratLabel[0]) ? ratLabel : "--";
  u8g2.drawStr(12 + 5*(2+1) + 4, topY, rlab);   // 2G/3G/4G/5G/--

  // GPS centro top
  int xgps = 102;
  drawSatelliteMini(xgps, topY);
  char gpsBuf[16];
  if (d.sats >= 0) snprintf(gpsBuf, sizeof(gpsBuf), "GPS %02d", d.sats);
  else             snprintf(gpsBuf, sizeof(gpsBuf), "GPS --");
  u8g2.drawStr(xgps + 12, topY, gpsBuf);

  // IR derecha top
  int xir = 190;
  drawGlobeMini(xir, topY);
  if (d.iridiumLvl < 0) {
    u8g2.drawStr(xir + 12, topY, "IR --");
  } else {
    uint8_t irBars = clampi(d.iridiumLvl, 0, 5);
    drawBarsMini(xir + 12, topY, irBars, 5, 2, 1);
    u8g2.drawStr(xir + 12 + 5*(2+1) + 4, topY, "IR");
  }

  // Mensajes (contador en top derecha)
  String sMsgTop = fmtMsgs(d.msgRx);
  int wMsgTop = u8g2.getStrWidth(sMsgTop.c_str());
  u8g2.drawStr(256 - 4 - wMsgTop, topY, sMsgTop.c_str());

  // ---------- Centro ----------
  u8g2.setFont(u8g2_font_6x12_tf);
  String sLat = fmtCoord(d.lat, true);
  String sLon = fmtCoord(d.lon, false);
  String sAlt = fmtAlt(d.alt);
  String sSpd = fmtSpeed(d.speed_mps);

  u8g2.drawStr(4, 33,  sLat.c_str());
  u8g2.drawStr(90, 33, sLon.c_str());
  int wAlt = u8g2.getStrWidth(sAlt.c_str());
  u8g2.drawStr(256 - 4 - wAlt, 33, sAlt.c_str());

  int wSpd = u8g2.getStrWidth(sSpd.c_str());
  u8g2.drawStr(256 - 4 - wSpd, 47, sSpd.c_str());

  u8g2.drawHLine(0, 51, 256);

  // ---------- Inferior ----------
  String sPdop = fmtPdop(d.pdop);
  String sUtc  = fmtUtc(d.utc);

  u8g2.drawStr(4, 61, sPdop.c_str());
  int wUtc = u8g2.getStrWidth(sUtc.c_str());
  u8g2.drawStr((256 - wUtc)/2, 61, sUtc.c_str());

  u8g2.sendBuffer();
}

// =============================================================
//  GPS DETAIL
// =============================================================
static const char* cardinal(float deg) {
  if (isnan(deg)) return "---";
  static const char* dirs[]={"N","NNE","NE","ENE","E","ESE","SE","SSE",
                             "S","SSW","SW","WSW","W","WNW","NW","NNW"};
  int idx = (int)floor((deg/22.5f)+0.5f) & 15;
  return dirs[idx];
}
void oled_draw_gps_detail(const OwlUiData& d){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.drawStr(6, 14, "GPS Detail");
  u8g2.drawHLine(0, 18, 256);

  u8g2.setFont(u8g2_font_6x12_tf);
  char ln1[64]; snprintf(ln1, sizeof(ln1), "Sats: %02d   PDOP: %s",
                         d.sats<0?0:d.sats,
                         (d.pdop<0?"--":String(d.pdop,1).c_str()));
  u8g2.drawStr(6, 34, ln1);

  String s1 = fmtCoord(d.lat, true);
  String s2 = fmtCoord(d.lon, false);
  u8g2.drawStr(6, 48, s1.c_str());
  u8g2.drawStr(6, 62, s2.c_str());

  String sAlt = fmtAlt(d.alt);
  String sSpd = fmtSpeed(d.speed_mps);
  String sTrk = String("Trk: ") + (isnan(d.course_deg)?"---":String(d.course_deg,0)) +
                " " + cardinal(d.course_deg);

  int wAlt = u8g2.getStrWidth(sAlt.c_str());
  int wSpd = u8g2.getStrWidth(sSpd.c_str());
  int wTrk = u8g2.getStrWidth(sTrk.c_str());

  u8g2.drawStr(256-6-wAlt, 34, sAlt.c_str());
  u8g2.drawStr(256-6-wSpd, 48, sSpd.c_str());
  u8g2.drawStr(256-6-wTrk, 62, sTrk.c_str());

  u8g2.sendBuffer();
}

// =============================================================
//  IRIDIUM DETAIL
// =============================================================
void oled_draw_iridium_detail(bool present, int sigLevel, int unread, const String& imei){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.drawStr(6, 14, "Iridium Detail");
  u8g2.drawHLine(0, 18, 256);

  u8g2.setFont(u8g2_font_6x12_tf);
  String sPr = String("Present: ") + (present?"YES":"NO");
  u8g2.drawStr(6, 34, sPr.c_str());

  char ln1[48];
  snprintf(ln1, sizeof(ln1), "Signal: %s",
          (sigLevel<0?"--":String(sigLevel).c_str()));
  u8g2.drawStr(6, 48, ln1);

  String sIMEI = String("IMEI: ") + (imei.length()?imei:"--");
  u8g2.drawStr(6, 62, sIMEI.c_str());

  String sU = String("MT: ") + (unread>=0?String(unread):String("--"));
  int wU = u8g2.getStrWidth(sU.c_str());
  u8g2.drawStr(256-6-wU, 34, sU.c_str());

  u8g2.sendBuffer();
}

// =============================================================
//  GSM DETAIL
// =============================================================
void oled_draw_gsm_detail(const OwlUiData& d,
                          const String& imei,
                          bool netReg,
                          bool pdpUp,
                          int rssiDbm)
{
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.drawStr(6, 14, "GSM / LTE Detail");
  u8g2.drawHLine(0, 18, 256);

  u8g2.setFont(u8g2_font_6x12_tf);
  char ln1[64];
  snprintf(ln1, sizeof(ln1), "CSQ: %d   RSSI: %d dBm", d.csq, rssiDbm);
  u8g2.drawStr(6, 34, ln1);

  char ln2[64];
  snprintf(ln2, sizeof(ln2), "Net: %s   PDP: %s", netReg ? "REGISTERED" : "NO",
                                              pdpUp  ? "UP"         : "DOWN");
  u8g2.drawStr(6, 48, ln2);

  String sImei = String("IMEI: ") + (imei.length()?imei:"--");
  u8g2.drawStr(6, 62, sImei.c_str());

  u8g2.sendBuffer();
}

// =============================================================
//  BLE DETAIL
// =============================================================
void oled_draw_ble_detail(bool connected, const String& devName)
{
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.drawStr(6, 14, "BLE Detail");
  u8g2.drawHLine(0, 18, 256);

  u8g2.setFont(u8g2_font_6x12_tf);
  const char* st = connected ? "CONNECTED" : "ADVERTISING";
  char ln1[64];
  snprintf(ln1, sizeof(ln1), "Status: %s", st);
  u8g2.drawStr(6, 34, ln1);

  String nm = String("Name: ") + (devName.length()?devName:"OwlTracker");
  u8g2.drawStr(6, 48, nm.c_str());

  u8g2.drawStr(6, 62, "BTN1 long: cycle screens");

  u8g2.sendBuffer();
}

// =============================================================
//  SYS CONFIG y MESSAGES
// =============================================================
void oled_draw_sys_config(const OwlUiData& d, bool netReg, bool pdpUp,
                          const String& ip, bool sdOk, bool i2cOk, const char* fw) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.drawStr(6, 14, "System Config");
  u8g2.drawHLine(0, 18, 256);

  u8g2.setFont(u8g2_font_6x12_tf);
  char ln1[64]; snprintf(ln1, sizeof(ln1), "Net: %s   PDP: %s", netReg?"REG":"NO", pdpUp?"UP":"DOWN");
  u8g2.drawStr(6, 34, ln1);

  String ipS = String("IP: ") + (ip.length()?ip:"--");
  u8g2.drawStr(6, 48, ipS.c_str());

  char ln2[64]; snprintf(ln2, sizeof(ln2), "SD: %s   I2C: %s", sdOk?"OK":"NO", i2cOk?"OK":"NO");
  u8g2.drawStr(6, 62, ln2);

  int w = u8g2.getStrWidth(fw);
  u8g2.drawStr(256-6-w, 34, fw);

  u8g2.sendBuffer();
}

void oled_draw_messages(uint16_t unread, const String& last){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.drawStr(6, 14, "Messages");
  u8g2.drawHLine(0, 18, 256);

  u8g2.setFont(u8g2_font_6x12_tf);
  char ln1[32]; snprintf(ln1, sizeof(ln1), "Unread: %u", unread);
  u8g2.drawStr(6, 34, ln1);

  String sLast = String("Last: ") + (last.length()?last:"--");
  u8g2.drawStr(6, 48, sLast.c_str());

  u8g2.drawStr(6, 62, "BTN1 long: cycle screens");
  u8g2.sendBuffer();
}

// =============================================================
//  TESTING (simple, compat)
// =============================================================
static const char* pref_to_cstr(LinkPref p){
  switch(p){
    case LinkPref::AUTO:     return "AUTO";
    case LinkPref::GSM_ONLY: return "GSM";
    case LinkPref::IR_ONLY:  return "IRIDIUM";
    case LinkPref::BOTH:     return "BOTH";
    default:                 return "--";
  }
}
void oled_draw_testing(LinkPref pref, const char* lastResult, bool busy){
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.drawStr(6, 14, "Testing (legacy)");
  u8g2.drawHLine(0, 18, 256);

  u8g2.setFont(u8g2_font_6x12_tf);
  String ln = String("Pref: ") + pref_to_cstr(pref);
  u8g2.drawStr(6, 34, ln.c_str());

  String lr = String("Last: ") + (lastResult && *lastResult ? lastResult : "--");
  u8g2.drawStr(6, 48, lr.c_str());

  u8g2.drawStr(200, 14, busy ? "RUN..." : "IDLE");
  u8g2.sendBuffer();
}

// =============================================================
//  TESTING avanzado (paginado, no ejecuta)
// =============================================================
void oled_draw_testing_adv(
  const char* modeName, bool netReg, bool pdpUp,
  const char* ratLabel, int csq, int rssiDbm, const char* ipStr,
  const char* imeiGsm,
  bool irPresent, int irSig, const char* irImei, int irUnread,
  bool bleConn, uint32_t freeHeap,
  const char* lastResult, bool busy, uint8_t page
){
  u8g2.clearBuffer();

  // Header
  u8g2.setFont(u8g2_font_7x14_tf);
  char title[32];
  snprintf(title, sizeof(title), "Testing P%u/2", (unsigned)(page+1));
  u8g2.drawStr(6, 14, title);
  u8g2.drawHLine(0, 18, 256);
  u8g2.setFont(u8g2_font_6x12_tf);

  if (page == 0) {
    // -------- Página 1: Red móvil --------
    char ln1[64];
    snprintf(ln1, sizeof(ln1), "Mode: %s   RAT: %s", modeName?modeName:"--", ratLabel?ratLabel:"--");
    u8g2.drawStr(6, 32, ln1);

    char ln2[64];
    snprintf(ln2, sizeof(ln2), "CSQ: %d  RSSI: %d dBm", csq, rssiDbm);
    u8g2.drawStr(6, 44, ln2);

    char ln3[64];
    snprintf(ln3, sizeof(ln3), "Reg: %s   PDP: %s", netReg?"YES":"NO", pdpUp?"UP":"DOWN");
    u8g2.drawStr(6, 56, ln3);

    String ips = String("IP: ") + (ipStr && *ipStr ? ipStr : "--");
    u8g2.drawStr(6, 68, ips.c_str());
  } else {
    // -------- Página 2: Radios/IDs/Sistema --------
    String g = String("GSM IMEI: ") + (imeiGsm && *imeiGsm?imeiGsm:"--");
    u8g2.drawStr(6, 32, g.c_str());

    char ir1[64];
    snprintf(ir1, sizeof(ir1), "IR: %s  SIG:%s  MT:%s",
             irPresent?"YES":"NO",
             (irSig>=0? String(irSig).c_str() : "--"),
             (irUnread>=0? String(irUnread).c_str() : "--") );
    u8g2.drawStr(6, 44, ir1);

    String ir2 = String("IR IMEI: ") + (irImei && *irImei?irImei:"--");
    u8g2.drawStr(6, 56, ir2.c_str());

    char sys[64];
    snprintf(sys, sizeof(sys), "BLE:%s  Heap:%lu",
             bleConn?"ON":"OFF", (unsigned long)freeHeap);
    u8g2.drawStr(6, 68, sys);
  }

  // Estado “busy/idle” y última acción
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(200, 14, busy ? "RUN..." : "IDLE");
  String lr = String("Last: ") + (lastResult && *lastResult ? lastResult : "--");
  int w = u8g2.getStrWidth(lr.c_str());
  if (w > 246) {
    u8g2.drawStr(6, 24, lr.substring(0, 40).c_str());
  } else {
    u8g2.drawStr(6, 24, lr.c_str());
  }

  // Pie de ayuda
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(6, 80, "BTN1: next page | BTN4 long: exit");
  u8g2.sendBuffer();
}
