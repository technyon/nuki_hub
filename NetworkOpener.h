#pragma once

#include "networkDevices/NetworkDevice.h"
#include "networkDevices/WifiDevice.h"
#include "networkDevices/W5500Device.h"
#include <Preferences.h>
#include <vector>
#include "NukiConstants.h"
#include "NukiOpenerConstants.h"
#include "NetworkLock.h"

class NetworkOpener : public MqttReceiver
{
public:
    explicit NetworkOpener(Network* network, Preferences* preferences, char* buffer, size_t bufferSize);
    virtual ~NetworkOpener() = default;

    void initialize();
    void update();

    void publishKeyTurnerState(const NukiOpener::OpenerState& keyTurnerState, const NukiOpener::OpenerState& lastKeyTurnerState);
    void publishRing(const bool locked);
    void publishState(NukiOpener::OpenerState lockState);
    void publishAuthorizationInfo(const std::list<NukiOpener::LogEntry>& logEntries);
    void clearAuthorizationInfo();
    void publishCommandResult(const char* resultStr);
    void publishLockstateCommandResult(const char* resultStr);
    void publishBatteryReport(const NukiOpener::BatteryReport& batteryReport);
    void publishConfig(const NukiOpener::Config& config);
    void publishAdvancedConfig(const NukiOpener::AdvancedConfig& config);
    void publishRssi(const int& rssi);
    void publishRetry(const std::string& message);
    void publishBleAddress(const std::string& address);
    void publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction);
    void removeHASSConfig(char* uidString);
    void publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount);
    void publishConfigCommandResult(const char* result);
    void publishKeypadCommandResult(const char* result);

    void setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char* value));
    void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char* value));
    void setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled));

    void onMqttDataReceived(const char* topic, byte* payload, const unsigned int length) override;

    bool reconnected();
    uint8_t queryCommands();

private:
    bool comparePrefixedPath(const char* fullPath, const char* subPath);

    void publishFloat(const char* topic, const float value, const uint8_t precision = 2);
    void publishInt(const char* topic, const int value);
    void publishUInt(const char* topic, const unsigned int value);
    void publishBool(const char* topic, const bool value);
    void publishString(const char* topic, const String& value);
    void publishString(const char* topic, const std::string& value);
    void publishString(const char* topic, const char* value);
    void publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry);

    void buildMqttPath(const char* path, char* outPath);
    void subscribe(const char* path);
    void logactionCompletionStatusToString(uint8_t value, char* out);
    void batteryTypeToString(const Nuki::BatteryType battype, char* str);
    void buttonPressActionToString(const NukiOpener::ButtonPressAction btnPressAction, char* str);
    void advertisingModeToString(const Nuki::AdvertisingMode advmode, char* str);
    void timeZoneIdToString(const Nuki::TimeZoneId timeZoneId, char* str);
    void fobActionToString(const int fobact, char* str);
    void operatingModeToString(const int opmode, char* str);
    void capabilitiesToString(const int capabilities, char* str);

    String concat(String a, String b);

    Preferences* _preferences;

    Network* _network = nullptr;

    char _mqttPath[181] = {0};
    bool _isConnected = false;

    bool _firstTunerStatePublish = true;
    bool _haEnabled= false;
    bool _reconnected = false;

    String _keypadCommandName = "";
    String _keypadCommandCode = "";
    uint _keypadCommandId = 0;
    int _keypadCommandEnabled = 1;
    unsigned long _resetRingStateTs = 0;
    uint8_t _queryCommands = 0;
    uint32_t authId = 0;
    char authName[33];

    NukiOpener::LockState _currentLockState = NukiOpener::LockState::Undefined;

    char* _buffer;
    const size_t _bufferSize;

    LockActionResult (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* value) = nullptr;
    void (*_keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled) = nullptr;
};
