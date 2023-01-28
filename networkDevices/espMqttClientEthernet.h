#pragma once

#include "MqttClientSetup.h"
#include "ClientSyncEthernet.h"

class espMqttClientEthernet : public MqttClientSetup<espMqttClientEthernet> {
public:
#if defined(ARDUINO_ARCH_ESP32)
    explicit espMqttClientEthernet(uint8_t priority = 1, uint8_t core = 1);
#else
    espMqttClient();
#endif

protected:
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
    espMqttClientInternals::ClientSyncEthernet _client;
#elif defined(__linux__)
    espMqttClientInternals::ClientPosix _client;
#endif
};
