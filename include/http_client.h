#pragma once
#include <Arduino.h>
#include <TinyGsmClient.h>

class OwlHttpClient {
public:
  OwlHttpClient(TinyGsm& modem, TinyGsmClient& client, const String& host, uint16_t port);
  void begin(TinyGsm& modem, const char* host, uint16_t port, const char* root_ca);

  // Getters seguros (evitan choques con macros)
  TinyGsmClient& client();
  const String&  host() const;
  uint16_t       port() const;

  // HTTP raw (sobre TinyGsmClient)
  int putJson (const char* path, const char* jsonBody, const char* bearer = nullptr);
  int postJson(const char* path, const char* jsonBody, const char* bearer = nullptr);

private:
  // ⚠️ Renombrado para evitar colisiones con posibles macros
  TinyGsmClient& _client;  // referencia, no copiable
  String         _host;
  uint16_t       _port = 80;
};
