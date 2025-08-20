#include "gps.h"

static GpsFix g_fix;
static String line;

// --- helpers ---
static double nmea_coord_to_deg(const String& fld, bool isLat) {
  // lat: ddmm.mmmm ; lon: dddmm.mmmm
  if (fld.length() < 4) return NAN;
  int degDigits = isLat ? 2 : 3;
  double deg = fld.substring(0, degDigits).toDouble();
  double min = fld.substring(degDigits).toDouble();
  return deg + (min / 60.0);
}

static void parse_gga(const String& s) {
  // $GxGGA,time,lat,N,lon,E,fix,sats,hdop,alt,unit,geoid,unit,age,stat*cs
  // campos:
  // 0:$..GGA 1:time 2:lat 3:N/S 4:lon 5:E/W 6:fix 7:sats 8:hdop ...
  int idx = 0, from = 0;
  String f[15];
  while (idx < 15) {
    int p = s.indexOf(',', from);
    if (p < 0) { f[idx++] = s.substring(from); break; }
    f[idx++] = s.substring(from, p);
    from = p + 1;
  }
  if (idx < 9) return;

  String time = f[1];
  String lat  = f[2];
  String ns   = f[3];
  String lon  = f[4];
  String ew   = f[5];
  String fix  = f[6];
  String sats = f[7];

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

  // time HHMMSS.sss -> HH:MM:SS
  if (time.length() >= 6) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%c%c:%c%c:%c%c",
             time[0], time[1], time[2], time[3], time[4], time[5]);
    g_fix.utc = String(buf);  // ojo: UTC GNSS (no ajustamos fecha aquí)
  }
}

static void parse_gsa(const String& s) {
  // $GxGSA,mode,fix,s1,s2,...,PDOP,HDOP,VDOP*CS
  // PDOP típico es el campo 15 (índice 15 con base 0 contando '$'..)
  int count = 0, from = 0;
  String f[20];
  while (count < 20) {
    int p = s.indexOf(',', from);
    if (p < 0) { f[count++] = s.substring(from); break; }
    f[count++] = s.substring(from, p);
    from = p + 1;
  }
  // PDOP está 3 antes del final si hay los 3 DOPs; para simplificar,
  // buscamos el último bloque separado por '*' y tomamos el campo -2
  // pero más simple: muchos emisores ponen PDOP en f[15]
  if (count > 15) {
    // quitar checksum del último campo si vino pegado "VDOP*CS"
    int star = f[17].indexOf('*');
    if (star >= 0) f[17] = f[17].substring(0, star);

    float pdop = f[15].toFloat();
    if (pdop > 0.0f) g_fix.pdop = pdop;
  }
}

// --- API ---
bool gps_begin_uart(HardwareSerial& gpsSerial, int rxPin, int txPin, uint32_t baud) {
  gpsSerial.begin(baud, SERIAL_8N1, rxPin, txPin);
  g_fix = GpsFix();  // reset
  line.reserve(128);
  return true;
}

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
      }
      line = "";
    } else {
      if (line.length() < 127) line += c; // evita desbordes
    }
  }
}

GpsFix gps_last_fix() {
  return g_fix;
}
