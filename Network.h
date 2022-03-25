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
    void publishBatteryVoltage(const float& value);

    void setLockActionReceived(void (*lockActionReceivedCallback)(const char* value));

private:
    static void onMqttDataReceivedCallback(char* topic, byte* payload, unsigned int length);
    void onMqttDataReceived(char*& topic, byte*& payload, unsigned int& length);

    bool reconnect();

    PubSubClient _mqttClient;
    WiFiClient _wifiClient;

    void (*_lockActionReceivedCallback)(const char* value) = NULL;
};
