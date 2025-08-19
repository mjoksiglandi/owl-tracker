# 🦉 Owl Tracker

Proyecto de telemetría IoT basado en **ESP32 + SIMCom A7670G (Cat‑1/LTE)** sobre **PlatformIO**.  
Objetivo: conectar a red celular, obtener estado del módem (operador/RSSI/RAT), y enviar datos (JSON) a un backend HTTP/MQTT.  
Este repositorio es **privado** y de uso interno.

---

## ✅ Estado actual (hito base)

- Secuencia de **power‑on** del A7670G (EN=12, PWRKEY=4).
- Forzado de **LTE only (AT+CNMP=38)** con verificación y persistencia (`AT&W`).
- **Registro de red** (+ prints de operador / RSSI / RAT).
- **PDP activo** (APN WOM: `internet`) y **IP obtenida**.

Salida típica por monitor serie:
[Owl] Power: EN=HIGH, PWRKEY pulse HIGH (1200ms + 4000ms)
[Owl] Inicializando módem...
[Owl][AT] +CNMP=38 -> OK
[Owl] RAT = LTE only confirmado
[Owl][AT] &W -> OK
[Owl] Esperando registro de red...
.[Owl] Red conectada ✅
[Owl] Operador: 73009
[Owl] RSSI: 30 dBm
[Owl] RAT: 38
[Owl] PDP ACTIVO
[Owl] IP: 223.44.156.252

yaml
Copiar
Editar

---

## 🧱 Estructura del proyecto
````
Owl/
├─ include/ # Headers (.h)
│ └─ board_pins.h, config.h, net_config.h
├─ src/
│ ├─ main.cpp # Punto de entrada (stages)
│ ├─ modem_config.h/.cpp # Encendido y configuración RAT + registro
│ ├─ http_client.h/.cpp # (cliente HTTP simple; en evolución)
│ └─ core/log.* # (si se usa logging custom)
├─ lib/ # Librerías locales (opcional)
├─ platformio.ini
└─ README.md

````

## 🔌 Hardware
````
- Placa: **LILYGO T‑A7670G** (ESP32‑WROVER‑E + SIMCom A7670G).
- Pines usados (revisión R2):
  - **MODEM_TX (ESP32→A7670 RX): 26**
  - **MODEM_RX (ESP32←A7670 TX): 27**
  - **MODEM_PWR (PWRKEY): 4**
  - **MODEM_EN: 12**
- Antena LTE conectada y **SIM nano** (sin PIN).
- **Alimentación robusta** (picos >2 A). Ideal: batería 18650 + USB.

````

## ⚙️ Setup (PlatformIO)

`platformio.ini` (fragmento clave):

```ini
[env:owl]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed  = 921600

build_unflags = -std=gnu++11
build_flags =
  -std=gnu++17
  -DTINY_GSM_MODEM_SIM7600
  -DTINY_GSM_RX_BUFFER=2048

lib_deps =
  https://github.com/lewisxhe/TinyGSM-fork.git#master
  bblanchon/ArduinoJson @ ^7
  arduino-libraries/ArduinoHttpClient @ ^0.6.0   ; (para HTTP en claro)
TLS/HTTPS: el fork usado no expone TinyGsmClientSecure. Para HTTPS evaluaremos:

migrar a TinyGSM oficial + TINY_GSM_USE_SSL, o

usar SSL nativo del SIMCom vía AT (pendiente de decisión).
````

🌐 Configuración de red
```
include/net_config.h:

cpp
Copiar
Editar
namespace netcfg {
  // WOM (Chile)
  inline const char* APN      = "internet";
  inline const char* APN_USER = "";
  inline const char* APN_PASS = "";

  // Endpoint (temporal de prueba o backend real)
  inline const char* HOST = "httpbin.org";  // cambiar cuando haya API
  inline const uint16_t  PORT = 80;         // 80 HTTP claro (temporal)
  inline const char* PATH = "/put";
  inline const char* AUTH_BEARER = "";      // ej. "Bearer <token>"
  inline const uint32_t PUT_PERIOD_MS = 5000;
}
````
▶️ Cómo compilar y probar
Conectar la placa, abrir VS Code → PlatformIO.

Verificar pines y APN en board_pins.h / net_config.h.

Build y Upload.

Abrir Monitor Serie @115200. Debe aparecer el log de arriba.

🔎 Utilidades
Conversión CSQ→dBm (puedes dejarla en main.cpp o utils.*):
```
cpp
Copiar
Editar
inline int csq_to_dbm(int csq) {
  if (csq <= 0)  return -113;
  if (csq == 1)  return -111;
  if (csq >= 31) return -51;
  return -113 + 2 * csq;
}

Forzar LTE only en boot:

AT+CNMP=38 y verificación con AT+CNMP? → +CNMP: 38

Persistencia: AT&W (si el firmware lo guarda).
````
🪜 Hoja de ruta (paso a paso)
````
Fase 1 — Base (listo)
Power‑on, LTE only, registro, PDP, prints de operador/RSSI/RAT/IP.

Fase 2 — HTTP PUT JSON (modo prueba)
Cliente HTTP sin TLS (puerto 80).

Enviar payload mínimo con {imei, operator, csq, rssi_dbm, ts_ms} a httpbin.org/put.

Reintentos + reconexión PDP si falla.

Fase 3 — TLS/HTTPS
Opción A: migrar a TinyGSM oficial + TINY_GSM_USE_SSL.

Opción B: SSL por AT (contextos TLS del SIMCom).

Cargar Root CA y validar certificado del backend.

Fase 4 — GNSS / GPS
Activar GNSS del A7670G o módulo externo.

Parsear NMEA, obtener lat/lon/alt/speed/hdop.

Enviar JSON {lat, lon, speed, hdop, t} en el mismo PUT.

Fase 5 — Robustez
Backoff exponencial en reintentos.

Watchdog + auto‑restart del módem en fallas.

Persistencia de colas offline (FS/SD) si no hay red.

Fase 6 — Backend real
Autenticación (Bearer o x-api-key).

Esquema estable de payload.

Versionado de API y métricas.
````
🧪 Diagnóstico rápido

¿No responde AT? Revisa EN (12), PWRKEY (4) y tiempos (settle ≥ 4000 ms).

RSSI=99: valor desconocido (acaba de registrar o poca señal). Esperar 1–2 min o mover antena.

Sin PDP: validar APN; reiniciar con gprsDisconnect() → gprsConnect(); revisar IP/antena.

Alimentación: imprescindible >2 A en picos.


