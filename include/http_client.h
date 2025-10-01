#pragma once
#include <Arduino.h>
#include <Client.h>
#include <TinyGsmClient.h>

/**
 * OwlHttpClient (versión simple, sin TLS)
 * - Usa un Client (TinyGsmClient) igual que ArduinoHttpClient.
 * - Métodos: connect(), disconnect(), postJson(), putJson().
 * - setSecure(...) existe por compatibilidad pero se ignora.
 */
class OwlHttpClient {
public:
  OwlHttpClient(TinyGsm& modem, Client& tcpClient, const char* host, uint16_t port)
  : modem_(&modem), tcp_(&tcpClient), host_(host), port_(port) {}

  // Compat: no hace nada en esta versión
  void setSecure(bool /*enable*/, const char* /*caPem*/ = nullptr) {}

  bool connect(uint32_t timeout_ms = 10000);
  void disconnect();

  // Devuelven el código HTTP o -1 si falló
  int postJson(const char* path, const char* body,
               const char* bearer = nullptr, uint32_t timeout_ms = 12000);
  int putJson (const char* path, const char* body,
               const char* bearer = nullptr, uint32_t timeout_ms = 12000);

private:
  TinyGsm*  modem_;
  Client*   tcp_;
  const char* host_;
  uint16_t  port_;

  int  sendRequest_(const char* method, const char* path, const char* body,
                    const char* bearer, uint32_t timeout_ms);
  int  parseStatus_(Stream& s, uint32_t timeout_ms);
  void drainHeaders_(Stream& s, uint32_t timeout_ms);
};
