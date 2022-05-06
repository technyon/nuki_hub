#pragma once

#include <PubSubClient.h>
#include "networkDevices/NetworkDevice.h"
#include "networkDevices/WifiDevice.h"
#include "networkDevices/W5500Device.h"
#include <Preferences.h>
#include <vector>
#include "NukiConstants.h"
#include "SpiffsCookie.h"

enum class NetworkDeviceType
{
    WiFi,
    W5500
};

class Network
{
public:
    explicit Network(const NetworkDeviceType networkDevice, Preferences* preferences);
    virtual ~Network();

    void initialize();
    void update();
    void setupDevice(const NetworkDeviceType hardware);
    void initializeW5500();

    bool isMqttConnected();

    void publishKeyTurnerState(const Nuki::KeyTurnerState& keyTurnerState, const Nuki::KeyTurnerState& lastKeyTurnerState);
    void publishAuthorizationInfo(const uint32_t authId, const char* authName);
    void publishCommandResult(const char* resultStr);
    void publishBatteryReport(const Nuki::BatteryReport& batteryReport);
    void publishConfig(const Nuki::Config& config);
    void publishAdvancedConfig(const Nuki::AdvancedConfig& config);
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
    void publishUInt(const char* topic, const unsigned int value);
    void publishBool(const char* topic, const bool value);
    void publishString(const char* topic, const char* value);

    void buildMqttPath(const char* path, char* outPath);
    void subscribe(const char* path);

    bool reconnect();

    NetworkDevice* _device = nullptr;
    Preferences* _preferences;
    String _hostname;
    NetworkDeviceType _networkDeviceType;

    bool _mqttConnected = false;

    unsigned long _nextReconnect = 0;
    char _mqttBrokerAddr[101] = {0};
    char _mqttPath[181] = {0};
    char _mqttUser[31] = {0};
    char _mqttPass[31] = {0};

    char* _presenceCsv = nullptr;

    std::vector<char*> _configTopics;

    bool _firstTunerStatePublish = true;

    long _lastMaintain = 0;

    void (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* path, const char* value) = nullptr;
};
