#pragma once
#include <Arduino.h>
#include <ArduinoHttpClient.h>
#include <TinyGsmClient.h>
#include <memory> 

#include <TinyGsmClient.h>

class OwlHttpClient {
public:
    bool begin(TinyGsm& modem, const char* host, uint16_t port, const char* path);
    bool get();
private:
    TinyGsmClient* _cli;
    String _host;
    uint16_t _port;
    String _path;
};
