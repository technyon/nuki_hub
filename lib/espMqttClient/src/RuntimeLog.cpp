#if defined(ARDUINO_ARCH_ESP32)

#include "RuntimeLog.h"
#include <stdarg.h>
#include <stdio.h>

bool emcLogEnabled = false;
void (*emcLogHandler)(const char* line) = nullptr;

void emcLogLine(char level, const char* format, ...) {
  char line[160];
  int len = snprintf(line, sizeof(line), "[espMqttClient %c] ", level);
  if (len < 0 || len >= (int)sizeof(line)) {
    return;
  }
  va_list args;
  va_start(args, format);
  vsnprintf(line + len, sizeof(line) - len, format, args);
  va_end(args);
  if (emcLogHandler != nullptr) {
    emcLogHandler(line);
  } else {
    Serial.println(line);
  }
}

#endif
