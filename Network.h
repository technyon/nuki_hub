#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>
#include "NukiConstants.h"

class Network
{
public:
    Network();

    void initialize();
    void update();

    void publishKeyTurnerState(const char* state);

private:
    static void onMqttDataReceivedCallback(char* topic, byte* payload, unsigned int length);
    void onMqttDataReceived(char*& topic, byte*& payload, unsigned int& length);

    bool reconnect();

    PubSubClient _mqttClient;
    WiFiClient _wifiClient;

    unsigned long _publishTs = 0;
};
