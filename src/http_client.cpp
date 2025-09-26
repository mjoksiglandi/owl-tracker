#include "http_client.h"

void OwlHttpClient::setSecure(bool en, const char* ca_pem) {
  use_tls_ = en;
  ca_pem_  = ca_pem;
#ifdef TINY_GSM_USE_SSL
  if (tls_) {
    if (use_tls_) {
      if (ca_pem_) {
        tls_->setCACert(ca_pem_);   // validación completa
      } else {
        tls_->setInsecure();        // pruebas (sin validar CA)
      }
    }
  }
#endif
}

bool OwlHttpClient::connect() {
  if (!use_tls_) {
    return tcp_.connect(host_, port_);
  }
#ifdef TINY_GSM_USE_SSL
  if (tls_) {
    return tls_->connect(host_, port_);
  }
#endif
  return false;
}

void OwlHttpClient::disconnect() {
  if (!use_tls_) {
    tcp_.stop();
    return;
  }
#ifdef TINY_GSM_USE_SSL
  if (tls_) tls_->stop();
#endif
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

  String req;
  size_t bodyLen = json ? strlen(json) : 0;
  req.reserve(256 + bodyLen);
  req += "POST "; req += path; req += " HTTP/1.1\r\n";
  req += "Host: "; req += host_; req += "\r\n";
  req += "Connection: close\r\n";
  req += "Content-Type: application/json\r\n";
  if (bearer && *bearer) {
    req += "Authorization: Bearer "; req += bearer; req += "\r\n";
  }
  req += "Content-Length: "; req += String(bodyLen); req += "\r\n\r\n";

  if (!use_tls_) {
    tcp_.print(req);
    if (json && bodyLen) tcp_.write((const uint8_t*)json, bodyLen);
  } else {
#ifdef TINY_GSM_USE_SSL
    tls_->print(req);
    if (json && bodyLen) tls_->write((const uint8_t*)json, bodyLen);
#else
    disconnect();
    return -1;
#endif
  }

  int code = -1;
  if (!use_tls_) {
    code = parseHttpStatus(tcp_);
  } else {
#ifdef TINY_GSM_USE_SSL
    code = parseHttpStatus(*tls_);
#endif
  }

  disconnect();
  return code;
}
