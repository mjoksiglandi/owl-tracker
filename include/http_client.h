#pragma once
#include <Arduino.h>
#include <ArduinoHttpClient.h>
#include <TinyGsmClient.h>
#include <memory> 

#include <TinyGsmClient.h>

class OwlHttpClient {
    // Agrega estas declaraciones dentro de la clase OwlHttpClient:
int putJson(const char* path, const char* jsonBody, const char* bearer = nullptr);
int postJson(const char* path, const char* jsonBody, const char* bearer = nullptr);

public:
    bool begin(TinyGsm& modem, const char* host, uint16_t port, const char* path);
    bool get();
private:
    TinyGsmClient* _cli;
    String _host;
    uint16_t _port;
    String _path;
};

