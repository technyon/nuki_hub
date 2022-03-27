#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include "NukiConstants.h"

class Network
{
public:
    explicit Network(Preferences* preferences);
    virtual ~Network() = default;

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
    Preferences* _preferences;

    unsigned long _nextReconnect = 0;
    char _mqttBrokerAddr[100] = {0};

    void (*_lockActionReceivedCallback)(const char* value) = NULL;
};
