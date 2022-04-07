#pragma once

#include "NukiBle.h"
#include "Network.h"
#include "NukiConstants.h"
#include "NukiDataTypes.h"

class NukiWrapper : public Nuki::SmartlockEventHandler
{
public:
    NukiWrapper(const std::string& deviceName, uint32_t id, Network* network, Preferences* preferences);
    virtual ~NukiWrapper();

    void initialize();
    void update();

    const Nuki::KeyTurnerState& keyTurnerState();
    const bool isPaired();

    BleScanner* bleScanner();

    void notify(Nuki::EventType eventType) override;

private:
    static void onLockActionReceived(const char* value);

    void updateKeyTurnerState();
    void updateBatteryState();

    Nuki::LockAction lockActionToEnum(const char* str); // char array at least 14 characters

    std::string _deviceName;
    Nuki::NukiBle _nukiBle;
    BleScanner* _bleScanner;
    Network* _network;
    Preferences* _preferences;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds

    Nuki::KeyTurnerState _lastKeyTurnerState;
    Nuki::KeyTurnerState _keyTurnerState;

    Nuki::BatteryReport _batteryReport;
    Nuki::BatteryReport _lastBatteryReport;

    bool _paired = false;
    bool _statusUpdated = false;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    unsigned long _nextPairTs = 0;
    Nuki::LockAction _nextLockAction = (Nuki::LockAction)0xff;
};
