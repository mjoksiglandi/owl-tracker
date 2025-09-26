#include "inbox.h"

namespace inbox {

static Msg*               g_buf      = nullptr;
static uint8_t            g_cap      = 0;
static volatile uint16_t  g_count    = 0;    // elementos válidos en buffer
static uint16_t           g_head     = 0;    // próxima posición de escritura
static SemaphoreHandle_t  g_mtx      = nullptr;

static inline void lock()   { if (g_mtx) xSemaphoreTake(g_mtx, portMAX_DELAY); }
static inline void unlock() { if (g_mtx) xSemaphoreGive(g_mtx); }

void begin(uint8_t capacity) {
  if (capacity == 0) capacity = 8;
  if (g_buf) { delete[] g_buf; g_buf = nullptr; }
  g_buf = new Msg[capacity];
  g_cap = capacity;
  g_count = 0;
  g_head = 0;
  if (!g_mtx) g_mtx = xSemaphoreCreateMutex();
}

void clear() {
  lock();
  g_count = 0;
  g_head = 0;
  unlock();
}

static inline uint16_t idx_last() {
  if (g_count == 0) return 0;
  return (uint16_t)((g_head + g_cap - 1) % g_cap);
}

bool push(const char* source, const String& body) {
  if (!g_buf || g_cap == 0) return false;
  lock();

  // escribir en head
  uint16_t i = g_head;
  g_buf[i].ts_ms = millis();
  strncpy(g_buf[i].source, source ? source : "UNK", sizeof(g_buf[i].source) - 1);
  g_buf[i].source[sizeof(g_buf[i].source) - 1] = '\0';
  g_buf[i].body = body;
  g_buf[i].unread = true;

  // avanzar head
  g_head = (uint16_t)((g_head + 1) % g_cap);
  if (g_count < g_cap) {
    g_count++;
  } else {
    // sobrescritura: si llenamos, el más antiguo se pierde
    // (nada adicional que hacer)
  }

  unlock();
  return true;
}

uint16_t size() {
  return g_count;
}

uint16_t unread_count() {
  if (!g_buf || g_count == 0) return 0;
  lock();
  uint16_t n = 0;
  for (uint16_t k = 0; k < g_count; ++k) {
    uint16_t idx = (uint16_t)((g_head + g_cap - 1 - k) % g_cap);
    if (g_buf[idx].unread) n++;
  }
  unlock();
  return n;
}

bool peek_last(Msg& out) {
  if (!g_buf || g_count == 0) return false;
  lock();
  out = g_buf[idx_last()];
  unlock();
  return true;
}

String last_body() {
  if (!g_buf || g_count == 0) return String("");
  lock();
  String s = g_buf[idx_last()].body;
  unlock();
  return s;
}

void mark_all_read() {
  if (!g_buf || g_count == 0) return;
  lock();
  for (uint16_t k = 0; k < g_count; ++k) {
    uint16_t idx = (uint16_t)((g_head + g_cap - 1 - k) % g_cap);
    g_buf[idx].unread = false;
  }
  unlock();
}

void mark_last_read() {
  if (!g_buf || g_count == 0) return;
  lock();
  g_buf[idx_last()].unread = false;
  unlock();
}

void dump_to_serial() {
  if (!g_buf || g_count == 0) { Serial.println("[INBOX] vacía"); return; }
  lock();
  Serial.printf("[INBOX] últimos %u (cap=%u):\n", g_count, g_cap);
  for (uint16_t k = 0; k < g_count; ++k) {
    uint16_t idx = (uint16_t)((g_head + g_cap - 1 - k) % g_cap);
    Serial.printf("  [%02u] t=%lu src=%s unread=%d body=%s\n",
                  k, (unsigned long)g_buf[idx].ts_ms, g_buf[idx].source,
                  g_buf[idx].unread ? 1 : 0,
                  g_buf[idx].body.c_str());
  }
  unlock();
}

} // namespace inbox
