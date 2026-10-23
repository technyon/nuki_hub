#pragma once

#include <functional>
#include "NukiOpener.h"
#include "NukiNetworkOpener.h"
#include "NukiOpenerConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"
#include "Gpio.h"
#include "NukiDeviceId.h"
#include "util/NukiRetryHandler.h"
#include "util/NukiOpenerHelper.h"
#include <NukiOpenerUtils.h>
#include "NukiCommandHandler.h"

class String;

struct NukiOpenerCommandTraits
{
    using KeypadEntry = NukiOpener::KeypadEntry;
    using NewKeypadEntry = NukiOpener::NewKeypadEntry;
    using UpdatedKeypadEntry = NukiOpener::UpdatedKeypadEntry;
    using AuthorizationEntry = NukiOpener::AuthorizationEntry;
    using NewAuthorizationEntry = NukiOpener::NewAuthorizationEntry;
    using UpdatedAuthorizationEntry = NukiOpener::UpdatedAuthorizationEntry;
    using TimeControlEntry = NukiOpener::TimeControlEntry;
    using NewTimeControlEntry = NukiOpener::NewTimeControlEntry;
    using LockAction = NukiOpener::LockAction;

    static void cmdResultToString(const Nuki::CmdResult result, char* str)
    {
        NukiOpener::cmdResultToString(result, str);
    }

    static void printCommandResult(const Nuki::CmdResult result)
    {
        NukiOpenerHelper::printCommandResult(result);
    }

    static LockAction lockActionToEnum(const char* str)
    {
        return NukiOpenerHelper::lockActionToEnum(str);
    }
};

class NukiOpenerWrapper : public NukiOpener::SmartlockEventHandler, public NukiCommandHandler<NukiOpenerWrapper, NukiOpenerCommandTraits>
{
public:
    friend class NukiCommandHandler<NukiOpenerWrapper, NukiOpenerCommandTraits>;
    NukiOpenerWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NukiNetworkOpener* network, Gpio* gpio, Preferences* preferences, char* buffer, size_t bufferSize);
    virtual ~NukiOpenerWrapper();

    void initialize();
    void readSettings();
    void update();

    void electricStrikeActuation();
    void activateRTO();
    void activateCM();
    void deactivateRtoCm();
    void deactivateRTO();
    void deactivateCM();

    bool hasConnected();
    bool isPinValid();
    void setPin(const uint16_t pin);
    const uint16_t getPin();

    void unpair();
    void disableWatchdog();

    const NukiOpener::OpenerState& keyTurnerState();
    const bool isPaired() const;
    const bool hasKeypad() const;
    const BLEAddress getBleAddress() const;
    const uint8_t restartController() const;

    const std::string firmwareVersion() const;
    NukiOpener::NukiOpener& nukiDevice();
    const std::string hardwareVersion() const;
    const std::string name() const;
    static LockActionResult onLockActionReceivedCallback(const char* value);

    const BleScanner::Scanner* bleScanner();

    void notify(NukiOpener::EventType eventType) override;

private:
    static void onConfigUpdateReceivedCallback(const char* value);
    static void onKeypadCommandReceivedCallback(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    static void onKeypadJsonCommandReceivedCallback(const char* value);
    static void onTimeControlCommandReceivedCallback(const char* value);
    static void onAuthCommandReceivedCallback(const char* value);
    static void IRAM_ATTR gpioActionCallback(const GpioAction& action, const int& pin);

    bool checkPaired();
    void checkRestartByBeacon(const int64_t& ts);
    void checkLockAction(const int64_t& ts);
    void checkQueries(const int64_t& ts, const uint8_t& queryCommands);

    void onConfigUpdateReceived(const char* value);
    void updateConfigBool(JsonDocument& jsonResult, const char* key, const char* value, uint8_t current, Nuki::CmdResult& cmdResult, std::function<Nuki::CmdResult(bool)> setter);
    void updateConfigInt(JsonDocument& jsonResult, const char* key, const char* value, int min, int max, int current, Nuki::CmdResult& cmdResult, std::function<Nuki::CmdResult(int)> setter);
    void onKeypadJsonCommandReceived(const char* value);
    void checkKeypadJsonCommand();
    void publishKeypadCache();

    bool updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData(bool retrieved);
    bool retrieveMissingLogEntries(std::list<NukiOpener::LogEntry>& log);
    void waitForLogEntries(uint32_t count);
    void updateKeypad(bool retrieved);
    void updateTimeControl(bool retrieved);
    void updateAuth(bool retrieved);
    void postponeBleWatchdog();
    bool updateTime();

    void updateGpioOutputs();

    void readConfig();
    void readAdvancedConfig();

    std::string _deviceName;
    NukiDeviceId* _deviceId = nullptr;
    NukiOpener::NukiOpener _nukiOpener;
    BleScanner::Scanner* _bleScanner = nullptr;
    NukiNetworkOpener* _network = nullptr;
    Preferences* _preferences = nullptr;
    Gpio* _gpio = nullptr;
    NukiRetryHandler* _nukiRetryHandler = nullptr;

    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _intervalKeypad = 0; // seconds
    int _restartBeaconTimeout = 0; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;
    bool _disableNonJSON = false;
    bool _checkKeypadCodes = false;
    bool _pairedAsApp = false;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    int _retryConfigCount = 0;
    int _retryLockstateCount = 0;
    int64_t _nextRetryTs = 0;
    int _invalidCount = 0;
    int64_t _lastCodeCheck = 0;
    std::vector<uint16_t> _keypadCodeIds;
    std::vector<uint32_t> _keypadCodes;
    std::vector<uint8_t> _timeControlIds;
    std::vector<uint32_t> _authIds;

    NukiOpener::OpenerState _lastKeyTurnerState;
    NukiOpener::OpenerState _keyTurnerState;

    NukiOpener::BatteryReport _batteryReport;
    NukiOpener::BatteryReport _lastBatteryReport;

    NukiOpener::Config _nukiConfig = {0};
    NukiOpener::AdvancedConfig _nukiAdvancedConfig = {0};
    bool _nukiConfigValid = false;
    bool _nukiAdvancedConfigValid = false;
    bool _hassEnabled = false;
    bool _hassSetupCompleted = false;

    bool _paired = false;
    bool _statusUpdated = false;
    int _newSignal = 0;
    bool _hasKeypad = false;
    bool _forceKeypad = false;
    bool _keypadEnabled = false;
    bool _forceId = false;
    bool _hasConnected = false;
    uint _maxKeypadCodeCount = 0;
    uint _maxTimeControlEntryCount = 0;
    uint _maxAuthEntryCount = 0;
    uint8_t _restartController = 0;
    int _rssiPublishInterval = 0;
    int64_t _statusUpdatedTs = 0;
    int64_t _nextLockStateUpdateTs = 0;
    int64_t _nextStatusPollTs = 0;
    int64_t _nextBatteryReportTs = 0;
    int64_t _nextConfigUpdateTs = 0;
    int64_t _waitAuthLogUpdateTs = 0;
    int64_t _waitKeypadUpdateTs = 0;
    int64_t _waitTimeControlUpdateTs = 0;
    int64_t _waitAuthUpdateTs = 0;
    int64_t _nextTimeUpdateTs = 0;
    int64_t _nextKeypadUpdateTs = 0;
    int64_t _nextPairTs = 0;
    int64_t _nextRssiTs = 0;
    int64_t _lastRssi = 0;
    int64_t _disableBleWatchdogTs = 0;
    uint32_t _aclPrefs[17] = {0};
    uint32_t _basicOpenerConfigAclPrefs[16];
    uint32_t _advancedOpenerConfigAclPrefs[22];
    std::string _firmwareVersion = "";
    std::string _hardwareVersion = "";
    NukiOpener::LockAction _nextLockAction = (NukiOpener::LockAction)0xff;
    std::list<NukiOpener::KeypadEntry> _keypadEntriesCache;
    std::vector<std::pair<uint16_t, int64_t>> _keypadDeleted;
    bool _keypadCacheDirty = false;
    int64_t _nextCachePublishTs = 0;
    QueueHandle_t _keypadJsonQueue = nullptr;

    char* _buffer;
    const size_t _bufferSize;
};
