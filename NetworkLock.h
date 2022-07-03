#pragma once

#include <PubSubClient.h>
#include "networkDevices/NetworkDevice.h"
#include "networkDevices/WifiDevice.h"
#include "networkDevices/W5500Device.h"
#include <Preferences.h>
#include <vector>
#include "NukiConstants.h"
#include "SpiffsCookie.h"
#include "NukiLockConstants.h"
#include "Network.h"

class NetworkLock : public MqttReceiver
{
public:
    explicit NetworkLock(Network* network, Preferences* preferences);
    virtual ~NetworkLock();

    void initialize();

    void publishKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState, const NukiLock::KeyTurnerState& lastKeyTurnerState);
    void publishAuthorizationInfo(const uint32_t authId, const char* authName);
    void publishCommandResult(const char* resultStr);
    void publishBatteryReport(const NukiLock::BatteryReport& batteryReport);
    void publishConfig(const NukiLock::Config& config);
    void publishAdvancedConfig(const NukiLock::AdvancedConfig& config);
    void publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState);
    void removeHASSConfig(char* uidString);

    void setLockActionReceivedCallback(bool (*lockActionReceivedCallback)(const char* value));
    void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char* path, const char* value));

    void onMqttDataReceived(char*& topic, byte*& payload, unsigned int& length) override;

private:
    void publishFloat(const char* topic, const float value, const uint8_t precision = 2);
    void publishInt(const char* topic, const int value);
    void publishUInt(const char* topic, const unsigned int value);
    void publishBool(const char* topic, const bool value);
    bool publishString(const char* topic, const char* value);
    bool comparePrefixedPath(const char* fullPath, const char* subPath);

    void buildMqttPath(const char* path, char* outPath);

    Network* _network;
    Preferences* _preferences;

    std::vector<char*> _configTopics;
    char _mqttPath[181] = {0};

    bool _firstTunerStatePublish = true;

    bool (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* path, const char* value) = nullptr;
};
