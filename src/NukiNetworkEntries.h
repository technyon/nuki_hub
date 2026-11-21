#pragma once

#include "networkDevices/NetworkDevice.h"
#ifndef CONFIG_IDF_TARGET_ESP32H2
#include "networkDevices/WifiDevice.h"
#endif
#include <Preferences.h>
#include <vector>
#include <list>
#include "NukiConstants.h"
#include "NukiLockConstants.h"
#include "NukiNetwork.h"
#include "QueryCommand.h"
#include "LockActionResult.h"
#include "NukiOfficial.h"
#include "NukiPublisher.h"
#include "EspMillis.h"
#include "DoorSensorOverride.h"

class NukiNetworkEntries
{
public:
    void publishKeypad(const std::list<Nuki::KeypadEntry>& entries, uint maxKeypadCodeCount);
    void publishAuth(const std::list<Nuki::AuthorizationEntry>& authEntries, uint maxAuthEntryCount);
    virtual void publishKeypadCommandResult(const char* result) = 0;
    virtual void publishKeypadJsonCommandResult(const char* result) = 0;
    virtual void publishAuthCommandResult(const char* result) = 0;

protected:
    explicit NukiNetworkEntries(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize, const char* idPreference, const char* topicSuffix, const char* deviceType);

    void publishKeypadEntry(const String topic, Nuki::KeypadEntry entry);
    String concat(String a, String b);

    NukiNetwork* _network = nullptr;
    NukiPublisher* _nukiPublisher = nullptr;
    Preferences* _preferences = nullptr;

    std::map<uint32_t, String> _authEntries;
    char _mqttPath[200] = {0};

    bool _disableNonJSON = false;
    bool _clearNonJsonKeypad = true;
    bool _clearKeypadSlotEntities = true;
    uint _publishedKeypadCount = 0;
    std::vector<std::string> _keypadHaSlots;
    bool _clearAuthSlotEntities = true;
    std::vector<std::string> _authHaSlots;

    char _nukiName[33];

    char* _buffer;
    size_t _bufferSize;

private:
    const char* _idPreference;
    const char* _topicSuffix;
    const char* _deviceType;
};
