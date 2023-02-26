#pragma once

#include "MqttClientSetup.h"
#include "ClientSyncW5500.h"

class espMqttClientW5500 : public MqttClientSetup<espMqttClientW5500> {
public:
#if defined(ARDUINO_ARCH_ESP32)
    explicit espMqttClientW5500(uint8_t priority = 1, uint8_t core = 1);
#else
    espMqttClient();
#endif

    void update();

protected:
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
    espMqttClientInternals::ClientSyncW5500 _client;
#elif defined(__linux__)
    espMqttClientInternals::ClientPosix _client;
#endif
};
