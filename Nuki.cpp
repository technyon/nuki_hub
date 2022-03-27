#include "Nuki.h"
#include <FreeRTOS.h>
#include "PreferencesKeys.h"

Nuki* nukiInst;

Nuki::Nuki(const std::string& name, uint32_t id, Network* network, Preferences* preferences)
: _nukiBle(name, id),
  _network(network),
  _preferences(preferences)
{
    nukiInst = this;

    memset(&_lastKeyTurnerState, sizeof(KeyTurnerState), 0);
    memset(&_keyTurnerState, sizeof(KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(BatteryReport), 0);
    memset(&_batteryReport, sizeof(BatteryReport), 0);

    network->setLockActionReceived(nukiInst->onLockActionReceived);
}

void Nuki::initialize()
{
    _nukiBle.initialize();

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);

    if(_intervalLockstate == 0)
    {
        _intervalLockstate = 30;
        _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
    }
    if(_intervalBattery == 0)
    {
        _intervalBattery = 60 * 30;
        _preferences->putInt(preference_query_interval_battery, _intervalBattery);
    }

    Serial.print(F("Lock state interval: "));
    Serial.print(_intervalLockstate);
    Serial.print(F("| Battery interval: "));
    Serial.println(_intervalBattery);
}

void Nuki::update()
{
    if (!_paired) {
        Serial.println(F("Nuki start pairing"));

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

    vTaskDelay( 200 / portTICK_PERIOD_MS);

    unsigned long ts = millis();

    if(_nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs)
    {
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        updateKeyTurnerState();
    }
    if(_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs)
    {
        _nextBatteryReportTs = ts + _intervalBattery * 1000;
        updateBatteryState();
    }
    if(_nextLockAction != (LockAction)0xff)
    {
         _nukiBle.lockAction(_nextLockAction, 0, 0);
         _nextLockAction = (LockAction)0xff;
    }
}

void Nuki::updateKeyTurnerState()
{
    _nukiBle.requestKeyTurnerState(&_keyTurnerState);

    char lockStateStr[20];
    lockstateToString(_keyTurnerState.lockState, lockStateStr);
    char triggerStr[20];
    triggerToString(_keyTurnerState.trigger, triggerStr);
    char completionStatusStr[20];
    completionStatusToString(_keyTurnerState.lastLockActionCompletionStatus, completionStatusStr);

    if(_keyTurnerState.lockState != _lastKeyTurnerState.lockState)
    {
        _network->publishKeyTurnerState(lockStateStr, triggerStr, completionStatusStr);
        Serial.print(F("Nuki lock state: "));
        Serial.println(lockStateStr);
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(KeyTurnerState));
}


void Nuki::updateBatteryState()
{
    _nukiBle.requestBatteryReport(&_batteryReport);

    Serial.print(F("Voltage: ")); Serial.println(_batteryReport.batteryVoltage);
    Serial.print(F("Drain: ")); Serial.println(_batteryReport.batteryDrain);
    Serial.print(F("Resistance: ")); Serial.println(_batteryReport.batteryResistance);
    Serial.print(F("Max Current: ")); Serial.println(_batteryReport.maxTurnCurrent);
    Serial.print(F("Crit. State: ")); Serial.println(_batteryReport.criticalBatteryState);
    Serial.print(F("Lock Dist: ")); Serial.println(_batteryReport.lockDistance);

    _network->publishBatteryReport(_batteryReport);
}


void Nuki::lockstateToString(const LockState state, char* str)
{
    switch(state)
    {
        case LockState::uncalibrated:
            strcpy(str, "uncalibrated");
            break;
        case LockState::locked:
            strcpy(str, "locked");
            break;
        case LockState::locking:
            strcpy(str, "locking");
            break;
        case LockState::unlocked:
            strcpy(str, "unlocked");
            break;
        case LockState::unlatched:
            strcpy(str, "unlatched");
            break;
        case LockState::unlockedLnga:
            strcpy(str, "unlockedLnga");
            break;
        case LockState::unlatching:
            strcpy(str, "unlatching");
            break;
        case LockState::calibration:
            strcpy(str, "calibration");
            break;
        case LockState::bootRun:
            strcpy(str, "bootRun");
            break;
        case LockState::motorBlocked:
            strcpy(str, "motorBlocked");
            break;
        default:
            strcpy(str, "undefined");
            break;
    }
}


void Nuki::triggerToString(const NukiTrigger trigger, char *str)
{
    switch(trigger)
    {
        case NukiTrigger::autoLock:
            strcpy(str, "autoLock");
            break;
        case NukiTrigger::automatic:
            strcpy(str, "automatic");
            break;
        case NukiTrigger::button:
            strcpy(str, "button");
            break;
        case NukiTrigger::manual:
            strcpy(str, "autoLock");
            break;
        case NukiTrigger::system:
            strcpy(str, "system");
            break;
        default:
            strcpy(str, "undefined");
            break;
    }
}

void Nuki::completionStatusToString(const CompletionStatus status, char *str)
{
    switch (status)
    {
        case CompletionStatus::success:
            strcpy(str, "success");
            break;
        case CompletionStatus::busy:
            strcpy(str, "busy");
            break;
        case CompletionStatus::canceled:
            strcpy(str, "canceled");
            break;
        case CompletionStatus::clutchFailure:
            strcpy(str, "clutchFailure");
            break;
        case CompletionStatus::incompleteFailure:
            strcpy(str, "incompleteFailure");
            break;
        case CompletionStatus::invalidCode:
            strcpy(str, "invalidCode");
            break;
        case CompletionStatus::lowMotorVoltage:
            strcpy(str, "lowMotorVoltage");
            break;
        case CompletionStatus::motorBlocked:
            strcpy(str, "motorBlocked");
            break;
        case CompletionStatus::motorPowerFailure:
            strcpy(str, "motorPowerFailure");
            break;
        case CompletionStatus::otherError:
            strcpy(str, "otherError");
            break;
        case CompletionStatus::tooRecent:
            strcpy(str, "tooRecent");
            break;
        case CompletionStatus::unknown:
            strcpy(str, "unknown");
            break;
        default:
            strcpy(str, "undefined");
            break;

    }
}

LockAction Nuki::lockActionToEnum(const char *str)
{
    if(strcmp(str, "unlock") == 0) return LockAction::unlock;
    else if(strcmp(str, "lock") == 0) return LockAction::lock;
    else if(strcmp(str, "unlatch") == 0) return LockAction::unlatch;
    else if(strcmp(str, "lockNgo") == 0) return LockAction::lockNgo;
    else if(strcmp(str, "lockNgoUnlatch") == 0) return LockAction::lockNgoUnlatch;
    else if(strcmp(str, "fullLock") == 0) return LockAction::fullLock;
    else if(strcmp(str, "fobAction2") == 0) return LockAction::fobAction2;
    else if(strcmp(str, "fobAction1") == 0) return LockAction::fobAction1;
    else if(strcmp(str, "fobAction3") == 0) return LockAction::fobAction3;
    return (LockAction)0xff;
}


void Nuki::onLockActionReceived(const char *value)
{
    nukiInst->_nextLockAction = nukiInst->lockActionToEnum(value);
    Serial.print(F("Action: "));
    Serial.println((int)nukiInst->_nextLockAction);
}
