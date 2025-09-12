#include "inbox.h"

static const uint16_t CAP = 32;
static InboxItem ring[CAP];
static uint16_t  head = 0;   // apunta al próximo a escribir
static uint16_t  size_ = 0;

void inbox_clear() { head = 0; size_ = 0; }

void inbox_push(const char* src, const String& data) {
  ring[head].ts   = millis();
  ring[head].src  = src;
  ring[head].data = data;
  head = (head + 1) % CAP;
  if (size_ < CAP) size_++;
}

uint16_t inbox_count() { return size_; }

bool inbox_get(uint16_t idx, InboxItem& out) {
  if (idx >= size_) return false;
  int pos = (int)head - 1 - (int)idx;
  if (pos < 0) pos += CAP;
  out = ring[pos];
  return true;
}
