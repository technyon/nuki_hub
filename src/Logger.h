#ifndef NUKI_HUB_UPDATER
#ifndef MQTT_LOGGER_GLOBAL
#define MQTT_LOGGER_GLOBAL

#include "MqttLogger.h"
extern Print* Log;

#endif
#else
#include <Print.h>
extern Print* Log;
#endif