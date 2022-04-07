#include "NukiWrapper.h"
#include <FreeRTOS.h>
#include "PreferencesKeys.h"

NukiWrapper* nukiInst;

NukiWrapper::NukiWrapper(const std::string& deviceName, uint32_t id, Network* network, Preferences* preferences)
: _deviceName(deviceName),
  _nukiBle(deviceName, id),
  _network(network),
  _preferences(preferences)
{
    nukiInst = this;

    memset(&_lastKeyTurnerState, sizeof(Nuki::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(Nuki::BatteryReport), 0);
    memset(&_batteryReport, sizeof(Nuki::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(Nuki::KeyTurnerState), 0);
    _keyTurnerState.lockState = Nuki::LockState::Undefined;

    network->setLockActionReceived(nukiInst->onLockActionReceived);
}


NukiWrapper::~NukiWrapper()
{
    delete _bleScanner;
    _bleScanner = nullptr;
}


void NukiWrapper::initialize()
{
    _bleScanner = new BleScanner();
    _bleScanner->initialize(_deviceName);
    _bleScanner->setScanDuration(10);
    _nukiBle.initialize();
    _nukiBle.registerBleScanner(_bleScanner);

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);

    if(_intervalLockstate == 0)
    {
        _intervalLockstate = 60 * 5;
        _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
    }
    if(_intervalBattery == 0)
    {
        _intervalBattery = 60 * 30;
        _preferences->putInt(preference_query_interval_battery, _intervalBattery);
    }

    _nukiBle.setEventHandler(this);

    Serial.print(F("Lock state interval: "));
    Serial.print(_intervalLockstate);
    Serial.print(F(" | Battery interval: "));
    Serial.println(_intervalBattery);
}

void NukiWrapper::update()
{
    if (!_paired) {
        Serial.println(F("Nuki start pairing"));

        _bleScanner->update();
        vTaskDelay( 5000 / portTICK_PERIOD_MS);
        if (_nukiBle.pairNuki()) {
            Serial.println(F("Nuki paired"));
            _paired = true;
        }
        else
        {
            vTaskDelay( 200 / portTICK_PERIOD_MS);
            return;
        }
    }

    vTaskDelay( 20 / portTICK_PERIOD_MS);
    _bleScanner->update();

    unsigned long ts = millis();

    if(_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs)
    {
        _statusUpdated = false;
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        updateKeyTurnerState();
    }
    if(_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs)
    {
        _nextBatteryReportTs = ts + _intervalBattery * 1000;
        updateBatteryState();
    }
    if(_nextLockAction != (Nuki::LockAction)0xff)
    {
         _nukiBle.lockAction(_nextLockAction, 0, 0);
         _nextLockAction = (Nuki::LockAction)0xff;
         if(_intervalLockstate > 10)
         {
             _nextLockStateUpdateTs = ts + 10 * 1000;
         }
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(Nuki::KeyTurnerState));
}

void NukiWrapper::updateKeyTurnerState()
{
    _nukiBle.requestKeyTurnerState(&_keyTurnerState);
    _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);

    if(_keyTurnerState.lockState != _lastKeyTurnerState.lockState)
    {
        char lockStateStr[20];
        lockstateToString(_keyTurnerState.lockState, lockStateStr);
        Serial.print(F("Nuki lock state: "));
        Serial.println(lockStateStr);
    }
}

void NukiWrapper::updateBatteryState()
{
    _nukiBle.requestBatteryReport(&_batteryReport);

    /*
    Serial.print(F("Voltage: ")); Serial.println(_batteryReport.batteryVoltage);
    Serial.print(F("Drain: ")); Serial.println(_batteryReport.batteryDrain);
    Serial.print(F("Resistance: ")); Serial.println(_batteryReport.batteryResistance);
    Serial.print(F("Max Current: ")); Serial.println(_batteryReport.maxTurnCurrent);
    Serial.print(F("Crit. State: ")); Serial.println(_batteryReport.criticalBatteryState);
    Serial.print(F("Lock Dist: ")); Serial.println(_batteryReport.lockDistance);
    */

    _network->publishBatteryReport(_batteryReport);
}

Nuki::LockAction NukiWrapper::lockActionToEnum(const char *str)
{
    if(strcmp(str, "unlock") == 0) return Nuki::LockAction::Unlock;
    else if(strcmp(str, "lock") == 0) return Nuki::LockAction::Lock;
    else if(strcmp(str, "unlatch") == 0) return Nuki::LockAction::Unlatch;
    else if(strcmp(str, "lockNgo") == 0) return Nuki::LockAction::LockNgo;
    else if(strcmp(str, "lockNgoUnlatch") == 0) return Nuki::LockAction::LockNgoUnlatch;
    else if(strcmp(str, "fullLock") == 0) return Nuki::LockAction::FullLock;
    else if(strcmp(str, "fobAction2") == 0) return Nuki::LockAction::FobAction2;
    else if(strcmp(str, "fobAction1") == 0) return Nuki::LockAction::FobAction1;
    else if(strcmp(str, "fobAction3") == 0) return Nuki::LockAction::FobAction3;
    return (Nuki::LockAction)0xff;
}

void NukiWrapper::onLockActionReceived(const char *value)
{
    nukiInst->_nextLockAction = nukiInst->lockActionToEnum(value);
}

const Nuki::KeyTurnerState &NukiWrapper::keyTurnerState()
{
    return _keyTurnerState;
}

const bool NukiWrapper::isPaired()
{
    return _paired;
}

BleScanner *NukiWrapper::bleScanner()
{
    return _bleScanner;
}

void NukiWrapper::notify(Nuki::EventType eventType)
{
    if(eventType == Nuki::EventType::KeyTurnerStatusUpdated)
    {
        _statusUpdated = true;
    }
}
