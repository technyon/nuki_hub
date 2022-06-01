#include "NukiWrapper.h"
#include <FreeRTOS.h>
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include <NukiLockUtils.h>

NukiWrapper* nukiInst;

NukiWrapper::NukiWrapper(const std::string& deviceName, uint32_t id, BleScanner::Scanner* scanner, Network* network, Preferences* preferences)
: _deviceName(deviceName),
  _bleScanner(scanner),
  _nukiBle(deviceName, id),
  _network(network),
  _preferences(preferences)
{
    nukiInst = this;

    memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    _keyTurnerState.lockState = NukiLock::LockState::Undefined;

    network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
    network->setConfigUpdateReceivedCallback(nukiInst->onConfigUpdateReceivedCallback);
}


NukiWrapper::~NukiWrapper()
{
    _bleScanner = nullptr;
}


void NukiWrapper::initialize()
{

    _nukiBle.initialize();
    _nukiBle.registerBleScanner(_bleScanner);

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);
    _publishAuthData = _preferences->getBool(preference_publish_authdata);

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
    Serial.print(_intervalBattery);
    Serial.print(F(" | Publish auth data: "));
    Serial.println(_publishAuthData ? "yes" : "no");

    if(!_publishAuthData)
    {
        _clearAuthData = true;
    }
}

void NukiWrapper::update()
{
    if (!_paired) {
        Serial.println(F("Nuki start pairing"));

        _bleScanner->update();
        vTaskDelay( 5000 / portTICK_PERIOD_MS);
        if (_nukiBle.pairNuki() == Nuki::PairingResult::Success) {
            Serial.println(F("Nuki paired"));
            _paired = true;
        }
        else
        {
            vTaskDelay( 200 / portTICK_PERIOD_MS);
            return;
        }
    }

    _nukiBle.updateConnectionState();

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
    if(_nextConfigUpdateTs == 0 || ts > _nextConfigUpdateTs)
    {
        _nextConfigUpdateTs = ts + _intervalConfig * 1000;
        updateConfig();
    }

    if(_nextLockAction != (NukiLock::LockAction)0xff)
    {
         Nuki::CmdResult cmdResult = _nukiBle.lockAction(_nextLockAction, 0, 0);

         char resultStr[15] = {0};
         NukiLock::cmdResultToString(cmdResult, resultStr);

         _network->publishCommandResult(resultStr);

         Serial.print(F("Lock action result: "));
         Serial.println(resultStr);

         _nextLockAction = (NukiLock::LockAction)0xff;
         if(_intervalLockstate > 10)
         {
             _nextLockStateUpdateTs = ts + 10 * 1000;
         }
    }

    if(_clearAuthData)
    {
        _network->publishAuthorizationInfo(0, "");
        _clearAuthData = false;
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(NukiLock::KeyTurnerState));
}

void NukiWrapper::setPin(const uint16_t pin)
{
        _nukiBle.saveSecurityPincode(pin);
}

void NukiWrapper::unpair()
{
    _nukiBle.unPairNuki();
    _paired = false;
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

    if(_publishAuthData)
    {
        updateAuthData();
    }
}

void NukiWrapper::updateBatteryState()
{
    _nukiBle.requestBatteryReport(&_batteryReport);
    _network->publishBatteryReport(_batteryReport);
}

void NukiWrapper::updateConfig()
{
    readConfig();
    readAdvancedConfig();
    _network->publishConfig(_nukiConfig);
    _network->publishAdvancedConfig(_nukiAdvancedConfig);
}

void NukiWrapper::updateAuthData()
{
    Nuki::CmdResult result = _nukiBle.retrieveLogEntries(0, 0, 0, true);
    if(result != Nuki::CmdResult::Success)
    {
        _network->publishAuthorizationInfo(0, "");
        return;
    }
    vTaskDelay( 100 / portTICK_PERIOD_MS);

    result = _nukiBle.retrieveLogEntries(_nukiBle.getLogEntryCount() - 2, 1, 0, false);
    if(result != Nuki::CmdResult::Success)
    {
        _network->publishAuthorizationInfo(0, "");
        return;
    }
    vTaskDelay( 200 / portTICK_PERIOD_MS);

    std::list<Nuki::LogEntry> log;
    _nukiBle.getLogEntries(&log);

    if(log.size() > 0)
    {
        const Nuki::LogEntry& entry = log.front();
//        log_d("Log: %d-%d-%d %d:%d:%d %s", entry.timeStampYear, entry.timeStampMonth, entry.timeStampDay,
//              entry.timeStampHour, entry.timeStampMinute, entry.timeStampSecond, entry.name);
        if(entry.authId != _lastAuthId)
        {
            _network->publishAuthorizationInfo(entry.authId, (char *) entry.name);
            _lastAuthId = entry.authId;
        }
    }
    else
    {
        _network->publishAuthorizationInfo(0, "");
    }
}

NukiLock::LockAction NukiWrapper::lockActionToEnum(const char *str)
{
    if(strcmp(str, "unlock") == 0) return NukiLock::LockAction::Unlock;
    else if(strcmp(str, "lock") == 0) return NukiLock::LockAction::Lock;
    else if(strcmp(str, "unlatch") == 0) return NukiLock::LockAction::Unlatch;
    else if(strcmp(str, "lockNgo") == 0) return NukiLock::LockAction::LockNgo;
    else if(strcmp(str, "lockNgoUnlatch") == 0) return NukiLock::LockAction::LockNgoUnlatch;
    else if(strcmp(str, "fullLock") == 0) return NukiLock::LockAction::FullLock;
    else if(strcmp(str, "fobAction2") == 0) return NukiLock::LockAction::FobAction2;
    else if(strcmp(str, "fobAction1") == 0) return NukiLock::LockAction::FobAction1;
    else if(strcmp(str, "fobAction3") == 0) return NukiLock::LockAction::FobAction3;
    return (NukiLock::LockAction)0xff;
}

bool NukiWrapper::onLockActionReceivedCallback(const char *value)
{
    NukiLock::LockAction action = nukiInst->lockActionToEnum(value);
    nukiInst->_nextLockAction = action;
    return (int)action != 0xff;
}

void NukiWrapper::onConfigUpdateReceivedCallback(const char *topic, const char *value)
{
    nukiInst->onConfigUpdateReceived(topic, value);
}


void NukiWrapper::onConfigUpdateReceived(const char *topic, const char *value)
{
    if(strcmp(topic, mqtt_topic_config_button_enabled) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiConfigValid || _nukiConfig.buttonEnabled == newValue) return;
        _nukiBle.enableButton(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    if(strcmp(topic, mqtt_topic_config_led_enabled) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiConfigValid || _nukiConfig.ledEnabled == newValue) return;
        _nukiBle.enableLedFlash(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_led_brightness) == 0)
    {
        int newValue = atoi(value);
        if(!_nukiConfigValid || _nukiConfig.ledBrightness == newValue) return;
        _nukiBle.setLedBrightness(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_auto_unlock) == 0)
    {
        bool newValue = !(atoi(value) > 0);
        if(!_nukiAdvancedConfigValid || _nukiAdvancedConfig.autoUnLockDisabled == newValue) return;
        _nukiBle.disableAutoUnlock(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_auto_lock) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiAdvancedConfigValid || _nukiAdvancedConfig.autoLockEnabled == newValue) return;
        _nukiBle.enableAutoLock(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_auto_lock) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiAdvancedConfigValid || _nukiAdvancedConfig.autoLockEnabled == newValue) return;
        _nukiBle.enableAutoLock(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
}

const NukiLock::KeyTurnerState &NukiWrapper::keyTurnerState()
{
    return _keyTurnerState;
}

const bool NukiWrapper::isPaired()
{
    return _paired;
}

void NukiWrapper::notify(Nuki::EventType eventType)
{
    if(eventType == Nuki::EventType::KeyTurnerStatusUpdated)
    {
        _statusUpdated = true;
    }
}

void NukiWrapper::readConfig()
{
    Serial.print(F("Reading config. Result: "));
    Nuki::CmdResult result = _nukiBle.requestConfig(&_nukiConfig);
    _nukiConfigValid = result == Nuki::CmdResult::Success;
    Serial.println(result);
}

void NukiWrapper::readAdvancedConfig()
{
    Serial.print(F("Reading advanced config. Result: "));
    Nuki::CmdResult result = _nukiBle.requestAdvancedConfig(&_nukiAdvancedConfig);
    _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;
    Serial.println(result);
}
