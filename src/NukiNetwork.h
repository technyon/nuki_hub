#pragma once

#include <Preferences.h>
#include <vector>
#include <map>
#include "networkDevices/NetworkDevice.h"
#include "networkDevices/IPConfiguration.h"
#include "enums/NetworkDeviceType.h"
#include "util/NetworkUtil.h"

#ifndef NUKI_HUB_UPDATER
#include "MqttReceiver.h"
#include "mqtt_client.h"
#include "MqttTopics.h"
#include "Gpio.h"
#include <ArduinoJson.h>
#include "NukiConstants.h"
#endif

class NukiNetwork
{
public:
    void initialize();
    void readSettings();
    bool update();
    void reconfigureDevice();
    void clearWifiFallback();

    const String networkDeviceName() const;
    const String networkBSSID() const;
    const NetworkDeviceType networkDeviceType();
    void setKeepAliveCallback(std::function<void()> reconnectTick);

    NetworkDevice* device();

    #ifdef NUKI_HUB_UPDATER
    explicit NukiNetwork(Preferences* preferences);
    #else
    explicit NukiNetwork(Preferences* preferences, Gpio* gpio, const String& maintenancePathPrefix, char* buffer, size_t bufferSize);

    void registerMqttReceiver(MqttReceiver* receiver);
    void disableAutoRestarts(); // disable on OTA start
    void disableMqtt();
    String localIP();
    bool isConnected();

    void subscribe(const char* prefix, const char* path);
    void initTopic(const char* prefix, const char* path, const char* value);
    void publishFloat(const char* prefix, const char* topic, const float value, bool retain, const uint8_t precision = 2);
    void publishInt(const char* prefix, const char* topic, const int value, bool retain);
    void publishUInt(const char* prefix, const char* topic, const unsigned int value, bool retain);
    void publishULong(const char* prefix, const char* topic, const unsigned long value, bool retain);
    void publishLongLong(const char* prefix, const char* topic, int64_t value, bool retain);
    void publishBool(const char* prefix, const char* topic, const bool value, bool retain);
    bool publishString(const char* prefix, const char* topic, const char* value, bool retain);

    void publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const char *softwareVersion, const char *hardwareVersion, const char* availabilityTopic, const bool& hasKeypad, char* lockAction, char* unlockAction, char* openAction);
    void publishHASSConfigAdditionalLockEntities(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigDoorSensor(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigAdditionalOpenerEntities(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigAccessLog(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigKeypad(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSWifiRssiConfig(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void removeHASSConfig(char* uidString);
    void removeHASSConfigTopic(char* deviceType, char* name, char* uidString);
    void publishHassTopic(const String& mqttDeviceType,
                          const String& mqttDeviceName,
                          const String& uidString,
                          const String& uidStringPostfix,
                          const String& displayName,
                          const String& name,
                          const String& baseTopic,
                          const String& stateTopic,
                          const String& deviceType,
                          const String& deviceClass,
                          const String& stateClass = "",
                          const String& entityCat = "",
                          const String& commandTopic = "",
                          std::vector<std::pair<char*, char*>> additionalEntries = {}
                          );
    void removeHassTopic(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString);
    void removeTopic(const String& mqttPath, const String& mqttTopic);
    void batteryTypeToString(const Nuki::BatteryType battype, char* str);
    void advertisingModeToString(const Nuki::AdvertisingMode advmode, char* str);
    void timeZoneIdToString(const Nuki::TimeZoneId timeZoneId, char* str);

    int mqttConnectionState(); // 0 = not connected; 1 = connected; 2 = connected and mqtt processed
    bool mqttRecentlyConnected();
    bool pathEquals(const char* prefix, const char* path, const char* referencePath);
    uint16_t subscribe(const char* topic, uint8_t qos);

    void addReconnectedCallback(std::function<void()> reconnectedCallback);
    #endif
private:
    void setupDevice();
    bool reconnect();

    static NukiNetwork* _inst;

    const char* _latestVersion;

    Preferences* _preferences;
    IPConfiguration* _ipConfiguration = nullptr;
    String _hostname;
    char _hostnameArr[101] = {0};
    NetworkDevice* _device = nullptr;

    std::function<void()> _keepAliveCallback = nullptr;
    std::vector<std::function<void()>> _reconnectedCallbacks;

    NetworkDeviceType _networkDeviceType  = (NetworkDeviceType)-1;
    bool _firstBootAfterDeviceChange = false;
    bool _webEnabled = true;
    bool _updateFromMQTT = false;
    bool _offEnabled = false;

    #ifndef NUKI_HUB_UPDATER
    static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void onMqttDataReceived(const char* topic, const uint8_t* payload, size_t& len);
    void parseGpioTopics(const char* topic, const uint8_t* payload, size_t& len);
    void gpioActionCallback(const GpioAction& action, const int& pin);

    String createHassTopicPath(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString);
    JsonDocument createHassJson(const String& uidString,
                        const String& uidStringPostfix,
                        const String& displayName,
                        const String& name,
                        const String& baseTopic,
                        const String& stateTopic,
                        const String& deviceType,
                        const String& deviceClass,
                        const String& stateClass = "",
                        const String& entityCat = "",
                        const String& commandTopic = "",
                        std::vector<std::pair<char*, char*>> additionalEntries = {}
                        );
    void buildMqttPath(char* outPath, std::initializer_list<const char*> paths);

    const char* _lastWillPayload = "offline";
    char _mqttConnectionStateTopic[211] = {0};
    String _lockPath;
    String _discoveryTopic;

    Gpio* _gpio;
  
    int _mqttConnectionState = 0;
    bool _mqttConnected = false;
    int _mqttConnectCounter = 0;
    int _mqttPort = 1883;
    long _mqttConnectedTs = -1;
    bool _firstDisconnected = true;

    esp_mqtt_client_handle_t _mqttClient;
    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
    
    int64_t _nextReconnect = 0;
    char _mqttBrokerAddr[101] = {0};
    char _mqttUser[31] = {0};
    char _mqttPass[31] = {0};
    char _mqttPresencePrefix[181] = {0};
    char _maintenancePathPrefix[181] = {0};
    int _networkTimeout = 0;
    std::vector<MqttReceiver*> _mqttReceivers;
    bool _restartOnDisconnect = false;
    bool _checkUpdates = false;
    bool _reconnectNetworkOnMqttDisconnect = false;
    bool _firstConnect = true;
    bool _publishDebugInfo = false;
    bool _logIp = true;
    std::vector<String> _subscribedTopics;
    std::map<String, String> _initTopics;
    int64_t _lastConnectedTs = 0;
    int64_t _lastMaintenanceTs = 0;
    int64_t _lastUpdateCheckTs = 0;
    int64_t _lastRssiTs = 0;
    bool _mqttEnabled = true;
    int _rssiPublishInterval = 0;
    std::map<uint8_t, int64_t> _gpioTs;

    char* _buffer;
    const size_t _bufferSize;

    int8_t _lastRssi = 127;
    #endif
};