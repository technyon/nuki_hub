#pragma once

#include "networkDevices/NetworkDevice.h"
#include "networkDevices/WifiDevice.h"
#include "networkDevices/W5500Device.h"
#include <Preferences.h>
#include <vector>
#include <list>
#include "NukiConstants.h"
#include "NukiLockConstants.h"
#include "NukiNetwork.h"
#include "QueryCommand.h"
#include "LockActionResult.h"

#define LOCK_LOG_JSON_BUFFER_SIZE 2048

class NukiNetworkLock : public MqttReceiver
{
public:
    explicit NukiNetworkLock(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize);
    virtual ~NukiNetworkLock();

    void initialize();

    void publishKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState, const NukiLock::KeyTurnerState& lastKeyTurnerState);
    void publishState(NukiLock::LockState lockState);
    void publishAuthorizationInfo(const std::list<NukiLock::LogEntry>& logEntries, bool latest);
    void clearAuthorizationInfo();
    void publishCommandResult(const char* resultStr);
    void publishLockstateCommandResult(const char* resultStr);
    void publishBatteryReport(const NukiLock::BatteryReport& batteryReport);
    void publishConfig(const NukiLock::Config& config);
    void publishAdvancedConfig(const NukiLock::AdvancedConfig& config);
    void publishRssi(const int& rssi);
    void publishRetry(const std::string& message);
    void publishBleAddress(const std::string& address);
    void publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const char *softwareVersion, const char *hardwareVersion, const bool& hasDoorSensor, const bool& hasKeypad, const bool& publishAuthData, char* lockAction, char* unlockAction, char* openAction);
    void removeHASSConfig(char* uidString);
    void publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount);
    void publishTimeControl(const std::list<NukiLock::TimeControlEntry>& timeControlEntries, uint maxTimeControlEntryCount);
    void publishStatusUpdated(const bool statusUpdated);
    void publishConfigCommandResult(const char* result);
    void publishKeypadCommandResult(const char* result);
    void publishKeypadJsonCommandResult(const char* result);
    void publishTimeControlCommandResult(const char* result);
    void publishOffAction(const int value);

    void setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char* value));
    void setOfficialUpdateReceivedCallback(void (*officialUpdateReceivedCallback)(const char* path, const char* value));
    void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char* value));
    void setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled));
    void setKeypadJsonCommandReceivedCallback(void (*keypadJsonCommandReceivedReceivedCallback)(const char* value));
    void setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char* value));
    void onMqttDataReceived(const char* topic, byte* payload, const unsigned int length) override;

    void publishFloat(const char* topic, const float value, bool retain, const uint8_t precision = 2);
    void publishInt(const char* topic, const int value, bool retain);
    void publishUInt(const char* topic, const unsigned int value, bool retain);
    void publishULong(const char* topic, const unsigned long value, bool retain);
    void publishLongLong(const char* topic, int64_t value, bool retain);    
    void publishBool(const char* topic, const bool value, bool retain);
    bool publishString(const char* topic, const String& value, bool retain);
    bool publishString(const char* topic, const std::string& value, bool retain);
    bool publishString(const char* topic, const char* value, bool retain);

    bool reconnected();
    uint8_t queryCommands();
    //uint8_t _offMode = 0;
    uint8_t _offState = 0;
    bool _offCritical = false;
    uint8_t _offChargeState = 100;
    bool _offCharging = false;
    bool _offKeypadCritical = false;
    uint8_t _offDoorsensorState = 0;
    bool _offDoorsensorCritical = false;
    bool _offConnected = false;
    uint8_t _offCommandResponse = 0;
    char* _offLockActionEvent;
    uint8_t _offLockAction = 0;
    uint8_t _offTrigger = 0;
    uint32_t _offAuthId = 0;
    uint32_t _offCodeId = 0;
    uint8_t _offContext = 0;
    uint32_t _authId = 0;
    int64_t _offCommandExecutedTs = 0;
    NukiLock::LockAction _offCommand = (NukiLock::LockAction)0xff;
    char _nukiName[33];
    char _authName[33];
    bool _authFound = false;

private:
    bool comparePrefixedPath(const char* fullPath, const char* subPath, bool offPath = false);

    void publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry);
    void buttonPressActionToString(const NukiLock::ButtonPressAction btnPressAction, char* str);
    void homeKitStatusToString(const int hkstatus, char* str);
    void fobActionToString(const int fobact, char* str);

    String concat(String a, String b);

    void buildMqttPath(const char* path, char* outPath, bool offPath = false);

    NukiNetwork* _network;
    Preferences* _preferences;

    std::vector<char*> _offTopics;
    char _mqttPath[181] = {0};
    char _offMqttPath[181] = {0};

    bool _firstTunerStatePublish = true;
    int64_t _lastMaintenanceTs = 0;
    bool _haEnabled = false;
    bool _reconnected = false;

    String _keypadCommandName = "";
    String _keypadCommandCode = "";
    uint _keypadCommandId = 0;
    int _keypadCommandEnabled = 1;
    uint8_t _queryCommands = 0;
    uint32_t _lastRollingLog = 0;

    char* _buffer;
    size_t _bufferSize;

    LockActionResult (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_officialUpdateReceivedCallback)(const char* path, const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* value) = nullptr;
    void (*_keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled) = nullptr;
    void (*_keypadJsonCommandReceivedReceivedCallback)(const char* value) = nullptr;
    void (*_timeControlCommandReceivedReceivedCallback)(const char* value) = nullptr;
};