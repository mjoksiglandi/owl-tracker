#pragma once
#include <Arduino.h>
#include <TinyGsmClient.h>

class OwlHttpClient {
public:
  OwlHttpClient(TinyGsm& modem,
                TinyGsmClient& tcpClient,
#ifdef TINY_GSM_USE_SSL
                TinyGsmClientSecure& tlsClient,
#endif
                const char* host, uint16_t port)
  : modem_(modem),
    tcp_(tcpClient),
#ifdef TINY_GSM_USE_SSL
    tls_(&tlsClient),
#else
    tls_(nullptr),
#endif
    host_(host),
    port_(port),
    use_tls_(false),
    ca_pem_(nullptr)
  {}

  // Cambiar endpoint en runtime (opcional)
  void setEndpoint(const char* host, uint16_t port) {
    host_ = host; port_ = port;
  }

  // Activa HTTPS. Si ca_pem = nullptr, va "insecure" (para pruebas)
  void setSecure(bool en, const char* ca_pem=nullptr);

  // POST application/json (Bearer opcional)
  // Devuelve código HTTP (200..599) o -1 si falla transporte/parsing.
  int postJson(const char* path, const char* json, const char* bearer = nullptr);

private:
  int  parseHttpStatus(Stream& s, uint32_t timeout_ms=5000);
  bool connect();
  void disconnect();

private:
  TinyGsm&        modem_;
  TinyGsmClient&  tcp_;
#ifdef TINY_GSM_USE_SSL
  TinyGsmClientSecure* tls_;
#else
  void*           tls_; // dummy
#endif
  const char*     host_;
  uint16_t        port_;
  bool            use_tls_;
  const char*     ca_pem_;
};
