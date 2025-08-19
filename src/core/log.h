#pragma once
#include <Arduino.h>

// Tag por defecto; los .cpp pueden redefinirlo antes de incluir este header
#ifndef LOG_TAG
#define LOG_TAG "APP"
#endif

// Implementación en log.cpp (no usamos Serial aquí)
void log_printf_(char level, const char* tag, const char* fmt, ...);

// Macros de uso
#define LOGI(fmt, ...) log_printf_('I', LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) log_printf_('W', LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) log_printf_('E', LOG_TAG, fmt, ##__VA_ARGS__)
