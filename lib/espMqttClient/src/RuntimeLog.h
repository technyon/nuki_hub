#pragma once

// Runtime-switchable logging for the ESP32 build of espMqttClient.
// emcLogEnabled turns the messages on, emcLogHandler receives each formatted
// line; when no handler is set the line goes to Serial. The handler runs on the
// task that logs, which can be the client's own task, so it must not call back
// into the client (for example by publishing the line over MQTT).

#include <Arduino.h>

extern bool emcLogEnabled;
extern void (*emcLogHandler)(const char* line);

void emcLogLine(char level, const char* format, ...);
