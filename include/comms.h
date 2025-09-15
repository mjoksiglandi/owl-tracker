#pragma once
#include <Arduino.h>
#include "ble.h"
#include "http_client.h"
#include "iridium.h"
#include "crypto.h"
#include "inbox.h"
#include "report.h"   // TU OwlReport

// Modo de operación para pruebas
enum class CommsMode : uint8_t {
  AUTO = 0,   // GSM prioritario; fallback Iridium; intenta volver a GSM
  GSM_ONLY,
  IR_ONLY,
  BOTH
};

struct CommsStatus {
  bool gsm_ready = false;  // PDP OK
  bool ir_present = false;
  int  ir_rssi   = -1;     // 0..5
  bool ble_link  = false;
  uint16_t inbox = 0;
  CommsMode mode = CommsMode::AUTO;
};

void        comms_begin();
void        comms_set_mode(CommsMode m);
CommsMode   comms_get_mode();
bool        comms_send_report(const OwlReport& rpt);
void        comms_poll();
CommsStatus comms_status();

// helpers
String make_report_json(const OwlReport& rpt);
String make_report_cipher_b64(const OwlReport& rpt);
