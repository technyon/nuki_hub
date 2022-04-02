#pragma once

#include "NukiBle.h"
#include "Network.h"

class Nuki : public NukiSmartlockEventHandler
{
public:
    Nuki(const std::string& name, uint32_t id, Network* network, Preferences* preferences);

    void initialize();
    void update();

    const bool isPaired();

    void notify(NukiEventType eventType) override;

private:
    static void onLockActionReceived(const char* value);

    void updateKeyTurnerState();
    void updateBatteryState();

    LockAction lockActionToEnum(const char* str); // char array at least 14 characters

    NukiBle _nukiBle;
    BleScanner _bleScanner;
    Network* _network;
    Preferences* _preferences;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds

    KeyTurnerState _lastKeyTurnerState;
    KeyTurnerState _keyTurnerState;

    BatteryReport _batteryReport;
    BatteryReport _lastBatteryReport;

    bool _paired = false;
    bool _statusUpdated = false;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    LockAction _nextLockAction = (LockAction)0xff;
};
