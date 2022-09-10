#include "NukiWrapper.h"
#include <RTOS.h>
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include <NukiLockUtils.h>

NukiWrapper* nukiInst;

NukiWrapper::NukiWrapper(const std::string& deviceName, uint32_t id, BleScanner::Scanner* scanner, NetworkLock* network, Preferences* preferences)
: _deviceName(deviceName),
  _bleScanner(scanner),
  _nukiLock(deviceName, id),
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
    network->setKeypadCommandReceivedCallback(nukiInst->onKeypadCommandReceivedCallback);
}


NukiWrapper::~NukiWrapper()
{
    _bleScanner = nullptr;
}


void NukiWrapper::initialize()
{

    _nukiLock.initialize();
    _nukiLock.registerBleScanner(_bleScanner);

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);
    _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
    _keypadEnabled = _preferences->getBool(preference_keypad_control_enabled);
    _publishAuthData = _preferences->getBool(preference_publish_authdata);
    _maxKeypadCodeCount = _preferences->getUInt(preference_max_keypad_code_count);

    if(_intervalLockstate == 0)
    {
        _intervalLockstate = 60 * 30;
        _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
    }
    if(_intervalBattery == 0)
    {
        _intervalBattery = 60 * 30;
        _preferences->putInt(preference_query_interval_battery, _intervalBattery);
    }
    if(_intervalKeypad == 0)
    {
        _intervalKeypad = 60 * 30;
        _preferences->putInt(preference_query_interval_keypad, _intervalKeypad);
    }

    _nukiLock.setEventHandler(this);

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

        if (_nukiLock.pairNuki() == Nuki::PairingResult::Success) {
            Serial.println(F("Nuki paired"));
            _paired = true;
            setupHASS();
        }
        else
        {
            delay(200);
            return;
        }
    }

    _nukiLock.updateConnectionState();

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
    if(_nextRssiTs == 0 || ts > _nextRssiTs)
    {
        _nextRssiTs = ts + 3000;

        int rssi = _nukiLock.getRssi();
        if(rssi != _lastRssi)
        {
            _network->publishRssi(rssi);
            _lastRssi = rssi;
        }
    }
    if(_hasKeypad && _keypadEnabled && (_nextKeypadUpdateTs == 0 || ts > _nextKeypadUpdateTs))
    {
        _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
        updateKeypad();
    }

    if(_nextLockAction != (NukiLock::LockAction)0xff)
    {
        Nuki::CmdResult cmdResult = _nukiLock.lockAction(_nextLockAction, 0, 0);

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
        _network->clearAuthorizationInfo();
        _clearAuthData = false;
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(NukiLock::KeyTurnerState));
}

void NukiWrapper::lock()
{
    _nextLockAction = NukiLock::LockAction::Lock;
}

void NukiWrapper::unlock()
{
    _nextLockAction = NukiLock::LockAction::Unlock;
}

void NukiWrapper::unlatch()
{
    _nextLockAction = NukiLock::LockAction::Unlatch;
}

void NukiWrapper::setPin(const uint16_t pin)
{
        _nukiLock.saveSecurityPincode(pin);
}

void NukiWrapper::unpair()
{
    _nukiLock.unPairNuki();
    _paired = false;
}

void NukiWrapper::updateKeyTurnerState()
{
    _nukiLock.requestKeyTurnerState(&_keyTurnerState);
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
    _nukiLock.requestBatteryReport(&_batteryReport);
    _network->publishBatteryReport(_batteryReport);
}

void NukiWrapper::updateConfig()
{
    readConfig();
    readAdvancedConfig();
    _configRead = true;
    _hasKeypad = _nukiConfig.hasKeypad > 0;
    _network->publishConfig(_nukiConfig);
    _network->publishAdvancedConfig(_nukiAdvancedConfig);
}

void NukiWrapper::updateAuthData()
{
    Nuki::CmdResult result = _nukiLock.retrieveLogEntries(0, 0, 0, true);
    if(result != Nuki::CmdResult::Success)
    {
        return;
    }
    delay(100);

    uint16_t count = _nukiLock.getLogEntryCount();

    result = _nukiLock.retrieveLogEntries(0, count < 5 ? count : 5, 1, false);
    if(result != Nuki::CmdResult::Success)
    {
        return;
    }
    delay(1000);

    std::list<NukiLock::LogEntry> log;
    _nukiLock.getLogEntries(&log);

    if(log.size() > 0)
    {
         _network->publishAuthorizationInfo(log);
    }
}

void NukiWrapper::updateKeypad()
{
    Nuki::CmdResult result = _nukiLock.retrieveKeypadEntries(0, 0xffff);
    if(result == 1)
    {
        std::list<NukiLock::KeypadEntry> entries;
        _nukiLock.getKeypadEntries(&entries);

        entries.sort([](const NukiLock::KeypadEntry& a, const NukiLock::KeypadEntry& b) { return a.codeId < b.codeId; });

        uint keypadCount = entries.size();
        if(keypadCount > _maxKeypadCodeCount)
        {
            _maxKeypadCodeCount = keypadCount;
            _preferences->putUInt(preference_max_keypad_code_count, _maxKeypadCodeCount);
        }

        _network->publishKeypad(entries, _maxKeypadCodeCount);

        _keypadCodeIds.clear();
        _keypadCodeIds.reserve(entries.size());
        for(const auto& entry : entries)
        {
            _keypadCodeIds.push_back(entry.codeId);
        }
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


void NukiWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    nukiInst->onKeypadCommandReceived(command, id, name, code, enabled);
}


void NukiWrapper::onConfigUpdateReceived(const char *topic, const char *value)
{
    if(strcmp(topic, mqtt_topic_config_button_enabled) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiConfigValid || _nukiConfig.buttonEnabled == newValue) return;
        _nukiLock.enableButton(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    if(strcmp(topic, mqtt_topic_config_led_enabled) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiConfigValid || _nukiConfig.ledEnabled == newValue) return;
        _nukiLock.enableLedFlash(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_led_brightness) == 0)
    {
        int newValue = atoi(value);
        if(!_nukiConfigValid || _nukiConfig.ledBrightness == newValue) return;
        _nukiLock.setLedBrightness(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    if(strcmp(topic, mqtt_topic_config_single_lock) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiConfigValid || _nukiConfig.singleLock == newValue) return;
        _nukiLock.enableSingleLock(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_auto_unlock) == 0)
    {
        bool newValue = !(atoi(value) > 0);
        if(!_nukiAdvancedConfigValid || _nukiAdvancedConfig.autoUnLockDisabled == newValue) return;
        _nukiLock.disableAutoUnlock(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_auto_lock) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiAdvancedConfigValid || _nukiAdvancedConfig.autoLockEnabled == newValue) return;
        _nukiLock.enableAutoLock(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    else if(strcmp(topic, mqtt_topic_config_auto_lock) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiAdvancedConfigValid || _nukiAdvancedConfig.autoLockEnabled == newValue) return;
        _nukiLock.enableAutoLock(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
}

void NukiWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    if(!_hasKeypad)
    {
        if(_configRead)
        {
            _network->publishKeypadCommandResult("KeypadNotAvailable");
        }
        return;
    }
    if(!_keypadEnabled)
    {
        return;
    }

    bool idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), id) != _keypadCodeIds.end();
    int codeInt = code.toInt();
    bool codeValid = codeInt > 100000 && codeInt < 1000000 && (code.indexOf('0') == -1);
    NukiLock::CmdResult result = (NukiLock::CmdResult)-1;

    if(strcmp(command, "add") == 0)
    {
        if(name == "" || name == "--")
        {
            _network->publishKeypadCommandResult("MissingParameterName");
            return;
        }
        if(codeInt == 0)
        {
            _network->publishKeypadCommandResult("MissingParameterCode");
            return;
        }
        if(!codeValid)
        {
            _network->publishKeypadCommandResult("CodeInvalid");
            return;
        }

        NukiLock::NewKeypadEntry entry;
        memset(&entry, 0, sizeof(entry));
        size_t nameLen = name.length();
        memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
        entry.code = codeInt;
        result = _nukiLock.addKeypadEntry(entry);
        Serial.print("Add keypad code: "); Serial.println((int)result);
        updateKeypad();
    }
    else if(strcmp(command, "delete") == 0)
    {
        if(!idExists)
        {
            _network->publishKeypadCommandResult("UnknownId");
            return;
        }
        result = _nukiLock.deleteKeypadEntry(id);
        Serial.print("Delete keypad code: "); Serial.println((int)result);
        updateKeypad();
    }
    else if(strcmp(command, "update") == 0)
    {
        if(name == "" || name == "--")
        {
            _network->publishKeypadCommandResult("MissingParameterName");
            return;
        }
        if(codeInt == 0)
        {
            _network->publishKeypadCommandResult("MissingParameterCode");
            return;
        }
        if(!codeValid)
        {
            _network->publishKeypadCommandResult("CodeInvalid");
            return;
        }
        if(!idExists)
        {
            _network->publishKeypadCommandResult("UnknownId");
            return;
        }

        NukiLock::UpdatedKeypadEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.codeId = id;
        size_t nameLen = name.length();
        memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
        entry.code = codeInt;
        entry.enabled = enabled == 0 ? 0 : 1;
        result = _nukiLock.updateKeypadEntry(entry);
        Serial.print("Update keypad code: "); Serial.println((int)result);
        updateKeypad();
    }
    else if(command == "--")
    {
        return;
    }
    else
    {
        _network->publishKeypadCommandResult("UnknownCommand");
        return;
    }

    if((int)result != -1)
    {
        char resultStr[15];
        memset(&resultStr, 0, sizeof(resultStr));
        NukiLock::cmdResultToString(result, resultStr);
        _network->publishKeypadCommandResult(resultStr);
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

const bool NukiWrapper::hasKeypad()
{
    return _hasKeypad;
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
    Nuki::CmdResult result = _nukiLock.requestConfig(&_nukiConfig);
    _nukiConfigValid = result == Nuki::CmdResult::Success;
    Serial.println(result);
}

void NukiWrapper::readAdvancedConfig()
{
    Serial.print(F("Reading advanced config. Result: "));
    Nuki::CmdResult result = _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
    _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;
    Serial.println(result);
}

void NukiWrapper::setupHASS()
{
    if(!_nukiConfigValid) // only ask for config once to save battery life
    {
        Nuki::CmdResult result = _nukiLock.requestConfig(&_nukiConfig);
        _nukiConfigValid = result == Nuki::CmdResult::Success;
    }
    if (_nukiConfigValid)
    {
        String baseTopic = _preferences->getString(preference_mqtt_lock_path);
        char uidString[20];
        itoa(_nukiConfig.nukiId, uidString, 16);
        _network->publishHASSConfig("SmartLock",baseTopic.c_str(),(char*)_nukiConfig.name,uidString,"lock","unlock","unlatch","locked","unlocked");
    }
    else
    {
        Serial.println(F("Unable to setup HASS. Invalid config received."));
    }
}

void NukiWrapper::disableHASS()
{
    if(!_nukiConfigValid) // only ask for config once to save battery life
    {
        Nuki::CmdResult result = _nukiLock.requestConfig(&_nukiConfig);
        _nukiConfigValid = result == Nuki::CmdResult::Success;
    }
    if (_nukiConfigValid)
    {
        String baseTopic = _preferences->getString(preference_mqtt_lock_path);
        char uidString[20];
        itoa(_nukiConfig.nukiId, uidString, 16);
        _network->removeHASSConfig(uidString);
    }
    else
    {
        Serial.println(F("Unable to disable HASS. Invalid config received."));
    }
}
