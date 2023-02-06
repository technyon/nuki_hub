#pragma once

#include "NukiOpener.h"
#include "NetworkOpener.h"
#include "NukiOpenerConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"

class NukiOpenerWrapper : public NukiOpener::SmartlockEventHandler
{
public:
    NukiOpenerWrapper(const std::string& deviceName,  uint32_t id,  BleScanner::Scanner* scanner, NetworkOpener* network, Preferences* preferences);
    virtual ~NukiOpenerWrapper();

    void initialize();
    void update();

    bool isPinSet();
    void setPin(const uint16_t pin);

    void unpair();
    
    void disableHASS();

    const NukiOpener::OpenerState& keyTurnerState();
    const bool isPaired();
    const BLEAddress getBleAddress() const;

    BleScanner::Scanner* bleScanner();

    void notify(NukiOpener::EventType eventType) override;

private:
    static bool onLockActionReceivedCallback(const char* value);
    static void onConfigUpdateReceivedCallback(const char* topic, const char* value);
    void onConfigUpdateReceived(const char* topic, const char* value);

    void updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData();
    void postponeBleWatchdog();

    void readConfig();
    void readAdvancedConfig();
    
    void setupHASS();

    NukiOpener::LockAction lockActionToEnum(const char* str); // char array at least 14 characters

    std::string _deviceName;
    NukiOpener::NukiOpener _nukiOpener;
    BleScanner::Scanner* _bleScanner;
    NetworkOpener* _network;
    Preferences* _preferences;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _restartBeaconTimeout = 0; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    int _retryCount = 0;
    unsigned long _nextRetryTs = 0;

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
    long _rssiPublishInterval = 0;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    unsigned long _nextConfigUpdateTs = 0;
    unsigned long _nextPairTs = 0;
    long _nextRssiTs = 0;
    unsigned long _lastRssi = 0;
    unsigned long _disableBleWatchdogTs = 0;
    NukiOpener::LockAction _nextLockAction = (NukiOpener::LockAction)0xff;
};
