#include "NukiOpenerWrapper.h"
#include <RTOS.h>
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "Logger.h"
#include "RestartReason.h"
#include <NukiOpenerUtils.h>

NukiOpenerWrapper* nukiOpenerInst;

NukiOpenerWrapper::NukiOpenerWrapper(const std::string& deviceName, uint32_t id, BleScanner::Scanner* scanner,  NetworkOpener* network, Preferences* preferences)
: _deviceName(deviceName),
  _nukiOpener(deviceName, id),
  _bleScanner(scanner),
  _network(network),
  _preferences(preferences)
{
    nukiOpenerInst = this;

    memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    _keyTurnerState.lockState = NukiOpener::LockState::Undefined;

    network->setLockActionReceivedCallback(nukiOpenerInst->onLockActionReceivedCallback);
    network->setConfigUpdateReceivedCallback(nukiOpenerInst->onConfigUpdateReceivedCallback);
    network->setKeypadCommandReceivedCallback(nukiOpenerInst->onKeypadCommandReceivedCallback);
}


NukiOpenerWrapper::~NukiOpenerWrapper()
{
    _bleScanner = nullptr;
}


void NukiOpenerWrapper::initialize()
{
    _nukiOpener.initialize();
    _nukiOpener.registerBleScanner(_bleScanner);

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalConfig = _preferences->getInt(preference_query_interval_configuration);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);
    _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
    _keypadEnabled = _preferences->getBool(preference_keypad_control_enabled);
    _publishAuthData = _preferences->getBool(preference_publish_authdata);
    _maxKeypadCodeCount = _preferences->getUInt(preference_opener_max_keypad_code_count);
    _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
    _hassEnabled = _preferences->getString(preference_mqtt_hass_discovery) != "";
    _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries);
    _retryDelay = _preferences->getInt(preference_command_retry_delay);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;

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
    /*
    if(_intervalKeypad == 0)
    {
        _intervalKeypad = 60 * 30;
        _preferences->putInt(preference_query_interval_keypad, _intervalKeypad);
    }
     */
    if(_restartBeaconTimeout < 10)
    {
        _restartBeaconTimeout = -1;
        _preferences->putInt(preference_restart_ble_beacon_lost, _restartBeaconTimeout);
    }

    _nukiOpener.setEventHandler(this);

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

void NukiOpenerWrapper::update()
{
    if (!_paired)
    {
        Log->println(F("Nuki opener start pairing"));
        _network->publishBleAddress("");

        Nuki::AuthorizationIdType idType = _preferences->getBool(preference_register_as_app) ?
                                           Nuki::AuthorizationIdType::App :
                                           Nuki::AuthorizationIdType::Bridge;

        if (_nukiOpener.pairNuki(idType) == NukiOpener::PairingResult::Success)
        {
            Log->println(F("Nuki opener paired"));
            _paired = true;
            _network->publishBleAddress(_nukiOpener.getBleAddress().toString());
        }
        else
        {
            delay(200);
            return;
        }
    }

    unsigned long ts = millis();
    unsigned long lastReceivedBeaconTs = _nukiOpener.getLastReceivedBeaconTs();
    uint8_t queryCommands = _network->queryCommands();

    if(_restartBeaconTimeout > 0 &&
       ts > 60000 &&
       lastReceivedBeaconTs > 0 &&
       (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
    {
        Log->print("No BLE beacon received from the opener for ");
        Log->print((millis() - _nukiOpener.getLastReceivedBeaconTs()) / 1000);
        Log->println(" seconds, restarting device.");
        delay(200);
        restartEsp(RestartReason::BLEBeaconWatchdog);
    }

    _nukiOpener.updateConnectionState();

    if(_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        updateKeyTurnerState();
        _statusUpdated = false;
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

        int rssi = _nukiOpener.getRssi();
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

    if(_nextLockAction != (NukiOpener::LockAction)0xff && ts > _nextRetryTs)
    {
        Nuki::CmdResult cmdResult = _nukiOpener.lockAction(_nextLockAction, 0, 0);

        char resultStr[15] = {0};
        NukiOpener::cmdResultToString(cmdResult, resultStr);

        _network->publishCommandResult(resultStr);

        Log->print(F("Lock action result: "));
        Log->println(resultStr);

        if(cmdResult == Nuki::CmdResult::Success)
        {
            _retryCount = 0;
            _nextLockAction = (NukiOpener::LockAction) 0xff;
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
                Log->print(F("Opener: Last command failed, retrying after "));
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
                Log->println(F("Opener: Maximum number of retries exceeded, aborting."));
                _network->publishRetry("failed");
                _retryCount = 0;
                _nextRetryTs = 0;
                _nextLockAction = (NukiOpener::LockAction) 0xff;
            }
        }
        postponeBleWatchdog();
    }

    if(_clearAuthData)
    {
        _network->clearAuthorizationInfo();
        _clearAuthData = false;
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(NukiOpener::OpenerState));
}

bool NukiOpenerWrapper::isPinSet()
{
    return _nukiOpener.getSecurityPincode() != 0;
}

void NukiOpenerWrapper::setPin(const uint16_t pin)
{
    _nukiOpener.saveSecurityPincode(pin);
}

void NukiOpenerWrapper::unpair()
{
    _nukiOpener.unPairNuki();
    _paired = false;
}

void NukiOpenerWrapper::updateKeyTurnerState()
{
    Log->print(F("Querying opener state: "));
    Nuki::CmdResult result =_nukiOpener.requestOpenerState(&_keyTurnerState);
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

    if(_statusUpdated &&
        _keyTurnerState.lockState == NukiOpener::LockState::Locked &&
        _lastKeyTurnerState.lockState == NukiOpener::LockState::Locked &&
        _lastKeyTurnerState.nukiState == _keyTurnerState.nukiState)
    {
        Log->println(F("Nuki opener: Ring detected"));
        _network->publishRing();
    }
    else
    {
        _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);

        if(_keyTurnerState.nukiState == NukiOpener::State::ContinuousMode)
        {
            Log->println(F("Continuous Mode"));
        }
        else
        {
            char lockStateStr[20];
            lockstateToString(_keyTurnerState.lockState, lockStateStr);
            Log->println(lockStateStr);
        }
    }

    if(_publishAuthData)
    {
        updateAuthData();
    }

    postponeBleWatchdog();
}

void NukiOpenerWrapper::updateBatteryState()
{
    Log->print("Querying opener battery state: ");
    Nuki::CmdResult result = _nukiOpener.requestBatteryReport(&_batteryReport);
    printCommandResult(result);
    if(result == Nuki::CmdResult::Success)
    {
        _network->publishBatteryReport(_batteryReport);
    }
    postponeBleWatchdog();
}

void NukiOpenerWrapper::updateConfig()
{
    readConfig();
    readAdvancedConfig();
    _configRead = true;
    _hasKeypad = _nukiConfig.hasKeypad > 0;
    if(_nukiConfigValid)
    {
        _network->publishConfig(_nukiConfig);
    }
    if(_nukiAdvancedConfigValid)
    {
        _network->publishAdvancedConfig(_nukiAdvancedConfig);
    }
}

void NukiOpenerWrapper::updateAuthData()
{
    Nuki::CmdResult result = _nukiOpener.retrieveLogEntries(0, 0, 0, true);
    if(result != Nuki::CmdResult::Success)
    {
        return;
    }
    delay(100);

    uint16_t count = _nukiOpener.getLogEntryCount();

    result = _nukiOpener.retrieveLogEntries(0, count < 5 ? count : 5, 1, false);
    if(result != Nuki::CmdResult::Success)
    {
        return;
    }
    delay(1000);

    std::list<NukiOpener::LogEntry> log;
    _nukiOpener.getLogEntries(&log);

    if(log.size() > 0)
    {
        _network->publishAuthorizationInfo(log);
    }
    postponeBleWatchdog();
}

void NukiOpenerWrapper::updateKeypad()
{
    Log->print(F("Querying opener keypad: "));
    Nuki::CmdResult result = _nukiOpener.retrieveKeypadEntries(0, 0xffff);
    printCommandResult(result);
    if(result == Nuki::CmdResult::Success)
    {
        std::list<NukiLock::KeypadEntry> entries;
        _nukiOpener.getKeypadEntries(&entries);

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

void NukiOpenerWrapper::postponeBleWatchdog()
{
    _disableBleWatchdogTs = millis() + 15000;
}

NukiOpener::LockAction NukiOpenerWrapper::lockActionToEnum(const char *str)
{
    if(strcmp(str, "activateRTO") == 0) return NukiOpener::LockAction::ActivateRTO;
    else if(strcmp(str, "deactivateRTO") == 0) return NukiOpener::LockAction::DeactivateRTO;
    else if(strcmp(str, "electricStrikeActuation") == 0) return NukiOpener::LockAction::ElectricStrikeActuation;
    else if(strcmp(str, "activateCM") == 0) return NukiOpener::LockAction::ActivateCM;
    else if(strcmp(str, "deactivateCM") == 0) return NukiOpener::LockAction::DeactivateCM;
    else if(strcmp(str, "fobAction2") == 0) return NukiOpener::LockAction::FobAction2;
    else if(strcmp(str, "fobAction1") == 0) return NukiOpener::LockAction::FobAction1;
    else if(strcmp(str, "fobAction3") == 0) return NukiOpener::LockAction::FobAction3;
    return (NukiOpener::LockAction)0xff;
}

bool NukiOpenerWrapper::onLockActionReceivedCallback(const char *value)
{
    NukiOpener::LockAction action = nukiOpenerInst->lockActionToEnum(value);
    nukiOpenerInst->_nextLockAction = action;
    return (int)action != 0xff;
}

void NukiOpenerWrapper::onConfigUpdateReceivedCallback(const char *topic, const char *value)
{
    nukiOpenerInst->onConfigUpdateReceived(topic, value);
}

void NukiOpenerWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    nukiOpenerInst->onKeypadCommandReceived(command, id, name, code, enabled);
}

void NukiOpenerWrapper::onConfigUpdateReceived(const char *topic, const char *value)
{
    if(strcmp(topic, mqtt_topic_config_button_enabled) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiConfigValid || _nukiConfig.buttonEnabled == newValue) return;
        _nukiOpener.enableButton(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    if(strcmp(topic, mqtt_topic_config_led_enabled) == 0)
    {
        bool newValue = atoi(value) > 0;
        if(!_nukiConfigValid || _nukiConfig.ledFlashEnabled == newValue) return;
        _nukiOpener.enableLedFlash(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
    if(strcmp(topic, mqtt_topic_config_sound_level) == 0)
    {
        uint8_t newValue = atoi(value);
        if(!_nukiAdvancedConfigValid || _nukiAdvancedConfig.soundLevel == newValue) return;
        _nukiOpener.setSoundLevel(newValue);
        _nextConfigUpdateTs = millis() + 300;
    }
}

void NukiOpenerWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
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
        result = _nukiOpener.addKeypadEntry(entry);
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
        result = _nukiOpener.deleteKeypadEntry(id);
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
        result = _nukiOpener.updateKeypadEntry(entry);
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
        NukiOpener::cmdResultToString(result, resultStr);
        _network->publishKeypadCommandResult(resultStr);
    }
}

const NukiOpener::OpenerState &NukiOpenerWrapper::keyTurnerState()
{
    return _keyTurnerState;
}

const bool NukiOpenerWrapper::isPaired()
{
    return _paired;
}

const bool NukiOpenerWrapper::hasKeypad()
{
    return _hasKeypad;
}

const BLEAddress NukiOpenerWrapper::getBleAddress() const
{
    return _nukiOpener.getBleAddress();
}

BleScanner::Scanner *NukiOpenerWrapper::bleScanner()
{
    return _bleScanner;
}

void NukiOpenerWrapper::notify(Nuki::EventType eventType)
{
    if(eventType == Nuki::EventType::KeyTurnerStatusUpdated)
    {
        _statusUpdated = true;
    }
}

void NukiOpenerWrapper::readConfig()
{
    Log->print(F("Reading opener config. Result: "));
    Nuki::CmdResult result = _nukiOpener.requestConfig(&_nukiConfig);
    _nukiConfigValid = result == Nuki::CmdResult::Success;
    char resultStr[20];
    NukiOpener::cmdResultToString(result, resultStr);
    Log->println(resultStr);
    postponeBleWatchdog();
}

void NukiOpenerWrapper::readAdvancedConfig()
{
    Log->print(F("Reading opener advanced config. Result: "));
    Nuki::CmdResult result = _nukiOpener.requestAdvancedConfig(&_nukiAdvancedConfig);
    _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;
    char resultStr[20];
    NukiOpener::cmdResultToString(result, resultStr);
    Log->println(resultStr);
    postponeBleWatchdog();
}

void NukiOpenerWrapper::setupHASS()
{
    if(!_nukiConfigValid) return;

    String baseTopic = _preferences->getString(preference_mqtt_opener_path);
    char uidString[20];
    itoa(_nukiConfig.nukiId, uidString, 16);
    _network->publishHASSConfig("Opener",baseTopic.c_str(),(char*)_nukiConfig.name,uidString,"deactivateRTO","activateRTO","electricStrikeActuation","locked","unlocked");
    _hassSetupCompleted = true;

    Log->println("HASS setup for opener completed.");
}

void NukiOpenerWrapper::disableHASS()
{
    if(!_nukiConfigValid) // only ask for config once to save battery life
    {
        Nuki::CmdResult result = _nukiOpener.requestConfig(&_nukiConfig);
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

void NukiOpenerWrapper::printCommandResult(Nuki::CmdResult result)
{
    char resultStr[15];
    NukiOpener::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}
