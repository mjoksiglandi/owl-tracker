#include "comms_mode.h"

static CommsMode g_mode = CommsMode::AUTO;

void comms_mode_set(CommsMode m) { g_mode = m; }
CommsMode comms_mode_get()       { return g_mode; }

const char* comms_mode_name(CommsMode m) {
  switch (m) {
    case CommsMode::AUTO:        return "AUTO";
    case CommsMode::GSM_ONLY:    return "GSM_ONLY";
    case CommsMode::IRIDIUM_ONLY:return "IRIDIUM_ONLY";
    case CommsMode::BOTH:        return "BOTH";
  }
  return "AUTO";
}

CommsMode comms_mode_next(CommsMode m) {
  uint8_t v = static_cast<uint8_t>(m);
  v = (v + 1) % 4;
  return static_cast<CommsMode>(v);
}
