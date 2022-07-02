#pragma once

#include <Preferences.h>
#include <vector>
#include "networkDevices/NetworkDevice.h"
#include "MqttReceiver.h"

enum class NetworkDeviceType
{
    WiFi,
    W5500
};

class Network
{
public:
    explicit Network(const NetworkDeviceType networkDevice, Preferences* preferences);

    void initialize();
    int update();
    void registerMqttReceiver(MqttReceiver* receiver);
    void reconfigureDevice();

    void subscribe(const char* prefix, const char* path);
    void publishFloat(const char* prefix, const char* topic, const float value, const uint8_t precision = 2);
    void publishInt(const char* prefix, const char* topic, const int value);
    void publishUInt(const char* prefix, const char* topic, const unsigned int value);
    void publishBool(const char* prefix, const char* topic, const bool value);
    bool publishString(const char* prefix, const char* topic, const char* value);

    void publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState);
    void removeHASSConfig(char* uidString);

    PubSubClient* mqttClient();
    bool isMqttConnected();

private:
    static void onMqttDataReceivedCallback(char* topic, byte* payload, unsigned int length);
    void onMqttDataReceived(char*& topic, byte*& payload, unsigned int& length);
    void setupDevice(const NetworkDeviceType hardware);
    bool reconnect();

    void buildMqttPath(const char* prefix, const char* path, char* outPath);

    static Network* _inst;
    Preferences* _preferences;
    String _hostname;
    NetworkDevice* _device = nullptr;
    bool _mqttConnected = false;

    unsigned long _nextReconnect = 0;
    char _mqttBrokerAddr[101] = {0};
    char _mqttUser[31] = {0};
    char _mqttPass[31] = {0};
    std::vector<String> _subscribedTopics;
    int _networkTimeout = 0;
    std::vector<MqttReceiver*> _mqttReceivers;

    unsigned long _lastConnectedTs = 0;
};
