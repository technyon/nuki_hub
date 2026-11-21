#pragma once

#include "networkDevices/NetworkDevice.h"
#include <Preferences.h>
#include <vector>
#include "NukiConstants.h"
#include "NukiOpenerConstants.h"
#include "NukiNetworkLock.h"
#include "EspMillis.h"

class NukiNetworkOpener : public MqttReceiver, public NukiNetworkEntries
{
public:
    explicit NukiNetworkOpener(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize);
    virtual ~NukiNetworkOpener() = default;

    void initialize();
    void update();

    void publishKeyTurnerState(const NukiOpener::OpenerState& keyTurnerState, const NukiOpener::OpenerState& lastKeyTurnerState);
    void publishAvailability(const bool available);
    void publishRing(const bool locked);
    const int64_t lastRingTs() const;
    const time_t lastRingTime() const;
    void publishState(NukiOpener::OpenerState lockState);
    void publishAuthorizationInfo(const std::list<NukiOpener::LogEntry>& logEntries, bool latest);
    uint32_t getLastRollingLog() const;
    void clearAuthorizationInfo();
    void publishCommandResult(const char* resultStr);
    void publishLockstateCommandResult(const char* resultStr);
    void publishBatteryReport(const NukiOpener::BatteryReport& batteryReport);
    void publishConfig(const NukiOpener::Config& config);
    void publishAdvancedConfig(const NukiOpener::AdvancedConfig& config);
    void publishRssi(const int& rssi);
    void publishRetry(const std::string& message);
    void publishBleAddress(const std::string& address);
    void publishTimeControl(const std::list<NukiOpener::TimeControlEntry>& timeControlEntries, uint maxTimeControlEntryCount);
    void publishStatusUpdated(const bool statusUpdated);
    void publishConfigCommandResult(const char* result);
    void publishKeypadCommandResult(const char* result);
    void publishKeypadJsonCommandResult(const char* result);
    void publishTimeControlCommandResult(const char* result);
    void publishAuthCommandResult(const char* result);
    const String& lastCommandResult() const;
    uint32_t commandResultCounter() const;

    void setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char* value));
    void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char* value));
    void setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled));
    void setKeypadJsonCommandReceivedCallback(void (*keypadJsonCommandReceivedReceivedCallback)(const char* value));
    void setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char* value));
    void setAuthCommandReceivedCallback(void (*authCommandReceivedReceivedCallback)(const char* value));
    void onMqttDataReceived(const char* topic, byte* payload, const unsigned int length) override;
    void setupHASS(int type, uint32_t nukiId, char* nukiName, const char* firmwareVersion, const char* hardwareVersion, bool hasDoorSensor, bool hasKeypad);
    bool hassRefreshRequested();

    int mqttConnectionState();
    uint8_t queryCommands();

private:
    bool comparePrefixedPath(const char* fullPath, const char* subPath);


    void buildMqttPath(const char* path, char* outPath);
    void subscribe(const char* path);




    void recordCommandResult(const char* result);

    String _lastCommandResult;
    uint32_t _commandResultCounter = 0;
    bool _firstTunerStatePublish = true;
    bool _haEnabled = false;
    bool _saveLogEnabled = false;
    bool _publishAuthData = false;

    String _keypadCommandName = "";
    String _keypadCommandCode = "";
    uint _keypadCommandId = 0;
    int _keypadCommandEnabled = 1;
    int64_t _resetRingStateTs = 0;
    int64_t _lastRingTs = 0;
    time_t _lastRingTime = 0;
    uint8_t _queryCommands = 0;
    uint32_t _authId = 0;
    char _authName[33];
    uint32_t _lastRollingLog = 0;
    bool _lastRollingLogReceived = false;


    LockActionResult (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* value) = nullptr;
    void (*_keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled) = nullptr;
    void (*_keypadJsonCommandReceivedReceivedCallback)(const char* value) = nullptr;
    void (*_timeControlCommandReceivedReceivedCallback)(const char* value) = nullptr;
    void (*_authCommandReceivedReceivedCallback)(const char* value) = nullptr;
};
