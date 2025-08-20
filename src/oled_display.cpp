#include "board_pins.h"
#include "oled_display.h"
#include <U8g2lib.h>
#include <SPI.h>

// SH1122 256x64, SPI HW 4‑hilos
// U8G2_SSD1322_NHD_256X64_F_4W_SW_SPI u8g2(
//   U8G2_R0, /*clk=*/18, /*mosi=*/23, /*cs=*/5, /*dc=*/21, /*reset=*/22
// );
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(
  U8G2_R0, /*cs=*/5, /*dc=*/21, /*reset=*/22
);

// ===== Helpers señal =====
int csq_to_dbm(int csq) {
  if (csq <= 0)  return -113;
  if (csq == 1)  return -111;
  if (csq >= 31) return -51;
  return -113 + 2 * csq;
}
int csq_to_bars(int csq) {
  if (csq == 99 || csq < 0) return 0;
  int b = (csq * 6) / 32;         // 0..5
  if (b < 0) b = 0; if (b > 5) b = 5;
  return b;
}

// ===== Iconos sólidos nítidos =====
static void drawAntenna(int x, int y) {          // 📡
  u8g2.drawBox(x+2, y-10, 2, 10);                // mástil
  u8g2.drawBox(x,   y-2,  6, 2);                 // base
}
static void drawGlobe(int x, int y) {            // 🌍
  u8g2.drawCircle(x, y-4, 6, U8G2_DRAW_ALL);
  u8g2.drawVLine(x,   y-10, 12);
  u8g2.drawHLine(x-6, y-4,  12);
}
static void drawSatellite(int x, int y) {        // 🛰
  u8g2.drawBox(x-4,  y-3,  8, 6);                // cuerpo
  u8g2.drawBox(x-10, y-2,  4, 4);                // panel izq
  u8g2.drawBox(x+6,  y-2,  4, 4);                // panel der
}

// ===== Barras =====
static void drawBars5(int x, int y, int level) { // gruesas (RSSI)
  for (int i=0;i<5;i++) {
    int h = (i+1)*3;                              // 3,6,9,12,15 px
    if (i < level) u8g2.drawBox(x+i*4, y-h, 3, h);
  }
}
static void drawBars5Thin(int x, int y, int level) { // finas (Iridium)
  if (level < 0) return;
  for (int i=0;i<5;i++) {
    int h = (i+1)*2;                              // 2,4,6,8,10 px
    if (i < level) u8g2.drawBox(x+i*3, y-h, 2, h);
  }
}

// ===== Utils =====
static bool hasPdp(const String& ip) {
  return ip.length() && ip != "0.0.0.0";
}
static String fitRight(const String& s, uint8_t maxChars) {
  if (s.length() <= maxChars) return s;
  return s.substring(0, maxChars-1) + "…";
}

// ===== API =====
bool oled_init() {
  SPI.begin(OLED_PIN_SCK, /*MISO=*/-1, OLED_PIN_MOSI, OLED_PIN_CS);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setContrast(180);                 // ajusta 160..220 si lo ves muy tenue/brillante
  return true;
}

void oled_splash(const char* title) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(6, 20, title);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(6, 38, "Inicializando...");
  u8g2.sendBuffer();
}

// Layout 256x64:
// Top:  📡 + barras  |           IP: a.b.c.d           |   🌍 + barras finas
// Mid:  Lat / Lon centrado
// Bot:  🛰 SAT/PDOP (izq) | UTC (centro) | MSG RX (derecha)
void oled_draw_dashboard(const OwlUiData& d) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  // ------- TOP -------
  // Antena (RSSI)
  drawAntenna(2, 14);
  drawBars5(14, 14, csq_to_bars(d.csq));

  // IP o “SIN PDP” invertido a la derecha
  if (hasPdp(d.ip)) {
    String ipShow = fitRight(d.ip, 18);
    String ipLbl  = "IP: " + ipShow;
    int ipW = u8g2.getStrWidth(ipLbl.c_str());
    u8g2.drawStr(256 - ipW - 2, 12, ipLbl.c_str());
  } else {
    const char* noPdp = " SIN PDP ";
    int w = u8g2.getStrWidth(noPdp);
    int x = 256 - w - 2;
    u8g2.drawBox(x-2, 2, w+4, 12);   // etiqueta invertida
    u8g2.setDrawColor(0);
    u8g2.drawStr(x, 12, noPdp);
    u8g2.setDrawColor(1);
  }

  // IR (globo + barras finas), si hay dato
  if (d.iridiumLvl >= 0) {
    // posición fija sobria a ~mitad superior derecha (ajusta si quieres)
    int irX = 136;
    drawGlobe(irX, 14);
    drawBars5Thin(irX+10, 14, d.iridiumLvl);
  }

  // ------- MIDDLE (lat/lon centrado) -------
  char ll[64];
  if (!isnan(d.lat) && !isnan(d.lon))
    snprintf(ll, sizeof(ll), "Lat: %.6f  Lon: %.6f", d.lat, d.lon);
  else
    snprintf(ll, sizeof(ll), "Lat: --.------  Lon: --.------");
  int llW = u8g2.getStrWidth(ll);
  u8g2.drawStr((256 - llW)/2, 30, ll);

  // ------- BOTTOM -------
  // SAT/PDOP (izquierda)
  char leftLine[32];
  if (d.sats >= 0 && d.pdop >= 0)      snprintf(leftLine, sizeof(leftLine), "%d  PDOP:%.1f", d.sats, d.pdop);
  else if (d.sats >= 0)                snprintf(leftLine, sizeof(leftLine), "%d", d.sats);
  else if (d.pdop >= 0)                snprintf(leftLine, sizeof(leftLine), "PDOP:%.1f", d.pdop);
  else                                 snprintf(leftLine, sizeof(leftLine), "-");
  drawSatellite(10, 62);
  u8g2.drawStr(22, 62, leftLine);

  // UTC (centro)
  String utcShow = d.utc.length()? d.utc : String("--:--:--Z");
  int utcW = u8g2.getStrWidth(utcShow.c_str());
  u8g2.drawStr((256 - utcW)/2, 62, utcShow.c_str());

  // Mensajes recibidos (derecha)
  char rightLine[24];
  snprintf(rightLine, sizeof(rightLine), "MSG RX:%lu", (unsigned long)d.msgRx);
  int rightW = u8g2.getStrWidth(rightLine);
  u8g2.drawStr(256 - rightW - 2, 62, rightLine);

  u8g2.sendBuffer();
}
