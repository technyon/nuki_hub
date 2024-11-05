#pragma once

#include "networkDevices/NetworkDevice.h"
#include <Preferences.h>
#include <vector>
#include "NukiConstants.h"
#include "NukiOpenerConstants.h"
#include "NukiNetworkLock.h"
#include "EspMillis.h"

class NukiNetworkOpener : public MqttReceiver
{
public:
    explicit NukiNetworkOpener(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize);
    virtual ~NukiNetworkOpener() = default;

    void initialize();
    void update();

    void publishKeyTurnerState(const NukiOpener::OpenerState& keyTurnerState, const NukiOpener::OpenerState& lastKeyTurnerState);
    void publishRing(const bool locked);
    void publishState(NukiOpener::OpenerState lockState);
    void publishAuthorizationInfo(const std::list<NukiOpener::LogEntry>& logEntries, bool latest);
    void clearAuthorizationInfo();
    void publishCommandResult(const char* resultStr);
    void publishLockstateCommandResult(const char* resultStr);
    void publishBatteryReport(const NukiOpener::BatteryReport& batteryReport);
    void publishConfig(const NukiOpener::Config& config);
    void publishAdvancedConfig(const NukiOpener::AdvancedConfig& config);
    void publishRssi(const int& rssi);
    void publishRetry(const std::string& message);
    void publishBleAddress(const std::string& address);
    void publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount);
    void publishTimeControl(const std::list<NukiOpener::TimeControlEntry>& timeControlEntries, uint maxTimeControlEntryCount);
    void publishAuth(const std::list<NukiLock::AuthorizationEntry>& authEntries, uint maxAuthEntryCount);
    void publishStatusUpdated(const bool statusUpdated);
    void publishConfigCommandResult(const char* result);
    void publishKeypadCommandResult(const char* result);
    void publishKeypadJsonCommandResult(const char* result);
    void publishTimeControlCommandResult(const char* result);
    void publishAuthCommandResult(const char* result);

    void setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char* value));
    void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char* value));
    void setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled));
    void setKeypadJsonCommandReceivedCallback(void (*keypadJsonCommandReceivedReceivedCallback)(const char* value));
    void setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char* value));
    void setAuthCommandReceivedCallback(void (*authCommandReceivedReceivedCallback)(const char* value));
    void onMqttDataReceived(const char* topic, byte* payload, const unsigned int length) override;

    int mqttConnectionState();
    bool reconnected(); //SETBACK
    uint8_t queryCommands();
    char _nukiName[33];

private:
    bool comparePrefixedPath(const char* fullPath, const char* subPath);

    void publishFloat(const char* topic, const float value, bool retain, const uint8_t precision = 2);
    void publishInt(const char* topic, const int value, bool retain);
    void publishUInt(const char* topic, const unsigned int value, bool retain);
    void publishBool(const char* topic, const bool value, bool retain);
    void publishString(const char* topic, const String& value, bool retain);
    void publishString(const char* topic, const std::string& value, bool retain);
    void publishString(const char* topic, const char* value, bool retain);
    void publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry);

    void buildMqttPath(const char* path, char* outPath);
    void subscribe(const char* path);
    void buttonPressActionToString(const NukiOpener::ButtonPressAction btnPressAction, char* str);
    void fobActionToString(const int fobact, char* str);
    void operatingModeToString(const int opmode, char* str);
    void doorbellSuppressionToString(const int dbsupr, char* str);
    void soundToString(const int sound, char* str);
    void capabilitiesToString(const int capabilities, char* str);

    String concat(String a, String b);

    Preferences* _preferences = nullptr;

    NukiNetwork* _network = nullptr;
    NukiPublisher* _nukiPublisher = nullptr;

    std::map<uint32_t, String> _authEntries;
    char _mqttPath[181] = {0};
    bool _isConnected = false;
    bool _firstTunerStatePublish = true;
    bool _haEnabled = false;
    bool _reconnected = false; //SETBACK
    bool _disableNonJSON = false;

    String _keypadCommandName = "";
    String _keypadCommandCode = "";
    uint _keypadCommandId = 0;
    int _keypadCommandEnabled = 1;
    int64_t _resetRingStateTs = 0;
    uint8_t _queryCommands = 0;
    uint32_t _authId = 0;
    char _authName[33];
    bool _authFound = false;
    uint32_t _lastRollingLog = 0;

    NukiOpener::LockState _currentLockState = NukiOpener::LockState::Undefined;

    char* _buffer;
    const size_t _bufferSize;

    LockActionResult (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* value) = nullptr;
    void (*_keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled) = nullptr;
    void (*_keypadJsonCommandReceivedReceivedCallback)(const char* value) = nullptr;
    void (*_timeControlCommandReceivedReceivedCallback)(const char* value) = nullptr;
    void (*_authCommandReceivedReceivedCallback)(const char* value) = nullptr;
};
