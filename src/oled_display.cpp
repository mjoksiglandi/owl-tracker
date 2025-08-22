#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "board_pins.h"
#include "oled_display.h"

// HW SPI (VSPI)
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(
  U8G2_R0, /*cs=*/OLED_PIN_CS, /*dc=*/OLED_PIN_DC, /*reset=*/OLED_PIN_RST
);

static inline int clamp(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

// -------- Iconos compactos (status bar) --------
static void drawAntennaSmall(int x, int y){ u8g2.drawBox(x, y-5, 1, 5); u8g2.drawBox(x-2, y-1, 5, 1); }
static void drawBarsSmall(int x, int y, uint8_t level, uint8_t maxBars){
  for(uint8_t i=0;i<maxBars;i++){ int h=1+i, yy=y-h, xx=x+i*3; if(i<level) u8g2.drawBox(xx,yy,2,h); else u8g2.drawFrame(xx,yy,2,h); }
}
static void drawSatelliteSmall(int x, int y){ u8g2.drawCircle(x+4,y-4,4,U8G2_DRAW_ALL); u8g2.drawLine(x+1,y-4,x+7,y-4); u8g2.drawLine(x+4,y-7,x+4,y-1); }
static void drawGlobeSmall(int x, int y){ u8g2.drawCircle(x+4,y-4,4,U8G2_DRAW_ALL); u8g2.drawLine(x+1,y-4,x+7,y-4); u8g2.drawLine(x+4,y-7,x+4,y-1); u8g2.drawEllipse(x+4,y-4,3,2,U8G2_DRAW_ALL); }
static void drawHeadingArrow8(int x,int y,float deg){
  static const int8_t dx[8]={0,1,1,1,0,-1,-1,-1}, dy[8]={-1,-1,0,1,1,1,0,-1};
  if(isnan(deg)){ u8g2.drawPixel(x,y-3); u8g2.drawFrame(x-2,y-5,4,4); return; }
  int idx=(int)((deg+22.5f)/45.0f)&7; int cx=x,cy=y-3;
  u8g2.drawLine(cx,cy,cx+dx[idx]*5,cy+dy[idx]*5);
  u8g2.drawLine(cx+dx[idx]*3,cy+dy[idx]*3,cx+dx[(idx+1)&7]*2,cy+dy[(idx+1)&7]*2);
  u8g2.drawLine(cx+dx[idx]*3,cy+dy[idx]*3,cx+dx[(idx+7)&7]*2,cy+dy[(idx+7)&7]*2);
}

// CSQ -> barras 0..5
static uint8_t csq_to_bars(int csq){ if(csq<0||csq==99) return 0; if(csq<=3) return 1; if(csq<=9) return 2; if(csq<=15) return 3; if(csq<=22) return 4; return 5; }

// Strings
static String fmtCoord(double v, bool isLat){ if(isnan(v)) return isLat?"Lat: --.--":"Lon: --.--"; char b[28]; snprintf(b,sizeof(b),"%s%.6f",(isLat?(v<0?"S ":"N "):(v<0?"W ":"E ")),fabs(v)); return String(b); }
static String fmtAlt(double a){ if(isnan(a)) return "Alt: -- m"; char b[24]; snprintf(b,sizeof(b),"Alt: %.1f m",a); return String(b); }
static String fmtMsgs(uint32_t n){ char b[24]; snprintf(b,sizeof(b),"MSG RX: %lu",(unsigned long)n); return String(b); }
static String fmtUtc(const String& s){ return s.length()?s:String("--:--:--Z"); }

// PDOP -> letra
static char pdopGrade(float p){ if(p<0) return '-'; if(p<1.5f) return 'E'; if(p<4.0f) return 'B'; if(p<6.0f) return 'M'; if(p<10.0f) return 'A'; return 'D'; }

bool oled_init(){
  SPI.begin(OLED_PIN_SCK, /*MISO*/-1, OLED_PIN_MOSI, OLED_PIN_CS);
  pinMode(OLED_PIN_RST, OUTPUT); digitalWrite(OLED_PIN_RST, LOW); delay(10); digitalWrite(OLED_PIN_RST, HIGH); delay(10);
  u8g2.begin(); u8g2.setContrast(210); return true;
}

// Layout 256x64
// top y=12: CEL | GPS nn | ↗ Vx.x | Pxxxx | IR
// mid y=34 : Lat ... | Lon ... | Alt ...
// bot y=62 : PDOP: G |   UTC   | MSG RX: n
void oled_draw_dashboard(const OwlUiData& d){
  u8g2.clearBuffer();

  // --- TOP ---
  int topY=12; u8g2.setFont(u8g2_font_5x8_mf);
  drawAntennaSmall(4,topY); drawBarsSmall(10,topY,clamp(csq_to_bars(d.csq),0,5),5); u8g2.drawStr(10+5*3+3,topY,"CEL");

  int xgps=80; drawSatelliteSmall(xgps,topY); char gbuf[10];
  if(d.sats>=0) snprintf(gbuf,sizeof(gbuf),"GPS %02d",d.sats); else snprintf(gbuf,sizeof(gbuf),"GPS --");
  u8g2.drawStr(xgps+12,topY,gbuf);

  int xv=150; drawHeadingArrow8(xv,topY,d.course_deg); char vbuf[10];
  if(isnan(d.speed_mps)) snprintf(vbuf,sizeof(vbuf),"V-.-"); else snprintf(vbuf,sizeof(vbuf),"V%.1f",d.speed_mps);
  u8g2.drawStr(xv+10,topY,vbuf);

  int xp=196; char pbuf[12]; if(isnan(d.pressure_hpa)) snprintf(pbuf,sizeof(pbuf),"P----"); else snprintf(pbuf,sizeof(pbuf),"P%04d",(int)roundf(d.pressure_hpa));
  u8g2.drawStr(xp,topY,pbuf);

  int xir=232; drawGlobeSmall(xir,topY);
  if(d.iridiumLvl<0){ u8g2.drawStr(xir+12,topY,"IR --"); }
  else{ uint8_t ib=clamp(d.iridiumLvl,0,5); drawBarsSmall(xir+12,topY,ib,5); u8g2.drawStr(xir+12+5*3+3,topY,"IR"); }

  // --- MID ---
  u8g2.setFont(u8g2_font_6x12_tf);
  String sLat=fmtCoord(d.lat,true), sLon=fmtCoord(d.lon,false), sAlt=fmtAlt(d.alt);
  u8g2.drawStr(6,34,sLat.c_str()); u8g2.drawStr(90,34,sLon.c_str());
  int wAlt=u8g2.getStrWidth(sAlt.c_str()); u8g2.drawStr(256-6-wAlt,34,sAlt.c_str());

  // --- BOT ---
  char pd[12]; snprintf(pd,sizeof(pd),"PDOP: %c",pdopGrade(d.pdop));
  String sMsg=fmtMsgs(d.msgRx), sUtc=fmtUtc(d.utc);
  u8g2.drawStr(6,62,pd);
  int wMsg=u8g2.getStrWidth(sMsg.c_str()); u8g2.drawStr(256-6-wMsg,62,sMsg.c_str());
  int wUtc=u8g2.getStrWidth(sUtc.c_str()); u8g2.drawStr((256-wUtc)/2,62,sUtc.c_str());

  u8g2.sendBuffer();
}
