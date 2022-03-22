#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>

class Network
{
public:
    Network();

    void initialize();
    void update();

private:
    bool reconnect();

    PubSubClient _mqttClient;
    WiFiClient _wifiClient;

    uint32_t _count = 0;
    unsigned long _publishTs = 0;
};
