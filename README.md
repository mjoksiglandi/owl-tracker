#🦉 Owl Tracker

Proyecto de telemetría IoT basado en ESP32 + SIMCom A7670G (Cat-1/LTE) sobre PlatformIO.
Objetivo: conectar a red celular, obtener estado del módem (operador/RSSI/RAT), mostrar datos en un OLED SH1122 256×64 y enviar payload JSON a un backend HTTP/MQTT.


##✅ Estado actual (hito base)

Secuencia de power-on del A7670G (EN=12, PWRKEY=4).

Forzado de LTE only (AT+CNMP=38) con verificación y persistencia (AT&W).

Registro de red (+ prints de operador / RSSI / RAT).

PDP activo (APN WOM: internet) y IP obtenida.

Interfaz gráfica en OLED SH1122 256×64:

Icono de antena + barras RSSI (0–5).

Estado de PDP: muestra IP o “SIN PDP” invertido.

Icono 🌍 + barras finas (nivel Iridium, reservado a futuro).

Lat/Lon centrados.

🛰 SAT/PDOP a la izquierda.

Fecha/hora UTC en el centro inferior.

Contador de mensajes recibidos a la derecha.

Salida típica por monitor serie:
````
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
````

## 🧱 Estructura del proyecto
````
Owl/
├─ include/ # Headers (.h)
│ └─ board_pins.h, config.h, net_config.h
├─ src/
│ ├─ main.cpp             # Punto de entrada (stages)
│ ├─ modem_config.*       # Encendido y configuración RAT + registro
│ ├─ http_client.*        # Cliente HTTP (modo prueba)
│ └─ oled_display.*       # Driver + UI OLED SH1122 256×64 
├─ lib/                   # Librerías locales (opcional)
├─ platformio.ini
└─ README.md


````

## 🔌 Hardware
````
- Placa: **LILYGO T-A7670G** (ESP32-WROVER-E + SIMCom A7670G).
- Pines usados (revisión R2):
  - MODEM_TX (ESP32→A7670 RX): 26
  - MODEM_RX (ESP32←A7670 TX): 27
  - MODEM_PWR (PWRKEY): 4
  - MODEM_EN: 12
- Antena LTE conectada y SIM nano (sin PIN).
- Alimentación robusta (>2 A picos).

- OLED: **SH1122 256×64 SPI**
  - SCK = 18
  - MOSI = 23
  - CS = 5
  - DC = 21
  - RST = 22
  - VCC = 3V3, GND común


````

## ⚙️ Setup (PlatformIO)

`platformio.ini` (fragmento clave):

```
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
  arduino-libraries/ArduinoHttpClient @ ^0.6.0
  olikraus/U8g2 @ ^2.35.8

````

🌐 Configuración de red
```
include/net_config.h:

namespace netcfg {
  // WOM (Chile)
  inline const char* APN      = "internet";
  inline const char* APN_USER = "";
  inline const char* APN_PASS = "";

  // Endpoint (temporal de prueba o backend real)
  inline const char* HOST = "httpbin.org";
  inline const uint16_t  PORT = 80;
  inline const char* PATH = "/put";
  inline const char* AUTH_BEARER = "";
  inline const uint32_t PUT_PERIOD_MS = 5000;
}

````
▶️ Cómo compilar y probar

Conectar la placa y abrir VS Code → PlatformIO.

Verificar pines y APN en board_pins.h / net_config.h.

Build + Upload.

Abrir Monitor Serie @115200 → ver logs.

El OLED mostrará la UI estilo “celular” con datos básicos.

🔎 Utilidades
Conversión CSQ→dBm (puedes dejarla en main.cpp o utils.*):
```
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
Power-on, LTE only, registro, PDP, UI OLED inicial.

Fase 2 — HTTP PUT JSON (modo prueba)
Cliente HTTP sin TLS (puerto 80).
Enviar payload mínimo con {imei, operator, csq, rssi_dbm, ts_ms}.

Fase 3 — TLS/HTTPS
Migrar a TinyGSM oficial con SSL o usar AT TLS nativo.

Fase 4 — GNSS / GPS
Integrar GNSS externo (ej. NEO-M9N).
Mostrar satélites/PDOP en OLED + enviar en payload.

Fase 5 — Robustez
Reintentos, watchdog, colas offline (FS/SD).

Fase 6 — Iridium (futuro)
Módulo de respaldo satelital (nivel señal en OLED 🌍).

Fase 7 — Backend real
Autenticación (Bearer o x-api-key), payload definitivo.

````
🧪 Diagnóstico rápido

¿No responde AT? → Revisa EN (12), PWRKEY (4), settle ≥ 4000 ms.

RSSI=99 → valor desconocido, esperar registro o mover antena.

“SIN PDP” en OLED → revisar APN, reconectar con gprsDisconnect() + gprsConnect().

OLED sin imagen → verificar pines, VCC=3.3 V, contraste en oled_init().

Alimentación: imprescindible >2 A en picos.


