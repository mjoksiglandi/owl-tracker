#pragma once
#include <Arduino.h>
#include "report.h"  // define OwlReport

bool ble_begin(const char* devName = nullptr);
bool ble_is_connected();

/** Envío directo: AHORA publica JSON EN CLARO por BLE (sin cifrar) */
bool ble_notify_report_json(const String& jsonPlain);

/** Compatibilidad con tu main: */
void  ble_poll();                          // no-op
bool  ble_update(const String& jsonPlain); // alias a notify
bool  ble_update(const OwlReport& rpt);    // build + publish (en claro)

void  ble_set_info(const String& info);
void  ble_set_name(const char* newName);
