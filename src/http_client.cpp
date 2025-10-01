#include "http_client.h"

static bool waitAvail_(Stream& s, uint32_t timeout_ms) {
  uint32_t t0 = millis();
  while (!s.available() && (millis() - t0) < timeout_ms) delay(10);
  return s.available() > 0;
}

bool OwlHttpClient::connect(uint32_t timeout_ms) {
  if (tcp_->connected()) return true;
  if (!tcp_->connect(host_, port_)) {
    // Pequeño backoff y reintento único
    delay(100);
    return tcp_->connect(host_, port_);
  }
  // Espera mínima para pila interna
  (void)timeout_ms;
  return tcp_->connected();
}

void OwlHttpClient::disconnect() {
  if (tcp_) tcp_->stop();
}

int OwlHttpClient::postJson(const char* path, const char* body,
                            const char* bearer, uint32_t timeout_ms) {
  return sendRequest_("POST", path, body, bearer, timeout_ms);
}

int OwlHttpClient::putJson(const char* path, const char* body,
                           const char* bearer, uint32_t timeout_ms) {
  return sendRequest_("PUT", path, body, bearer, timeout_ms);
}

int OwlHttpClient::sendRequest_(const char* method, const char* path, const char* body,
                                const char* bearer, uint32_t timeout_ms) {
  if (!connect(8000)) return -1;

  // Construir request
  size_t bodyLen = body ? strlen(body) : 0;

  // Línea de petición y cabeceras
  tcp_->print(method);              tcp_->print(" ");
  tcp_->print(path ? path : "/");  tcp_->print(" HTTP/1.1\r\n");
  tcp_->print("Host: ");           tcp_->print(host_); tcp_->print("\r\n");
  tcp_->print("Connection: close\r\n");
  tcp_->print("Content-Type: application/json\r\n");
  tcp_->print("Content-Length: "); tcp_->print((unsigned long)bodyLen); tcp_->print("\r\n");
  if (bearer && bearer[0]) {
    tcp_->print("Authorization: Bearer "); tcp_->print(bearer); tcp_->print("\r\n");
  }
  tcp_->print("\r\n");

  // Cuerpo
  if (bodyLen) tcp_->write((const uint8_t*)body, bodyLen);

  // Parsear status
  int code = parseStatus_(*tcp_, timeout_ms);

  // Drenar cabeceras y respuesta para limpiar el socket
  drainHeaders_(*tcp_, timeout_ms);

  // Cerramos (si quieres mantener vivo, cambia a keep-alive y no hagas stop())
  disconnect();
  return code;
}

int OwlHttpClient::parseStatus_(Stream& s, uint32_t timeout_ms) {
  if (!waitAvail_(s, timeout_ms)) return -1;
  String line = s.readStringUntil('\n'); // "HTTP/1.1 200 OK\r"
  int sp1 = line.indexOf(' ');
  if (sp1 < 0) return -1;
  int sp2 = line.indexOf(' ', sp1 + 1);
  String code = (sp2 > sp1) ? line.substring(sp1 + 1, sp2) : line.substring(sp1 + 1);
  code.trim();
  return code.toInt();
}

void OwlHttpClient::drainHeaders_(Stream& s, uint32_t timeout_ms) {
  // Leer headers hasta línea en blanco
  uint32_t t0 = millis();
  while ((millis() - t0) < timeout_ms) {
    if (!waitAvail_(s, timeout_ms)) break;
    String line = s.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break; // línea vacía => fin headers
  }

  // Opcional: drenar el body por completo (no necesario si cerramos después)
  // while (s.available()) s.read();
}
