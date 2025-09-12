#include <ArduinoHttpClient.h>
#include "http_client.h"

// Suponiendo que dentro de OwlHttpClient tienes:
//   TinyGsmClient* client_;
//   String host_;
//   uint16_t port_;
// y que begin(...) inicializa esos campos.

static void addAuth(HttpClient& http, const char* bearer) {
  if (bearer && bearer[0]) {
    http.sendHeader("Authorization", bearer);
  }
}

// PUT JSON
int OwlHttpClient::putJson(const char* path, const char* jsonBody, const char* bearer) {
  HttpClient http(*client_, host_.c_str(), port_);
  http.beginRequest();
  http.put(path);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Connection", "close");
  addAuth(http, bearer);
  http.sendHeader("Content-Length", (int)strlen(jsonBody));
  http.beginBody();
  http.print(jsonBody);
  http.endRequest();

  int status = http.responseStatusCode();
  // Puedes leer respuesta si te interesa:
  // String resp = http.responseBody();
  http.stop();
  return status;
}

// POST JSON (por si lo quieres usar también)
int OwlHttpClient::postJson(const char* path, const char* jsonBody, const char* bearer) {
  HttpClient http(*client_, host_.c_str(), port_);
  http.beginRequest();
  http.post(path);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Connection", "close");
  addAuth(http, bearer);
  http.sendHeader("Content-Length", (int)strlen(jsonBody));
  http.beginBody();
  http.print(jsonBody);
  http.endRequest();

  int status = http.responseStatusCode();
  // String resp = http.responseBody();
  http.stop();
  return status;
}
