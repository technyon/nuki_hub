#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <vector>
#include "NukiConstants.h"
#include "SpiffsCookie.h"

class Network
{
public:
    explicit Network(Preferences* preferences);
    virtual ~Network() = default;

    void initialize();
    void update();

    bool isMqttConnected();

    void publishKeyTurnerState(const Nuki::KeyTurnerState& keyTurnerState, const Nuki::KeyTurnerState& lastKeyTurnerState);
    void publishBatteryReport(const Nuki::BatteryReport& batteryReport);
    void publishConfig(const Nuki::Config& config);
    void publishPresenceDetection(char* csv);

    void setLockActionReceivedCallback(void (*lockActionReceivedCallback)(const char* value));
    void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char* path, const char* value));

    void restartAndConfigureWifi();

private:
    static void onMqttDataReceivedCallback(char* topic, byte* payload, unsigned int length);
    void onMqttDataReceived(char*& topic, byte*& payload, unsigned int& length);
    bool comparePrefixedPath(const char* fullPath, const char* subPath);

    void publishFloat(const char* topic, const float value, const uint8_t precision = 2);
    void publishInt(const char* topic, const int value);
    void publishBool(const char* topic, const bool value);
    void publishString(const char* topic, const char* value);

    void buildMqttPath(const char* path, char* outPath);
    void subscribe(const char* path);

    bool reconnect();

    PubSubClient _mqttClient;
    WiFiClient _wifiClient;
    Preferences* _preferences;
    SpiffsCookie _cookie;

    bool _mqttConnected = false;

    unsigned long _nextReconnect = 0;
    char _mqttBrokerAddr[101] = {0};
    char _mqttPath[181] = {0};
    char _mqttUser[31] = {0};
    char _mqttPass[31] = {0};

    char* _presenceCsv = nullptr;

    std::vector<char*> _configTopics;

    bool _firstTunerStatePublish = true;

    void (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* path, const char* value) = nullptr;
};
