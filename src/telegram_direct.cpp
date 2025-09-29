#include "telegram_direct.h"

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include "secrets.h"

// Externs provistos por tu main
extern TinyGsm modem;
extern TinyGsmClient gsmClient;
extern SemaphoreHandle_t g_modem_mtx;
extern volatile bool g_net_registered;
extern volatile bool g_pdp_up;

static inline bool modem_lock(uint32_t ms=15000){
  return g_modem_mtx ? (xSemaphoreTake(g_modem_mtx, pdMS_TO_TICKS(ms)) == pdTRUE) : true;
}
static inline void modem_unlock(){
  if (g_modem_mtx) xSemaphoreGive(g_modem_mtx);
}

// ===== SSLClient sobre el cliente TCP del módem =====
// Para pruebas: sin anchors, modo inseguro (acepta cualquier cert)
static SSLClient tgSSL(&gsmClient);

static String urlencode_(const String& s) {
  static const char *hex = "0123456789ABCDEF";
  String out; out.reserve(s.length()*3);
  for (size_t i=0;i<s.length();++i){
    uint8_t c = (uint8_t)s[i];
    bool safe = (c=='-'||c=='_'||c=='.'||c=='~'||
                 (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'));
    if (safe) out += (char)c;
    else if (c == ' ') out += "%20";
    else { out += '%'; out += hex[(c>>4)&0xF]; out += hex[c&0xF]; }
  }
  return out;
}

static int parse_http_status_(Stream& s, uint32_t timeout_ms=10000) {
  uint32_t t0 = millis();
  while (!s.available() && (millis()-t0) < timeout_ms) delay(10);
  if (!s.available()) return -1;
  String line = s.readStringUntil('\n'); // "HTTP/1.1 200 OK\r"
  int sp1 = line.indexOf(' ');
  if (sp1 < 0) return -1;
  int sp2 = line.indexOf(' ', sp1+1);
  String code = (sp2>sp1) ? line.substring(sp1+1, sp2) : line.substring(sp1+1);
  code.trim();
  return code.toInt();
}

bool telegram_send_text_direct(const String& text) {
  if (!g_net_registered) { Serial.println("[TG] sin red"); return false; }
  if (!g_pdp_up)         { Serial.println("[TG] sin PDP"); return false; }

  const char* HOST = "api.telegram.org";
  const uint16_t PORT = 443;

  // Para pruebas: desactiva validación de certificado
  tgSSL.setInsecure();

  if (!modem_lock(15000)) { Serial.println("[TG] modem busy"); return false; }

  bool ok = tgSSL.connect(HOST, PORT);
  if (!ok) {
    Serial.println("[TG] connect fail");
    modem_unlock();
    return false;
  }

  // Construye POST application/x-www-form-urlencoded
  String path = String("/bot") + String(TG_BOT_TOKEN) + "/sendMessage";
  String payload = String("chat_id=") + String(TG_CHAT_ID) + "&text=" + urlencode_(text);

  String req;
  req.reserve(256 + payload.length());
  req += "POST "; req += path; req += " HTTP/1.1\r\n";
  req += "Host: "; req += HOST; req += "\r\n";
  req += "Connection: close\r\n";
  req += "Content-Type: application/x-www-form-urlencoded\r\n";
  req += "Content-Length: "; req += String(payload.length()); req += "\r\n\r\n";
  req += payload;

  tgSSL.print(req);

  int code = parse_http_status_(tgSSL, 12000);
  Serial.printf("[TG] HTTP %d\n", code);

  tgSSL.stop();
  modem_unlock();

  return (code == 200);
}
