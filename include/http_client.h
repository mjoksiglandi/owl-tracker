#pragma once
#include <Arduino.h>
#include <Client.h>
#include <TinyGsmClient.h>

// Cliente HTTP que funciona con o sin TLS.
// Si pasas un tlsClient != nullptr y llamas setSecure(true, ...), usará TLS.
// Si no, usará tcpClient (HTTP plano).
class OwlHttpClient {
public:
  OwlHttpClient(TinyGsm& modem,
                Client& tcpClient,
                Client* tlsClient,             // puede ser nullptr
                const char* host, uint16_t port)
  : modem_(modem),
    tcp_(&tcpClient),
    tls_(tlsClient),
    host_(host),
    port_(port),
    use_tls_(false),
    ca_pem_(nullptr) {}

  void setEndpoint(const char* host, uint16_t port) { host_ = host; port_ = port; }

  // Si ca_pem = nullptr y tls_ != nullptr => TLS "insecure" (pruebas)
  // Si tls_ == nullptr => fuerza HTTP aunque en 'en' pongas true.
  void setSecure(bool en, const char* ca_pem=nullptr);

  // POST JSON. Devuelve 200..599 o -1 si error de transporte/parsing.
  int postJson(const char* path, const char* json, const char* bearer = nullptr);

private:
  int  parseHttpStatus(Stream& s, uint32_t timeout_ms=5000);
  bool connect();
  void disconnect();

private:
  TinyGsm&  modem_;
  Client*   tcp_;        // obligatorio (HTTP)
  Client*   tls_;        // opcional (HTTPS)
  const char* host_;
  uint16_t    port_;
  bool        use_tls_;
  const char* ca_pem_;   // guardado por si tu cliente TLS lo soporta
};
