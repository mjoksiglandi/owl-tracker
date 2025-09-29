#include "http_client.h"

void OwlHttpClient::setSecure(bool en, const char* ca_pem) {
  // Activamos TLS sólo si tenemos cliente TLS
  use_tls_ = (en && tls_ != nullptr);
  ca_pem_  = ca_pem;

  // Nota: algunos clientes TLS (p.ej. TinyGsmClientSecure) exponen setCACert/setInsecure,
  // pero aquí no hacemos cast directo al tipo concreto para evitar dependencias.
  // Si necesitas establecer la CA explícitamente, hazlo donde construyes el cliente TLS.
}

bool OwlHttpClient::connect() {
  if (use_tls_) {
    return tls_->connect(host_, port_);
  }
  return tcp_->connect(host_, port_);
}

void OwlHttpClient::disconnect() {
  if (use_tls_) {
    tls_->stop();
  } else {
    tcp_->stop();
  }
}

int OwlHttpClient::parseHttpStatus(Stream& s, uint32_t timeout_ms) {
  uint32_t t0 = millis();
  while (!s.available() && millis()-t0 < timeout_ms) { delay(10); }
  if (!s.available()) return -1;

  String status = s.readStringUntil('\n'); // "HTTP/1.1 200 OK\r"
  int sp1 = status.indexOf(' ');
  if (sp1 < 0) return -1;
  int sp2 = status.indexOf(' ', sp1+1);
  String codeStr = (sp2>sp1) ? status.substring(sp1+1, sp2) : status.substring(sp1+1);
  codeStr.trim();
  if (codeStr.length() < 3) return -1;
  return codeStr.toInt();
}

int OwlHttpClient::postJson(const char* path, const char* json, const char* bearer) {
  if (!connect()) return -1;

  size_t bodyLen = json ? strlen(json) : 0;

  String req;
  req.reserve(256 + bodyLen);
  req += "POST "; req += path; req += " HTTP/1.1\r\n";
  req += "Host: "; req += host_; req += "\r\n";
  req += "Connection: close\r\n";
  req += "Content-Type: application/json\r\n";
  if (bearer && *bearer) {
    req += "Authorization: Bearer "; req += bearer; req += "\r\n";
  }
  req += "Content-Length: "; req += String(bodyLen); req += "\r\n\r\n";

  Client& c = use_tls_ ? *tls_ : *tcp_;
  c.print(req);
  if (json && bodyLen) c.write((const uint8_t*)json, bodyLen);

  int code = parseHttpStatus(c);

  disconnect();
  return code;
}
