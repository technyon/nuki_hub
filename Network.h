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

    bool isMqttConnected();

    void publishKeyTurnerState(const char* state, const char* trigger, const char* completionStatus);
    void publishDoorSensorState(const char* state);
    void publishBatteryReport(const BatteryReport& batteryReport);

    void setLockActionReceived(void (*lockActionReceivedCallback)(const char* value));

private:
    static void onMqttDataReceivedCallback(char* topic, byte* payload, unsigned int length);
    void onMqttDataReceived(char*& topic, byte*& payload, unsigned int& length);

    void publishFloat(const char* topic, const float value, const uint8_t precision = 2);
    void publishInt(const char* topic, const int value);

    bool reconnect();

    PubSubClient _mqttClient;
    WiFiClient _wifiClient;
    Preferences* _preferences;

    bool _mqttConnected = false;

    unsigned long _nextReconnect = 0;
    char _mqttBrokerAddr[100] = {0};

    void (*_lockActionReceivedCallback)(const char* value) = NULL;
};
