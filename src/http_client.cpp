#include <ArduinoHttpClient.h>
#include "http_client.h"

// Lee la primera línea "HTTP/1.1 200 OK"
static int parse_status_line(Stream& s) {
  String line = s.readStringUntil('\n');  // respeta timeout del cliente
  int sp1 = line.indexOf(' ');
  if (sp1 < 0) return -1;
  int sp2 = line.indexOf(' ', sp1 + 1);
  if (sp2 < 0) sp2 = line.length();
  return line.substring(sp1 + 1, sp2).toInt();
}

int OwlHttpClient::putJson(const char* path, const char* jsonBody, const char* bearer) {
  if (!client_h.connect(_host.c_str(), _port)) return -1;

  client_h.print("PUT ");
  client_h.print(path);
  client_h.print(" HTTP/1.1\r\nHost: ");
  client_h.print(_host);
  client_h.print("\r\nConnection: close\r\nContent-Type: application/json\r\n");

  if (bearer && bearer[0]) {
    client_h.print("Authorization: ");
    client_h.print(bearer);
    client_h.print("\r\n");
  }

  client_h.print("Content-Length: ");
  client_h.print((int)strlen(jsonBody));
  client_h.print("\r\n\r\n");
  client_h.print(jsonBody);

  int code = parse_status_line(client_h);

  uint32_t t0 = millis();
  while ((client_h.connected() || client_h.available()) && (millis() - t0 < 5000)) {
    while (client_h.available()) client_h.read();
    delay(1);
  }
  client_h.stop();
  return code;
}

int OwlHttpClient::postJson(const char* path, const char* jsonBody, const char* bearer) {
  if (!client_h.connect(_host.c_str(), _port)) return -1;

  client_h.print("POST ");
  client_h.print(path);
  client_h.print(" HTTP/1.1\r\nHost: ");
  client_h.print(_host);
  client_h.print("\r\nConnection: close\r\nContent-Type: application/json\r\n");

  if (bearer && bearer[0]) {
    client_h.print("Authorization: ");
    client_h.print(bearer);
    client_h.print("\r\n");
  }

  client_h.print("Content-Length: ");
  client_h.print((int)strlen(jsonBody));
  client_h.print("\r\n\r\n");
  client_h.print(jsonBody);

  int code = parse_status_line(client_h);

  uint32_t t0 = millis();
  while ((client_h.connected() || client_h.available()) && (millis() - t0 < 5000)) {
    while (client_h.available()) client_h.read();
    delay(1);
  }
  client_h.stop();
  return code;
}
