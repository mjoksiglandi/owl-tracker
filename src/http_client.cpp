#include <Arduino.h>
#include <TinyGsmClient.h>     // asegura la declaración de TinyGsmClient[Secure]
#include "http_client.h"

bool OwlHttpClient::begin(TinyGsm& modem, const char* host, uint16_t port, const char* path) {
    _cli = new TinyGsmClient(modem);   // 👈 cambio aquí
    _host = host;
    _port = port;
    _path = path;
    return true;
}

bool OwlHttpClient::get() {
    if (!_cli->connect(_host.c_str(), _port)) {
        Serial.println("[Owl] ❌ No se pudo conectar al servidor");
        return false;
    }

    _cli->print(String("GET ") + _path + " HTTP/1.1\r\n" +
                "Host: " + _host + "\r\n" +
                "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (_cli->connected() && millis() - timeout < 10000L) {
        while (_cli->available()) {
            String line = _cli->readStringUntil('\n');
            Serial.println(line);
            timeout = millis();
        }
    }

    _cli->stop();
    return true;
}
