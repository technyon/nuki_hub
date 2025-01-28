#pragma once
#include <Preferences.h>
#include <ArduinoJson.h>
#include "networkDevices/NetworkDevice.h"

class HomeAssistantDiscovery
{
public:
    explicit HomeAssistantDiscovery(NetworkDevice* device, Preferences* preferences, char* buffer, size_t bufferSize);
    void setupHASS(int type, uint32_t nukiId, char* nukiName, const char* firmwareVersion, const char* hardwareVersion, bool hasDoorSensor, bool hasKeypad);
    void disableHASS();
    void removeHassTopic(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString);
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
private:
    void publishHASSConfig(char *deviceType, const char *baseTopic, char *name, char *uidString, const char *softwareVersion, const char *hardwareVersion, const bool& hasDoorSensor, const bool& hasKeypad, const bool& publishAuthData, char *lockAction, char *unlockAction, char *openAction);
    void publishHASSDeviceConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const char *softwareVersion, const char *hardwareVersion, const char* availabilityTopic, const bool& hasKeypad, char* lockAction, char* unlockAction, char* openAction);
    void publishHASSNukiHubConfig();

    void publishHASSConfigAdditionalLockEntities(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigDoorSensor(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigAdditionalOpenerEntities(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigAccessLog(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigKeypad(char* deviceType, const char* baseTopic, char* name, char* uidString);
    void publishHASSConfigWifiRssi(char* deviceType, const char* baseTopic, char* name, char* uidString);


    void removeHASSConfig(char* uidString);
    void removeHASSConfigTopic(char* deviceType, char* name, char* uidString);

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

    NetworkDevice* _device = nullptr;
    Preferences* _preferences = nullptr;
    
    String _discoveryTopic;
    String _baseTopic;
    String _hostname;
    
    JsonDocument _uidToName;
    char _nukiHubUidString[20];
    
    bool _offEnabled = false;
    bool _checkUpdates = false;
    bool _updateFromMQTT = false;
    
    char* _buffer;
    const size_t _bufferSize;
};