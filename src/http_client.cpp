#include "http_client.h"
#include <cstring> // strlen

// ==================== CTOR / BEGIN / GETTERS ====================
OwlHttpClient::OwlHttpClient(TinyGsm& /*modem*/,
                             TinyGsmClient& client,
                             const String& host,
                             uint16_t port)
: _client(client), _host(host), _port(port)
{}

void OwlHttpClient::begin(TinyGsm& /*modem*/,
                          const char* host,
                          uint16_t port,
                          const char* /*root_ca*/)
{
  _host = (host ? host : "");
  _port = port;
}

TinyGsmClient& OwlHttpClient::client()       { return _client; }
const String&  OwlHttpClient::host() const   { return _host; }
uint16_t       OwlHttpClient::port() const   { return _port; }

// ==================== HELPERS INTERNOS ====================
static int parse_status_line(Stream& s, uint32_t timeout_ms = 3000) {
  uint32_t t0 = millis();
  while (!s.available() && (millis() - t0 < timeout_ms)) { delay(1); }
  if (!s.available()) return -1;

  // Primera línea: "HTTP/1.1 200 OK\r\n"
  String line = s.readStringUntil('\n');
  int sp1 = line.indexOf(' ');
  if (sp1 < 0) return -1;
  int sp2 = line.indexOf(' ', sp1 + 1);
  if (sp2 < 0) sp2 = line.length();
  return line.substring(sp1 + 1, sp2).toInt();
}

static void flush_and_close(Client& c, uint32_t timeout_ms = 5000) {
  uint32_t t0 = millis();
  while ((c.connected() || c.available()) && (millis() - t0 < timeout_ms)) {
    while (c.available()) c.read();
    delay(1);
  }
  c.stop();
}

// ==================== PUT / POST JSON ====================
int OwlHttpClient::putJson(const char* path,
                           const char* jsonBody,
                           const char* bearer)
{
  auto& cl = client();
  if (!cl.connect(host().c_str(), port())) return -1;

  cl.print("PUT ");
  cl.print(path ? path : "/");
  cl.print(" HTTP/1.1\r\nHost: ");
  cl.print(host());
  cl.print("\r\nConnection: close\r\nContent-Type: application/json\r\n");

  if (bearer && bearer[0]) {
    cl.print("Authorization: ");
    cl.print(bearer);
    cl.print("\r\n");
  }

  cl.print("Content-Length: ");
  cl.print((int)strlen(jsonBody ? jsonBody : ""));
  cl.print("\r\n\r\n");

  if (jsonBody) cl.print(jsonBody);

  int code = parse_status_line(cl);
  flush_and_close(cl);
  return code;
}

int OwlHttpClient::postJson(const char* path,
                            const char* jsonBody,
                            const char* bearer)
{
  auto& cl = client();
  if (!cl.connect(host().c_str(), port())) return -1;

  cl.print("POST ");
  cl.print(path ? path : "/");
  cl.print(" HTTP/1.1\r\nHost: ");
  cl.print(host());
  cl.print("\r\nConnection: close\r\nContent-Type: application/json\r\n");

  if (bearer && bearer[0]) {
    cl.print("Authorization: ");
    cl.print(bearer);
    cl.print("\r\n");
  }

  cl.print("Content-Length: ");
  cl.print((int)strlen(jsonBody ? jsonBody : ""));
  cl.print("\r\n\r\n");

  if (jsonBody) cl.print(jsonBody);

  int code = parse_status_line(cl);
  flush_and_close(cl);
  return code;
}
