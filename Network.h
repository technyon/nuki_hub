#pragma once

#include <Preferences.h>
#include <vector>
#include <map>
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
    explicit Network(Preferences* preferences, const String& maintenancePathPrefix);

    void initialize();
    bool update();
    void registerMqttReceiver(MqttReceiver* receiver);
    void reconfigureDevice();
    void setMqttPresencePath(char* path);
    void disableAutoRestarts(); // disable on OTA start

    void subscribe(const char* prefix, const char* path);
    void initTopic(const char* prefix, const char* path, const char* value);
    void publishFloat(const char* prefix, const char* topic, const float value, const uint8_t precision = 2);
    void publishInt(const char* prefix, const char* topic, const int value);
    void publishUInt(const char* prefix, const char* topic, const unsigned int value);
    void publishULong(const char* prefix, const char* topic, const unsigned long value);
    void publishBool(const char* prefix, const char* topic, const bool value);
    bool publishString(const char* prefix, const char* topic, const char* value);

    void publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState);
    void publishHASSConfigBatLevel(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState);
    void publishHASSConfigDoorSensor(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState);
    void publishHASSConfigRingDetect(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState);
    void publishHASSWifiRssiConfig(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSBleRssiConfig(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void removeHASSConfig(char* uidString);

    void publishPresenceDetection(char* csv);

    MqttClient* mqttClient();
    int mqttConnectionState(); // 0 = not connected; 1 = connected; 2 = connected and mqtt processed

    const NetworkDeviceType networkDeviceType();

private:
    static void onMqttDataReceivedCallback(int);
    void onMqttDataReceived(int messageSize);
    void setupDevice();
    bool reconnect();

    void buildMqttPath(const char* prefix, const char* path, char* outPath);

    static Network* _inst;
    Preferences* _preferences;
    String _hostname;
    NetworkDevice* _device = nullptr;
    int _mqttConnectionState = 0;

    unsigned long _nextReconnect = 0;
    char _mqttBrokerAddr[101] = {0};
    char _mqttUser[31] = {0};
    char _mqttPass[31] = {0};
    char _mqttPresencePrefix[181] = {0};
    char _maintenancePathPrefix[181] = {0};
    int _networkTimeout = 0;
    std::vector<MqttReceiver*> _mqttReceivers;
    char* _presenceCsv = nullptr;
    bool _restartOnDisconnect = false;
    bool _firstConnect = true;
    std::vector<String> _subscribedTopics;
    std::map<String, String> _initTopics;

    unsigned long _lastConnectedTs = 0;
    unsigned long _lastMaintenanceTs = 0;
    unsigned long _lastRssiTs = 0;
    long _rssiPublishInterval = 0;

    NetworkDeviceType _networkDeviceType  = (NetworkDeviceType)-1;

    int8_t _lastRssi = 127;
};
