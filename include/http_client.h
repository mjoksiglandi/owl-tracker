 #pragma once
#include <Arduino.h>
#include <ArduinoHttpClient.h>
#include <TinyGsmClient.h>
#include <memory> 

class OwlHttpClient {

public:
  OwlHttpClient(TinyGsm& modem, TinyGsmClient& client, const String& host, uint16_t port);
  void begin(TinyGsm& modem, const char* host, uint16_t port, const char* root_ca);

  TinyGsmClient& client();        // devuelve la referencia al cliente GPRS
  const String&  host() const;    // host actual
  uint16_t       port() const;    // puerto actual

  // === Deben estar en PUBLIC ===
  int putJson (const char* path, const char* jsonBody, const char* bearer = nullptr);
  int postJson(const char* path, const char* jsonBody, const char* bearer = nullptr);

private:
  // NOTA: estos nombres deben existir en tu clase real.
  // Si tus miembros ya se llaman así, perfecto. Si cambian, ajusta las
  // implementaciones en el .cpp para usar los nombres correctos.
  TinyGsmClient client_h;  // o referencia si así la tienes
  String        _host;
  uint16_t      _port = 80;
};

