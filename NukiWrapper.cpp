#include "NukiWrapper.h"
#include <RTOS.h>
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "Logger.h"
#include "RestartReason.h"
#include <NukiLockUtils.h>

NukiWrapper* nukiInst;
Preferences* nukiLockPreferences = nullptr;

NukiWrapper::NukiWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NetworkLock* network, Gpio* gpio, Preferences* preferences)
: _deviceName(deviceName),
  _deviceId(deviceId),
  _bleScanner(scanner),
  _nukiLock(deviceName, _deviceId->get()),
  _network(network),
  _gpio(gpio),
  _preferences(preferences)
{
    Log->print("Device id lock: ");
    Log->println(_deviceId->get());

    nukiInst = this;

    memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    _keyTurnerState.lockState = NukiLock::LockState::Undefined;

    network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
    network->setConfigUpdateReceivedCallback(nukiInst->onConfigUpdateReceivedCallback);
    network->setKeypadCommandReceivedCallback(nukiInst->onKeypadCommandReceivedCallback);
    network->setKeypadJsonCommandReceivedCallback(nukiInst->onKeypadJsonCommandReceivedCallback);

    _gpio->addCallback(NukiWrapper::gpioActionCallback);
}


NukiWrapper::~NukiWrapper()
{
    _bleScanner = nullptr;
}


void NukiWrapper::initialize(const bool& firstStart)
{

    _nukiLock.initialize();
    _nukiLock.registerBleScanner(_bleScanner);

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalConfig = _preferences->getInt(preference_query_interval_configuration);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);
    _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
    _keypadEnabled = _preferences->getBool(preference_keypad_info_enabled);
    _publishAuthData = _preferences->getBool(preference_publish_authdata);
    _maxKeypadCodeCount = _preferences->getUInt(preference_lock_max_keypad_code_count);
    _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
    _hassEnabled = _preferences->getString(preference_mqtt_hass_discovery) != "";
    _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries);
    _retryDelay = _preferences->getInt(preference_command_retry_delay);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;

    if(firstStart)
    {
        _preferences->putInt(preference_command_nr_of_retries, 3);
        _preferences->putInt(preference_command_retry_delay, 1000);
        _preferences->putInt(preference_restart_ble_beacon_lost, 60);
        _preferences->putBool(preference_admin_enabled, true);
        uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        _preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
    }

    if(_retryDelay <= 100)
    {
        _retryDelay = 100;
        _preferences->putInt(preference_command_retry_delay, _retryDelay);
    }

    if(_intervalLockstate == 0)
    {
        _intervalLockstate = 60 * 30;
        _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
    }
    if(_intervalConfig == 0)
    {
        _intervalConfig = 60 * 60;
        _preferences->putInt(preference_query_interval_configuration, _intervalConfig);
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
    if(_restartBeaconTimeout < 10)
    {
        _restartBeaconTimeout = -1;
        _preferences->putInt(preference_restart_ble_beacon_lost, _restartBeaconTimeout);
    }

    _nukiLock.setEventHandler(this);

    Log->print(F("Lock state interval: "));
    Log->print(_intervalLockstate);
    Log->print(F(" | Battery interval: "));
    Log->print(_intervalBattery);
    Log->print(F(" | Publish auth data: "));
    Log->println(_publishAuthData ? "yes" : "no");

    if(!_publishAuthData)
    {
        _clearAuthData = true;
    }
}

void NukiWrapper::update()
{
    if (!_paired)
    {
        Log->println(F("Nuki lock start pairing"));
        _network->publishBleAddress("");

        Nuki::AuthorizationIdType idType = _preferences->getBool(preference_register_as_app) ?
                                           Nuki::AuthorizationIdType::App :
                                           Nuki::AuthorizationIdType::Bridge;

        if (_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success)
        {
            Log->println(F("Nuki paired"));
            _paired = true;
            _network->publishBleAddress(_nukiLock.getBleAddress().toString());
        }
        else
        {
            delay(200);
            return;
        }
    }

    unsigned long ts = millis();
    unsigned long lastReceivedBeaconTs = _nukiLock.getLastReceivedBeaconTs();
    uint8_t queryCommands = _network->queryCommands();

    if(_restartBeaconTimeout > 0 &&
       ts > 60000 &&
       lastReceivedBeaconTs > 0 &&
       _disableBleWatchdogTs < ts &&
       (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
    {
        Log->print("No BLE beacon received from the lock for ");
        Log->print((millis() - _nukiLock.getLastReceivedBeaconTs()) / 1000);
        Log->println(" seconds, restarting device.");
        delay(200);
        restartEsp(RestartReason::BLEBeaconWatchdog);
    }

    _nukiLock.updateConnectionState();

    if(_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        _statusUpdated = false;
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        updateKeyTurnerState();
    }
    if(_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs || (queryCommands & QUERY_COMMAND_BATTERY) > 0)
    {
        _nextBatteryReportTs = ts + _intervalBattery * 1000;
        updateBatteryState();
    }
    if(_nextConfigUpdateTs == 0 || ts > _nextConfigUpdateTs || (queryCommands & QUERY_COMMAND_CONFIG) > 0)
    {
        _nextConfigUpdateTs = ts + _intervalConfig * 1000;
        updateConfig();
        if(_hassEnabled && !_hassSetupCompleted)
        {
            setupHASS();
        }
    }
    if(_hassEnabled && _configRead && _network->reconnected())
    {
        setupHASS();
    }
    if(_rssiPublishInterval > 0 && (_nextRssiTs == 0 || ts > _nextRssiTs))
    {
        _nextRssiTs = ts + _rssiPublishInterval;

        int rssi = _nukiLock.getRssi();
        if(rssi != _lastRssi)
        {
            _network->publishRssi(rssi);
            _lastRssi = rssi;
        }
    }

    if(_hasKeypad && _keypadEnabled && (_nextKeypadUpdateTs == 0 || ts > _nextKeypadUpdateTs || (queryCommands & QUERY_COMMAND_KEYPAD) > 0))
    {
        _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
        updateKeypad();
    }

    if(_nextLockAction != (NukiLock::LockAction)0xff && ts > _nextRetryTs)
    {
        Nuki::CmdResult cmdResult = _nukiLock.lockAction(_nextLockAction, 0, 0);

        char resultStr[15] = {0};
        NukiLock::cmdResultToString(cmdResult, resultStr);

        _network->publishCommandResult(resultStr);

        Log->print(F("Lock action result: "));
        Log->println(resultStr);

        if(cmdResult == Nuki::CmdResult::Success)
        {
            _retryCount = 0;
            _nextLockAction = (NukiLock::LockAction) 0xff;
            _network->publishRetry("--");
            if (_intervalLockstate > 10)
            {
                _nextLockStateUpdateTs = ts + 10 * 1000;
            }
        }
        else
        {
            if(_retryCount < _nrOfRetries)
            {
                Log->print(F("Lock: Last command failed, retrying after "));
                Log->print(_retryDelay);
                Log->print(F(" milliseconds. Retry "));
                Log->print(_retryCount + 1);
                Log->print(" of ");
                Log->println(_nrOfRetries);

                _network->publishRetry(std::to_string(_retryCount + 1));

                _nextRetryTs = millis() + _retryDelay;

                ++_retryCount;
            }
            else
            {
                Log->println(F("Lock: Maximum number of retries exceeded, aborting."));
                _network->publishRetry("failed");
                _retryCount = 0;
                _nextRetryTs = 0;
                _nextLockAction = (NukiLock::LockAction) 0xff;
            }
        }
        postponeBleWatchdog();
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

void NukiWrapper::lockngo()
{
    _nextLockAction = NukiLock::LockAction::LockNgo;
}

void NukiWrapper::lockngounlatch()
{
    _nextLockAction = NukiLock::LockAction::LockNgoUnlatch;
}

bool NukiWrapper::isPinSet()
{
    return _nukiLock.getSecurityPincode() != 0;
}

void NukiWrapper::setPin(const uint16_t pin)
{
    _nukiLock.saveSecurityPincode(pin);
}

void NukiWrapper::unpair()
{
    _nukiLock.unPairNuki();
    _deviceId->assignNewId();
    _preferences->remove(preference_nuki_id_lock);
    _paired = false;
}

void NukiWrapper::updateKeyTurnerState()
{
    Log->print(F("Querying lock state: "));
    Nuki::CmdResult result =_nukiLock.requestKeyTurnerState(&_keyTurnerState);

    char resultStr[15];
    memset(&resultStr, 0, sizeof(resultStr));
    NukiLock::cmdResultToString(result, resultStr);
    _network->publishLockstateCommandResult(resultStr);

    if(result != Nuki::CmdResult::Success)
    {
        _retryLockstateCount++;
        postponeBleWatchdog();
        if(_retryLockstateCount < _nrOfRetries)
        {
            _nextLockStateUpdateTs = millis() + _retryDelay;
        }
        return;
    }

    _retryLockstateCount = 0;

    if(_publishAuthData)
    {
        updateAuthData();
    }

    _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);
    updateGpioOutputs();

    char lockStateStr[20];
    lockstateToString(_keyTurnerState.lockState, lockStateStr);
    Log->println(lockStateStr);

    postponeBleWatchdog();
}

void NukiWrapper::updateBatteryState()
{
    Log->print("Querying lock battery state: ");
    Nuki::CmdResult result = _nukiLock.requestBatteryReport(&_batteryReport);
    printCommandResult(result);
    if(result == Nuki::CmdResult::Success)
    {
        _network->publishBatteryReport(_batteryReport);
    }
    postponeBleWatchdog();
}

void NukiWrapper::updateConfig()
{
    readConfig();
    readAdvancedConfig();
    _configRead = true;
    bool expectedConfig = true;

    if(_nukiConfigValid)
    {
        if(_preferences->getUInt(preference_nuki_id_lock, 0) == 0  || _retryConfigCount == 10)
        {
            _preferences->putUInt(preference_nuki_id_lock, _nukiConfig.nukiId);
        }

        if(_preferences->getUInt(preference_nuki_id_lock, 0) == _nukiConfig.nukiId)
        {
            _hasKeypad = _nukiConfig.hasKeypad > 0 || _nukiConfig.hasKeypadV2 > 0;
            _firmwareVersion = std::to_string(_nukiConfig.firmwareVersion[0]) + "." + std::to_string(_nukiConfig.firmwareVersion[1]) + "." + std::to_string(_nukiConfig.firmwareVersion[2]);
            _hardwareVersion = std::to_string(_nukiConfig.hardwareRevision[0]) + "." + std::to_string(_nukiConfig.hardwareRevision[1]);
            _network->publishConfig(_nukiConfig);
            _retryConfigCount = 0;
        }
        else
        {
            expectedConfig = false;
            ++_retryConfigCount;
        }
    }
    else
    {
        expectedConfig = false;
        ++_retryConfigCount;
    }
    if(_nukiAdvancedConfigValid && _preferences->getUInt(preference_nuki_id_lock, 0) == _nukiConfig.nukiId)
    {
        _network->publishAdvancedConfig(_nukiAdvancedConfig);
        _retryConfigCount = 0;
    }
    else
    {
        expectedConfig = false;
        ++_retryConfigCount;
    }
    if(!expectedConfig && _retryConfigCount < 11)
    {
        unsigned long ts = millis();
        _nextConfigUpdateTs = ts + 60000;
    }
}

void NukiWrapper::updateAuthData()
{
    if(_nukiLock.getSecurityPincode() == 0) return;

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
    postponeBleWatchdog();
}

void NukiWrapper::updateKeypad()
{
    if(!_preferences->getBool(preference_keypad_info_enabled)) return;

    Log->print(F("Querying lock keypad: "));
    Nuki::CmdResult result = _nukiLock.retrieveKeypadEntries(0, 0xffff);
    printCommandResult(result);
    if(result == Nuki::CmdResult::Success)
    {
        std::list<NukiLock::KeypadEntry> entries;
        _nukiLock.getKeypadEntries(&entries);

        entries.sort([](const NukiLock::KeypadEntry& a, const NukiLock::KeypadEntry& b) { return a.codeId < b.codeId; });

        uint keypadCount = entries.size();
        if(keypadCount > _maxKeypadCodeCount)
        {
            _maxKeypadCodeCount = keypadCount;
            _preferences->putUInt(preference_lock_max_keypad_code_count, _maxKeypadCodeCount);
        }

        _network->publishKeypad(entries, _maxKeypadCodeCount);

        _keypadCodeIds.clear();
        _keypadCodeIds.reserve(entries.size());
        for(const auto& entry : entries)
        {
            _keypadCodeIds.push_back(entry.codeId);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::postponeBleWatchdog()
{
    _disableBleWatchdogTs = millis() + 15000;
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

LockActionResult NukiWrapper::onLockActionReceivedCallback(const char *value)
{
    NukiLock::LockAction action = nukiInst->lockActionToEnum(value);

    if((int)action == 0xff)
    {
        return LockActionResult::UnknownAction;
    }

    nukiLockPreferences = new Preferences();
    nukiLockPreferences->begin("nukihub", true);
    uint32_t aclPrefs[17];
    nukiLockPreferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    if((action == NukiLock::LockAction::Lock && (int)aclPrefs[0] == 1) || (action == NukiLock::LockAction::Unlock && (int)aclPrefs[1] == 1) || (action == NukiLock::LockAction::Unlatch && (int)aclPrefs[2] == 1) || (action == NukiLock::LockAction::LockNgo && (int)aclPrefs[3] == 1) || (action == NukiLock::LockAction::LockNgoUnlatch && (int)aclPrefs[4] == 1) || (action == NukiLock::LockAction::FullLock && (int)aclPrefs[5] == 1) || (action == NukiLock::LockAction::FobAction1 && (int)aclPrefs[6] == 1) || (action == NukiLock::LockAction::FobAction2 && (int)aclPrefs[7] == 1) || (action == NukiLock::LockAction::FobAction3 && (int)aclPrefs[8] == 1))
    {
        nukiLockPreferences->end();
        nukiInst->_nextLockAction = action;
        return LockActionResult::Success;
    }

    nukiLockPreferences->end();
    return LockActionResult::AccessDenied;
}

void NukiWrapper::onConfigUpdateReceivedCallback(const char *topic, const char *value)
{
    nukiInst->onConfigUpdateReceived(topic, value);
}

void NukiWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    nukiInst->onKeypadCommandReceived(command, id, name, code, enabled);
}

void NukiWrapper::onKeypadJsonCommandReceivedCallback(const char *value)
{
    nukiInst->onKeypadJsonCommandReceived(value);
}

void NukiWrapper::gpioActionCallback(const GpioAction &action, const int& pin)
{
    switch(action)
    {
        case GpioAction::Lock:
            nukiInst->lock();
            break;
        case GpioAction::Unlock:
            nukiInst->unlock();
            break;
        case GpioAction::Unlatch:
            nukiInst->unlatch();
            break;
        case GpioAction::LockNgo:
            nukiInst->lockngo();
            break;
        case GpioAction::LockNgoUnlatch:
            nukiInst->lockngounlatch();
            break;
    }
}

void NukiWrapper::onConfigUpdateReceived(const char *topic, const char *value)
{
    if(!_preferences->getBool(preference_admin_enabled)) return;

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
}

void NukiWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    if(!_preferences->getBool(preference_keypad_control_enabled))
    {
        _network->publishKeypadCommandResult("KeypadControlDisabled");
        return;
    }

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
        Log->print("Add keypad code: "); Log->println((int)result);
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
        Log->print("Delete keypad code: "); Log->println((int)result);
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
        Log->print("Update keypad code: "); Log->println((int)result);
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

void NukiWrapper::onKeypadJsonCommandReceived(const char *value)
{
    if(_nukiLock.getSecurityPincode() == 0)
    {
        _network->publishKeypadJsonCommandResult("noPinSet");
        return;
    }

    if(!_preferences->getBool(preference_keypad_control_enabled))
    {
        _network->publishKeypadJsonCommandResult("keypadControlDisabled");
        return;
    }

    if(!_hasKeypad)
    {
        if(_configRead && _nukiConfigValid)
        {
            _network->publishKeypadJsonCommandResult("keypadNotAvailable");
            return;
        }

        updateConfig();

        while (!_nukiConfigValid && _retryConfigCount < 11)
        {
            updateConfig();
        }

        if(_configRead && _nukiConfigValid)
        {
            _network->publishKeypadJsonCommandResult("keypadNotAvailable");
            return;
        }

        _network->publishKeypadJsonCommandResult("invalidConfig");
        return;
    }

    if(!_keypadEnabled)
    {
        _network->publishKeypadJsonCommandResult("keypadDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        _network->publishKeypadJsonCommandResult("invalidJson");
        return;
    }

    Nuki::CmdResult result = (Nuki::CmdResult)-1;

    const char *action = json["action"].as<const char*>();
    uint16_t codeId = json["codeId"].as<unsigned int>();
    uint32_t code = json["code"].as<unsigned int>();
    String codeStr = json["code"].as<String>();
    const char *name = json["name"].as<const char*>();
    uint8_t enabled = json["enabled"].as<unsigned int>();
    uint8_t timeLimited = json["timeLimited"].as<unsigned int>();
    const char *allowedFrom = json["allowedFrom"].as<const char*>();
    const char *allowedUntil = json["allowedUntil"].as<const char*>();
    String allowedWeekdays = json["allowedWeekdays"].as<String>();
    const char *allowedFromTime = json["allowedFromTime"].as<const char*>();
    const char *allowedUntilTime = json["allowedUntilTime"].as<const char*>();

    if(action)
    {
        bool idExists = false;

        if(codeId)
        {
            idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), codeId) != _keypadCodeIds.end();
        }

        if(strcmp(action, "delete") == 0) {
            if(idExists)
            {
                result = _nukiLock.deleteKeypadEntry(codeId);
                Log->print("Delete keypad code: ");
                Log->println((int)result);
            }
            else
            {
                _network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                return;
            }
        }
        else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
        {
            if(!name)
            {
                _network->publishKeypadJsonCommandResult("noNameSet");
                return;
            }

            if(code)
            {
                bool codeValid = code > 100000 && code < 1000000 && (codeStr.indexOf('0') == -1);

                if (!codeValid)
                {
                    _network->publishKeypadJsonCommandResult("noValidCodeSet");
                    return;
                }
            }
            else
            {
                _network->publishKeypadJsonCommandResult("noCodeSet");
                return;
            }

            unsigned int allowedFromAr[6];
            unsigned int allowedUntilAr[6];
            unsigned int allowedFromTimeAr[2];
            unsigned int allowedUntilTimeAr[2];
            uint8_t allowedWeekdaysInt = 0;

            if(timeLimited == 1 && enabled != 0)
            {
                if(allowedFrom)
                {
                    if(strlen(allowedFrom) == 19)
                    {
                        String allowedFromStr = allowedFrom;
                        allowedFromAr[0] = (uint16_t)allowedFromStr.substring(0, 4).toInt();
                        allowedFromAr[1] = (uint8_t)allowedFromStr.substring(5, 7).toInt();
                        allowedFromAr[2] = (uint8_t)allowedFromStr.substring(8, 10).toInt();
                        allowedFromAr[3] = (uint8_t)allowedFromStr.substring(11, 13).toInt();
                        allowedFromAr[4] = (uint8_t)allowedFromStr.substring(14, 16).toInt();
                        allowedFromAr[5] = (uint8_t)allowedFromStr.substring(17, 19).toInt();

                        if(allowedFromAr[0] < 2000 || allowedFromAr[0] > 3000 || allowedFromAr[1] < 1 || allowedFromAr[1] > 12 || allowedFromAr[2] < 1 || allowedFromAr[2] > 31 || allowedFromAr[3] < 0 || allowedFromAr[3] > 23 || allowedFromAr[4] < 0 || allowedFromAr[4] > 59 || allowedFromAr[5] < 0 || allowedFromAr[5] > 59)
                        {
                            _network->publishKeypadJsonCommandResult("invalidAllowedFrom");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishKeypadJsonCommandResult("invalidAllowedFrom");
                        return;
                    }
                }

                if(allowedUntil)
                {
                    if(strlen(allowedUntil) == 19)
                    {
                        String allowedUntilStr = allowedUntil;
                        allowedUntilAr[0] = (uint16_t)allowedUntilStr.substring(0, 4).toInt();
                        allowedUntilAr[1] = (uint8_t)allowedUntilStr.substring(5, 7).toInt();
                        allowedUntilAr[2] = (uint8_t)allowedUntilStr.substring(8, 10).toInt();
                        allowedUntilAr[3] = (uint8_t)allowedUntilStr.substring(11, 13).toInt();
                        allowedUntilAr[4] = (uint8_t)allowedUntilStr.substring(14, 16).toInt();
                        allowedUntilAr[5] = (uint8_t)allowedUntilStr.substring(17, 19).toInt();

                        if(allowedUntilAr[0] < 2000 || allowedUntilAr[0] > 3000 || allowedUntilAr[1] < 1 || allowedUntilAr[1] > 12 || allowedUntilAr[2] < 1 || allowedUntilAr[2] > 31 || allowedUntilAr[3] < 0 || allowedUntilAr[3] > 23 || allowedUntilAr[4] < 0 || allowedUntilAr[4] > 59 || allowedUntilAr[5] < 0 || allowedUntilAr[5] > 59)
                        {
                            _network->publishKeypadJsonCommandResult("invalidAllowedUntil");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishKeypadJsonCommandResult("invalidAllowedUntil");
                        return;
                    }
                }

                if(allowedFromTime)
                {
                    if(strlen(allowedFromTime) == 5)
                    {
                        String allowedFromTimeStr = allowedFromTime;
                        allowedFromTimeAr[0] = (uint8_t)allowedFromTimeStr.substring(0, 2).toInt();
                        allowedFromTimeAr[1] = (uint8_t)allowedFromTimeStr.substring(3, 5).toInt();

                        if(allowedFromTimeAr[0] < 0 || allowedFromTimeAr[0] > 23 || allowedFromTimeAr[1] < 0 || allowedFromTimeAr[1] > 59)
                        {
                            _network->publishKeypadJsonCommandResult("invalidAllowedFromTime");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishKeypadJsonCommandResult("invalidAllowedFromTime");
                        return;
                    }
                }

                if(allowedUntilTime)
                {
                    if(strlen(allowedUntilTime) == 5)
                    {
                        String allowedUntilTimeStr = allowedUntilTime;
                        allowedUntilTimeAr[0] = (uint8_t)allowedUntilTimeStr.substring(0, 2).toInt();
                        allowedUntilTimeAr[1] = (uint8_t)allowedUntilTimeStr.substring(3, 5).toInt();

                        if(allowedUntilTimeAr[0] < 0 || allowedUntilTimeAr[0] > 23 || allowedUntilTimeAr[1] < 0 || allowedUntilTimeAr[1] > 59)
                        {
                            _network->publishKeypadJsonCommandResult("invalidAllowedUntilTime");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishKeypadJsonCommandResult("invalidAllowedUntilTime");
                        return;
                    }
                }

                if(allowedWeekdays.indexOf("mon") >= 0) allowedWeekdaysInt += 64;
                if(allowedWeekdays.indexOf("tue") >= 0) allowedWeekdaysInt += 32;
                if(allowedWeekdays.indexOf("wed") >= 0) allowedWeekdaysInt += 16;
                if(allowedWeekdays.indexOf("thu") >= 0) allowedWeekdaysInt += 8;
                if(allowedWeekdays.indexOf("fri") >= 0) allowedWeekdaysInt += 4;
                if(allowedWeekdays.indexOf("sat") >= 0) allowedWeekdaysInt += 2;
                if(allowedWeekdays.indexOf("sun") >= 0) allowedWeekdaysInt += 1;
            }

            if(strcmp(action, "add") == 0)
            {
                NukiLock::NewKeypadEntry entry;
                memset(&entry, 0, sizeof(entry));
                size_t nameLen = strlen(name);
                memcpy(&entry.name, name, nameLen > 20 ? 20 : nameLen);
                entry.code = code;
                entry.timeLimited = timeLimited == 1 ? 1 : 0;

                if(allowedFrom)
                {
                    entry.allowedFromYear = allowedFromAr[0];
                    entry.allowedFromMonth = allowedFromAr[1];
                    entry.allowedFromDay = allowedFromAr[2];
                    entry.allowedFromHour = allowedFromAr[3];
                    entry.allowedFromMin = allowedFromAr[4];
                    entry.allowedFromSec = allowedFromAr[5];
                }

                if(allowedUntil)
                {
                    entry.allowedUntilYear = allowedUntilAr[0];
                    entry.allowedUntilMonth = allowedUntilAr[1];
                    entry.allowedUntilDay = allowedUntilAr[2];
                    entry.allowedUntilHour = allowedUntilAr[3];
                    entry.allowedUntilMin = allowedUntilAr[4];
                    entry.allowedUntilSec = allowedUntilAr[5];
                }

                entry.allowedWeekdays = allowedWeekdaysInt;

                if(allowedFromTime)
                {
                    entry.allowedFromTimeHour = allowedFromTimeAr[0];
                    entry.allowedFromTimeMin = allowedFromTimeAr[1];
                }

                if(allowedUntilTime)
                {
                    entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                    entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                }

                result = _nukiLock.addKeypadEntry(entry);
                Log->print("Add keypad code: ");
                Log->println((int)result);
            }
            else if (strcmp(action, "update") == 0)
            {
                NukiLock::UpdatedKeypadEntry entry;
                memset(&entry, 0, sizeof(entry));
                entry.codeId = codeId;
                size_t nameLen = strlen(name);
                memcpy(&entry.name, name, nameLen > 20 ? 20 : nameLen);
                entry.code = code;
                entry.enabled = enabled == 0 ? 0 : 1;
                entry.timeLimited = timeLimited == 1 ? 1 : 0;

                if(allowedFrom)
                {
                    entry.allowedFromYear = allowedFromAr[0];
                    entry.allowedFromMonth = allowedFromAr[1];
                    entry.allowedFromDay = allowedFromAr[2];
                    entry.allowedFromHour = allowedFromAr[3];
                    entry.allowedFromMin = allowedFromAr[4];
                    entry.allowedFromSec = allowedFromAr[5];
                }

                if(allowedUntil)
                {
                    entry.allowedUntilYear = allowedUntilAr[0];
                    entry.allowedUntilMonth = allowedUntilAr[1];
                    entry.allowedUntilDay = allowedUntilAr[2];
                    entry.allowedUntilHour = allowedUntilAr[3];
                    entry.allowedUntilMin = allowedUntilAr[4];
                    entry.allowedUntilSec = allowedUntilAr[5];
                }

                entry.allowedWeekdays = allowedWeekdaysInt;

                if(allowedFromTime)
                {
                    entry.allowedFromTimeHour = allowedFromTimeAr[0];
                    entry.allowedFromTimeMin = allowedFromTimeAr[1];
                }

                if(allowedUntilTime)
                {
                    entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                    entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                }

                result = _nukiLock.updateKeypadEntry(entry);
                Log->print("Update keypad code: ");
                Log->println((int)result);
            }
        }
        else
        {
            _network->publishKeypadJsonCommandResult("invalidAction");
            return;
        }

        updateKeypad();

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            NukiLock::cmdResultToString(result, resultStr);
            _network->publishKeypadJsonCommandResult(resultStr);
        }
    }
    else
    {
        _network->publishKeypadJsonCommandResult("noActionSet");
        return;
    }
}

const NukiLock::KeyTurnerState &NukiWrapper::keyTurnerState()
{
    return _keyTurnerState;
}

const bool NukiWrapper::isPaired() const
{
    return _paired;
}

const bool NukiWrapper::hasKeypad() const
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
    Log->print(F("Reading config. Result: "));
    Nuki::CmdResult result = _nukiLock.requestConfig(&_nukiConfig);
    _nukiConfigValid = result == Nuki::CmdResult::Success;
    char resultStr[20];
    NukiLock::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}

void NukiWrapper::readAdvancedConfig()
{
    Log->print(F("Reading advanced config. Result: "));
    Nuki::CmdResult result = _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
    _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;
    char resultStr[20];
    NukiLock::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}

void NukiWrapper::setupHASS()
{
    if(!_nukiConfigValid) return;
    if(_preferences->getUInt(preference_nuki_id_lock, 0) != _nukiConfig.nukiId) return;

    String baseTopic = _preferences->getString(preference_mqtt_lock_path);
    char uidString[20];
    itoa(_nukiConfig.nukiId, uidString, 16);

    _network->publishHASSConfig("SmartLock", baseTopic.c_str(),(char*)_nukiConfig.name, uidString, hasDoorSensor(), _hasKeypad, _publishAuthData, "lock", "unlock", "unlatch");
    _hassSetupCompleted = true;

    Log->println("HASS setup for lock completed.");
}

bool NukiWrapper::hasDoorSensor() const
{
    return _keyTurnerState.doorSensorState == Nuki::DoorSensorState::DoorClosed ||
           _keyTurnerState.doorSensorState == Nuki::DoorSensorState::DoorOpened ||
           _keyTurnerState.doorSensorState == Nuki::DoorSensorState::Calibrating;
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
        char uidString[20];
        itoa(_nukiConfig.nukiId, uidString, 16);
        _network->removeHASSConfig(uidString);
    }
    else
    {
        Log->println(F("Unable to disable HASS. Invalid config received."));
    }
}

const BLEAddress NukiWrapper::getBleAddress() const
{
    return _nukiLock.getBleAddress();
}

void NukiWrapper::printCommandResult(Nuki::CmdResult result)
{
    char resultStr[15];
    NukiLock::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}

std::string NukiWrapper::firmwareVersion() const
{
    return _firmwareVersion;
}

std::string NukiWrapper::hardwareVersion() const
{
    return _hardwareVersion;
}

void NukiWrapper::disableWatchdog()
{
    _restartBeaconTimeout = -1;
}

void NukiWrapper::updateGpioOutputs()
{
    using namespace NukiLock;

    const auto& pinConfiguration = _gpio->pinConfiguration();

    const LockState& lockState = _keyTurnerState.lockState;

    for(const auto& entry : pinConfiguration)
    {
        switch(entry.role)
        {
            case PinRole::OutputHighLocked:
                _gpio->setPinOutput(entry.pin, lockState == LockState::Locked || lockState == LockState::Locking ? HIGH : LOW);
                break;
            case PinRole::OutputHighUnlocked:
                _gpio->setPinOutput(entry.pin, lockState == LockState::Locked || lockState == LockState::Locking ? LOW : HIGH);
                break;
            case PinRole::OutputHighMotorBlocked:
                _gpio->setPinOutput(entry.pin, lockState == LockState::MotorBlocked  ? HIGH : LOW);
                break;
        }
    }
}

