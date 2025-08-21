#include "gps.h"

// Estado interno
static GpsFix g_fix;
static String line;

static uint32_t lastGgaMs = 0;
static uint32_t lastRmcMs = 0;
static uint32_t staleMs   = 10000; // 10 s sin GGA/RMC -> invalidar

// --- NUEVO: fecha y hora cacheadas por separado ---
static String s_date = "";  // "YYYY-MM-DD"
static String s_time = "";  // "HH:MM:SS"

void gps_set_stale_timeout(uint32_t ms) { staleMs = ms; }

bool gps_begin_uart(HardwareSerial& gpsSerial, int rxPin, int txPin, uint32_t baud) {
  gpsSerial.begin(baud, SERIAL_8N1, rxPin, txPin);
  g_fix = GpsFix();
  line.reserve(128);
  lastGgaMs = lastRmcMs = millis();
  s_date = ""; s_time = "";
  return true;
}

// ---------- Helpers ----------
static double nmea_coord_to_deg(const String& fld, bool isLat) {
  if (fld.length() < 4) return NAN;
  int degDigits = isLat ? 2 : 3;
  double deg = fld.substring(0, degDigits).toDouble();
  double min = fld.substring(degDigits).toDouble();
  return deg + (min / 60.0);
}

static String ymd_from_rmc_date(const String& d) {
  if (d.length() != 6) return "";
  char buf[11];
  int dd = (d.substring(0,2)).toInt();
  int mm = (d.substring(2,4)).toInt();
  int yy = (d.substring(4,6)).toInt() + 2000;
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", yy, mm, dd);
  return String(buf);
}
static String hms_from_time(const String& t) {
  if (t.length() < 6) return "";
  char buf[9];
  snprintf(buf, sizeof(buf), "%c%c:%c%c:%c%c", t[0],t[1],t[2],t[3],t[4],t[5]);
  return String(buf);
}

// ---------- Parsers ----------
static void parse_gga(const String& s) {
  // $GxGGA,time,lat,N,lon,E,fix,sats,hdop,alt,unit,geoid,unit,age,stat*cs
  String f[15]; int i=0, from=0;
  while (i<15) { int p=s.indexOf(',',from); if(p<0){f[i++]=s.substring(from);break;} f[i++]=s.substring(from,p); from=p+1; }
  if (i < 11) return;

  String time=f[1], lat=f[2], ns=f[3], lon=f[4], ew=f[5], fix=f[6], sats=f[7], alt=f[9];

  if (fix != "0" && lat.length() && lon.length()) {
    double dlat = nmea_coord_to_deg(lat,  true);
    double dlon = nmea_coord_to_deg(lon, false);
    if (ns == "S") dlat = -dlat;
    if (ew == "W") dlon = -dlon;
    g_fix.lat   = dlat;
    g_fix.lon   = dlon;
    g_fix.valid = true;
  } else {
    g_fix.valid = false;
  }

  if (sats.length()) g_fix.sats = sats.toInt();
  if (alt.length())  g_fix.alt  = alt.toDouble();

  // --- ACTUALIZA SIEMPRE LA HORA ---
  if (time.length() >= 6) {
    s_time = hms_from_time(time);  // HH:MM:SS
  }
  // Combina fecha conocida (de RMC) + hora de GGA para g_fix.utc
  if (s_time.length()) {
    if (s_date.length()) g_fix.utc = s_date + " " + s_time;
    else                 g_fix.utc = String("----/--/-- ") + s_time;
  }

  lastGgaMs = millis();
}

static void parse_gsa(const String& s) {
  // $GxGSA,mode,fix,s1,...,PDOP,HDOP,VDOP*CS
  String f[20]; int i=0, from=0;
  while (i<20) { int p=s.indexOf(',',from); if(p<0){f[i++]=s.substring(from);break;} f[i++]=s.substring(from,p); from=p+1; }
  if (i > 15) {
    int star = f[17].indexOf('*'); if (star >= 0) f[17] = f[17].substring(0, star);
    g_fix.pdop = f[15].toFloat();
  }
}

static void parse_rmc(const String& s) {
  // $GxRMC,hhmmss.sss,A,lat,N,lon,E,sog,cog,ddmmyy,...
  String f[20]; int i=0, from=0;
  while (i<20) { int p=s.indexOf(',',from); if(p<0){f[i++]=s.substring(from);break;} f[i++]=s.substring(from,p); from=p+1; }
  if (i < 10) return;
  if (f[2] != "A") return;   // válido

  String date = ymd_from_rmc_date(f[9]); // YYYY-MM-DD
  String time = hms_from_time(f[1]);     // HH:MM:SS
  if (date.length()) s_date = date;
  if (time.length()) s_time = time;

  // g_fix.utc siempre combinada
  if (s_time.length()) {
    if (s_date.length()) g_fix.utc = s_date + " " + s_time;
    else                 g_fix.utc = String("----/--/-- ") + s_time;
  }

  lastRmcMs = millis();
}

// ---------- Poll ----------
void gps_poll(HardwareSerial& gpsSerial) {
  while (gpsSerial.available()) {
    char c = (char)gpsSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (line.length() > 6 && line[0] == '$') {
        if (line.startsWith("$GPGGA") || line.startsWith("$GNGGA") || line.startsWith("$GLGGA"))
          parse_gga(line);
        else if (line.startsWith("$GPGSA") || line.startsWith("$GNGSA") || line.startsWith("$GLGSA"))
          parse_gsa(line);
        else if (line.startsWith("$GPRMC") || line.startsWith("$GNRMC") || line.startsWith("$GLRMC"))
          parse_rmc(line);
      }
      line = "";
    } else if (line.length() < 127) {
      line += c;
    }
  }

  uint32_t now = millis();
  if ((now - lastGgaMs > staleMs) && (now - lastRmcMs > staleMs)) {
    g_fix.valid = false;
  }
}

GpsFix gps_last_fix() { return g_fix; }
