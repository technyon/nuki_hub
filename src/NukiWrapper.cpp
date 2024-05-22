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
    network->setTimeControlCommandReceivedCallback(nukiInst->onTimeControlCommandReceivedCallback);

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
        uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        _preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
        uint32_t basicLockConfigAclPrefs[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        _preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
        uint32_t basicOpenerConfigAclPrefs[14] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        _preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[22] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        _preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        _preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
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
    if(!_paired)
    {
        Log->println(F("Nuki lock start pairing"));
        _network->publishBleAddress("");

        Nuki::AuthorizationIdType idType = _preferences->getBool(preference_register_as_app) ?
                                           Nuki::AuthorizationIdType::App :
                                           Nuki::AuthorizationIdType::Bridge;

        if(_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success)
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
        _network->publishStatusUpdated(_statusUpdated);
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
    if(_nextTimeControlUpdateTs != 0 && ts > _nextTimeControlUpdateTs)
    {
        _nextTimeControlUpdateTs = 0;
        updateTimeControl(true);
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

            if(_intervalLockstate > 10)
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

bool NukiWrapper::isPinValid()
{
    return _preferences->getInt(preference_lock_pin_status, 4) == 1;
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
        Log->println(F("Publishing auth data"));
        updateAuthData();
        Log->println(F("Done publishing auth data"));
    }

    _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);
    updateGpioOutputs();

    char lockStateStr[20];
    lockstateToString(_keyTurnerState.lockState, lockStateStr);
    Log->println(lockStateStr);

    postponeBleWatchdog();
    Log->println(F("Done querying lock state"));
}

void NukiWrapper::updateBatteryState()
{
    Log->print(F("Querying lock battery state: "));
    Nuki::CmdResult result = _nukiLock.requestBatteryReport(&_batteryReport);
    printCommandResult(result);
    if(result == Nuki::CmdResult::Success)
    {
        _network->publishBatteryReport(_batteryReport);
    }
    postponeBleWatchdog();
    Log->println(F("Done querying lock battery state"));
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

            if(_preferences->getBool(preference_timecontrol_info_enabled)) updateTimeControl(false);

            const int pinStatus = _preferences->getInt(preference_lock_pin_status, 4);

            if(isPinSet()) {
                Nuki::CmdResult result = _nukiLock.verifySecurityPin();

                if(result != Nuki::CmdResult::Success)
                {
                    if(pinStatus != 2) {
                        _preferences->putInt(preference_lock_pin_status, 2);
                    }
                }
                else
                {
                    if(pinStatus != 1) {
                        _preferences->putInt(preference_lock_pin_status, 1);
                    }
                }
            }
            else
            {
                if(pinStatus != 0) {
                    _preferences->putInt(preference_lock_pin_status, 0);
                }
            }
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
    if(!isPinValid())
    {
        Log->println(F("No valid PIN set"));
        return;
    }

    Nuki::CmdResult result = _nukiLock.retrieveLogEntries(0, 5, 1, false);
    Log->print(F("Retrieve log entries: "));
    Log->println(result);
    if(result != Nuki::CmdResult::Success)
    {
        return;
    }

    delay(100);

    std::list<NukiLock::LogEntry> log;
    _nukiLock.getLogEntries(&log);

    Log->print(F("Log size: "));
    Log->println(log.size());

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

        Log->print(F("Lock keypad codes: "));
        Log->println(entries.size());

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
        _keypadCodes.clear();
        _keypadCodes.reserve(entries.size());
        for(const auto& entry : entries)
        {
            _keypadCodeIds.push_back(entry.codeId);
            _keypadCodes.push_back(entry.code);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::updateTimeControl(bool retrieved)
{
    if(!_preferences->getBool(preference_timecontrol_info_enabled)) return;

    if(!retrieved)
    {
        Log->print(F("Querying lock time control: "));
        Nuki::CmdResult result = _nukiLock.retrieveTimeControlEntries();
        printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _nextTimeControlUpdateTs = millis() + 5000;
        }
    }
    else
    {
        std::list<NukiLock::TimeControlEntry> timeControlEntries;
        _nukiLock.getTimeControlEntries(&timeControlEntries);

        Log->print(F("Lock time control entries: "));
        Log->println(timeControlEntries.size());

        timeControlEntries.sort([](const NukiLock::TimeControlEntry& a, const NukiLock::TimeControlEntry& b) { return a.entryId < b.entryId; });

        _network->publishTimeControl(timeControlEntries);

        _timeControlIds.clear();
        _timeControlIds.reserve(timeControlEntries.size());
        for(const auto& entry : timeControlEntries)
        {
            _timeControlIds.push_back(entry.entryId);
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
    if(strcmp(str, "unlock") == 0 || strcmp(str, "Unlock") == 0) return NukiLock::LockAction::Unlock;
    else if(strcmp(str, "lock") == 0 || strcmp(str, "Lock") == 0) return NukiLock::LockAction::Lock;
    else if(strcmp(str, "unlatch") == 0 || strcmp(str, "Unlatch") == 0) return NukiLock::LockAction::Unlatch;
    else if(strcmp(str, "lockNgo") == 0 || strcmp(str, "LockNgo") == 0) return NukiLock::LockAction::LockNgo;
    else if(strcmp(str, "lockNgoUnlatch") == 0 || strcmp(str, "LockNgoUnlatch") == 0) return NukiLock::LockAction::LockNgoUnlatch;
    else if(strcmp(str, "fullLock") == 0 || strcmp(str, "FullLock") == 0) return NukiLock::LockAction::FullLock;
    else if(strcmp(str, "fobAction2") == 0 || strcmp(str, "FobAction2") == 0) return NukiLock::LockAction::FobAction2;
    else if(strcmp(str, "fobAction1") == 0 || strcmp(str, "FobAction1") == 0) return NukiLock::LockAction::FobAction1;
    else if(strcmp(str, "fobAction3") == 0 || strcmp(str, "FobAction3") == 0) return NukiLock::LockAction::FobAction3;
    return (NukiLock::LockAction)0xff;
}

LockActionResult NukiWrapper::onLockActionReceivedCallback(const char *value)
{
    NukiLock::LockAction action;

    if(value)
    {
        if(strlen(value) > 0)
        {
            action = nukiInst->lockActionToEnum(value);

            if((int)action == 0xff)
            {
                return LockActionResult::UnknownAction;
            }
        }
        else return LockActionResult::UnknownAction;
    }
    else return LockActionResult::UnknownAction;

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

void NukiWrapper::onConfigUpdateReceivedCallback(const char *value)
{
    nukiInst->onConfigUpdateReceived(value);
}

Nuki::AdvertisingMode NukiWrapper::advertisingModeToEnum(const char *str)
{
    if(strcmp(str, "Automatic") == 0) return Nuki::AdvertisingMode::Automatic;
    else if(strcmp(str, "Normal") == 0) return Nuki::AdvertisingMode::Normal;
    else if(strcmp(str, "Slow") == 0) return Nuki::AdvertisingMode::Slow;
    else if(strcmp(str, "Slowest") == 0) return Nuki::AdvertisingMode::Slowest;
    return (Nuki::AdvertisingMode)0xff;
}

Nuki::TimeZoneId NukiWrapper::timeZoneToEnum(const char *str)
{
    if(strcmp(str, "Africa/Cairo") == 0) return Nuki::TimeZoneId::Africa_Cairo;
    else if(strcmp(str, "Africa/Lagos") == 0) return Nuki::TimeZoneId::Africa_Lagos;
    else if(strcmp(str, "Africa/Maputo") == 0) return Nuki::TimeZoneId::Africa_Maputo;
    else if(strcmp(str, "Africa/Nairobi") == 0) return Nuki::TimeZoneId::Africa_Nairobi;
    else if(strcmp(str, "America/Anchorage") == 0) return Nuki::TimeZoneId::America_Anchorage;
    else if(strcmp(str, "America/Argentina/Buenos_Aires") == 0) return Nuki::TimeZoneId::America_Argentina_Buenos_Aires;
    else if(strcmp(str, "America/Chicago") == 0) return Nuki::TimeZoneId::America_Chicago;
    else if(strcmp(str, "America/Denver") == 0) return Nuki::TimeZoneId::America_Denver;
    else if(strcmp(str, "America/Halifax") == 0) return Nuki::TimeZoneId::America_Halifax;
    else if(strcmp(str, "America/Los_Angeles") == 0) return Nuki::TimeZoneId::America_Los_Angeles;
    else if(strcmp(str, "America/Manaus") == 0) return Nuki::TimeZoneId::America_Manaus;
    else if(strcmp(str, "America/Mexico_City") == 0) return Nuki::TimeZoneId::America_Mexico_City;
    else if(strcmp(str, "America/New_York") == 0) return Nuki::TimeZoneId::America_New_York;
    else if(strcmp(str, "America/Phoenix") == 0) return Nuki::TimeZoneId::America_Phoenix;
    else if(strcmp(str, "America/Regina") == 0) return Nuki::TimeZoneId::America_Regina;
    else if(strcmp(str, "America/Santiago") == 0) return Nuki::TimeZoneId::America_Santiago;
    else if(strcmp(str, "America/Sao_Paulo") == 0) return Nuki::TimeZoneId::America_Sao_Paulo;
    else if(strcmp(str, "America/St_Johns") == 0) return Nuki::TimeZoneId::America_St_Johns;
    else if(strcmp(str, "Asia/Bangkok") == 0) return Nuki::TimeZoneId::Asia_Bangkok;
    else if(strcmp(str, "Asia/Dubai") == 0) return Nuki::TimeZoneId::Asia_Dubai;
    else if(strcmp(str, "Asia/Hong_Kong") == 0) return Nuki::TimeZoneId::Asia_Hong_Kong;
    else if(strcmp(str, "Asia/Jerusalem") == 0) return Nuki::TimeZoneId::Asia_Jerusalem;
    else if(strcmp(str, "Asia/Karachi") == 0) return Nuki::TimeZoneId::Asia_Karachi;
    else if(strcmp(str, "Asia/Kathmandu") == 0) return Nuki::TimeZoneId::Asia_Kathmandu;
    else if(strcmp(str, "Asia/Kolkata") == 0) return Nuki::TimeZoneId::Asia_Kolkata;
    else if(strcmp(str, "Asia/Riyadh") == 0) return Nuki::TimeZoneId::Asia_Riyadh;
    else if(strcmp(str, "Asia/Seoul") == 0) return Nuki::TimeZoneId::Asia_Seoul;
    else if(strcmp(str, "Asia/Shanghai") == 0) return Nuki::TimeZoneId::Asia_Shanghai;
    else if(strcmp(str, "Asia/Tehran") == 0) return Nuki::TimeZoneId::Asia_Tehran;
    else if(strcmp(str, "Asia/Tokyo") == 0) return Nuki::TimeZoneId::Asia_Tokyo;
    else if(strcmp(str, "Asia/Yangon") == 0) return Nuki::TimeZoneId::Asia_Yangon;
    else if(strcmp(str, "Australia/Adelaide") == 0) return Nuki::TimeZoneId::Australia_Adelaide;
    else if(strcmp(str, "Australia/Brisbane") == 0) return Nuki::TimeZoneId::Australia_Brisbane;
    else if(strcmp(str, "Australia/Darwin") == 0) return Nuki::TimeZoneId::Australia_Darwin;
    else if(strcmp(str, "Australia/Hobart") == 0) return Nuki::TimeZoneId::Australia_Hobart;
    else if(strcmp(str, "Australia/Perth") == 0) return Nuki::TimeZoneId::Australia_Perth;
    else if(strcmp(str, "Australia/Sydney") == 0) return Nuki::TimeZoneId::Australia_Sydney;
    else if(strcmp(str, "Europe/Berlin") == 0) return Nuki::TimeZoneId::Europe_Berlin;
    else if(strcmp(str, "Europe/Helsinki") == 0) return Nuki::TimeZoneId::Europe_Helsinki;
    else if(strcmp(str, "Europe/Istanbul") == 0) return Nuki::TimeZoneId::Europe_Istanbul;
    else if(strcmp(str, "Europe/London") == 0) return Nuki::TimeZoneId::Europe_London;
    else if(strcmp(str, "Europe/Moscow") == 0) return Nuki::TimeZoneId::Europe_Moscow;
    else if(strcmp(str, "Pacific/Auckland") == 0) return Nuki::TimeZoneId::Pacific_Auckland;
    else if(strcmp(str, "Pacific/Guam") == 0) return Nuki::TimeZoneId::Pacific_Guam;
    else if(strcmp(str, "Pacific/Honolulu") == 0) return Nuki::TimeZoneId::Pacific_Honolulu;
    else if(strcmp(str, "Pacific/Pago_Pago") == 0) return Nuki::TimeZoneId::Pacific_Pago_Pago;
    else if(strcmp(str, "None") == 0) return Nuki::TimeZoneId::None;
    return (Nuki::TimeZoneId)0xff;
}

uint8_t NukiWrapper::fobActionToInt(const char *str)
{
    if(strcmp(str, "No Action") == 0) return 0;
    else if(strcmp(str, "Unlock") == 0) return 1;
    else if(strcmp(str, "Lock") == 0) return 2;
    else if(strcmp(str, "Lock n Go") == 0) return 3;
    else if(strcmp(str, "Intelligent") == 0) return 4;
    return 99;
}

NukiLock::ButtonPressAction NukiWrapper::buttonPressActionToEnum(const char* str)
{
    if(strcmp(str, "No Action") == 0) return NukiLock::ButtonPressAction::NoAction;
    else if(strcmp(str, "Intelligent") == 0) return NukiLock::ButtonPressAction::Intelligent;
    else if(strcmp(str, "Unlock") == 0) return NukiLock::ButtonPressAction::Unlock;
    else if(strcmp(str, "Lock") == 0) return NukiLock::ButtonPressAction::Lock;
    else if(strcmp(str, "Unlatch") == 0) return NukiLock::ButtonPressAction::Unlatch;
    else if(strcmp(str, "Lock n Go") == 0) return NukiLock::ButtonPressAction::LockNgo;
    else if(strcmp(str, "Show Status") == 0) return NukiLock::ButtonPressAction::ShowStatus;
    return (NukiLock::ButtonPressAction)0xff;
}

Nuki::BatteryType NukiWrapper::batteryTypeToEnum(const char* str)
{
    if(strcmp(str, "Alkali") == 0) return Nuki::BatteryType::Alkali;
    else if(strcmp(str, "Accumulators") == 0) return Nuki::BatteryType::Accumulators;
    else if(strcmp(str, "Lithium") == 0) return Nuki::BatteryType::Lithium;
    return (Nuki::BatteryType)0xff;
}

void NukiWrapper::onConfigUpdateReceived(const char *value)
{
    JsonDocument jsonResult;
    char _resbuf[2048];

    if(!_configRead || !_nukiConfigValid)
    {
        jsonResult["general"] = "configNotReady";
        serializeJson(jsonResult, _resbuf, sizeof(_resbuf));
        _network->publishConfigCommandResult(_resbuf);
        return;
    }

    if(!isPinValid())
    {
        jsonResult["general"] = "noValidPinSet";
        serializeJson(jsonResult, _resbuf, sizeof(_resbuf));
        _network->publishConfigCommandResult(_resbuf);
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        jsonResult["general"] = "invalidJson";
        serializeJson(jsonResult, _resbuf, sizeof(_resbuf));
        _network->publishConfigCommandResult(_resbuf);
        return;
    }

    Nuki::CmdResult cmdResult;
    const char *basicKeys[] = {"name", "latitude", "longitude", "autoUnlatch", "pairingEnabled", "buttonEnabled", "ledEnabled", "ledBrightness", "timeZoneOffset", "dstMode", "fobAction1",  "fobAction2", "fobAction3", "singleLock", "advertisingMode", "timeZone"};
    const char *advancedKeys[] = {"unlockedPositionOffsetDegrees", "lockedPositionOffsetDegrees", "singleLockedPositionOffsetDegrees", "unlockedToLockedTransitionOffsetDegrees", "lockNgoTimeout", "singleButtonPressAction", "doubleButtonPressAction", "detachedCylinder", "batteryType", "automaticBatteryTypeDetection", "unlatchDuration", "autoLockTimeOut",  "autoUnLockDisabled", "nightModeEnabled", "nightModeStartTime", "nightModeEndTime", "nightModeAutoLockEnabled", "nightModeAutoUnlockDisabled", "nightModeImmediateLockOnStart", "autoLockEnabled", "immediateAutoLockEnabled", "autoUpdateEnabled"};
    bool basicUpdated = false;
    bool advancedUpdated = false;
    uint32_t basicLockConfigAclPrefs[16];
    uint32_t advancedLockConfigAclPrefs[22];

    nukiLockPreferences = new Preferences();
    nukiLockPreferences->begin("nukihub", true);
    nukiLockPreferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
    nukiLockPreferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));

    for(int i=0; i < 16; i++)
    {
        if(json[basicKeys[i]])
        {
            const char *jsonchar = json[basicKeys[i]].as<const char*>();

            if(strlen(jsonchar) == 0)
            {
                jsonResult[basicKeys[i]] = "noValueSet";
                continue;
            }

            if((int)basicLockConfigAclPrefs[i] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;

                if(strcmp(basicKeys[i], "name") == 0)
                {
                    if(strlen(jsonchar) <= 32)
                    {
                        if(strcmp((const char*)_nukiConfig.name, jsonchar) == 0) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setName(std::string(jsonchar));
                    }
                    else jsonResult[basicKeys[i]] = "valueTooLong";
                }
                else if(strcmp(basicKeys[i], "latitude") == 0)
                {
                    const float keyvalue = atof(jsonchar);

                    if(keyvalue > 0)
                    {
                        if(_nukiConfig.latitude == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLatitude(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "longitude") == 0)
                {
                    const float keyvalue = atof(jsonchar);

                    if(keyvalue > 0)
                    {
                        if(_nukiConfig.longitude == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLongitude(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "autoUnlatch") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.autoUnlatch == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoUnlatch((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "pairingEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.pairingEnabled == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enablePairing((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "buttonEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.buttonEnabled == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableButton((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "ledEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.ledEnabled == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableLedFlash((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "ledBrightness") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= 0 && keyvalue <= 5)
                    {
                        if(_nukiConfig.ledBrightness == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLedBrightness(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "timeZoneOffset") == 0)
                {
                    const int16_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= 0 && keyvalue <= 60)
                    {
                        if(_nukiConfig.timeZoneOffset == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setTimeZoneOffset(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "dstMode") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.dstMode == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableDst((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "fobAction1") == 0)
                {
                    const uint8_t fobAct1 = nukiInst->fobActionToInt(jsonchar);

                    if(fobAct1 != 99)
                    {
                        if(_nukiConfig.fobAction1 == fobAct1) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setFobAction(1, fobAct1);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "fobAction2") == 0)
                {
                    const uint8_t fobAct2 = nukiInst->fobActionToInt(jsonchar);

                    if(fobAct2 != 99)
                    {
                        if(_nukiConfig.fobAction2 == fobAct2) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setFobAction(2, fobAct2);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "fobAction3") == 0)
                {
                    const uint8_t fobAct3 = nukiInst->fobActionToInt(jsonchar);

                    if(fobAct3 != 99)
                    {
                        if(_nukiConfig.fobAction3 == fobAct3) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setFobAction(3, fobAct3);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "singleLock") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.singleLock == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableSingleLock((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "advertisingMode") == 0)
                {
                    Nuki::AdvertisingMode advmode = nukiInst->advertisingModeToEnum(jsonchar);

                    if((int)advmode != 0xff)
                    {
                        if(_nukiConfig.advertisingMode == advmode) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setAdvertisingMode(advmode);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }
                else if(strcmp(basicKeys[i], "timeZone") == 0)
                {
                    Nuki::TimeZoneId tzid = nukiInst->timeZoneToEnum(jsonchar);

                    if((int)tzid != 0xff)
                    {
                        if(_nukiConfig.timeZoneId == tzid) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setTimeZoneId(tzid);
                    }
                    else jsonResult[basicKeys[i]] = "invalidValue";
                }

                if(cmdResult == Nuki::CmdResult::Success) basicUpdated = true;

                if(!jsonResult[basicKeys[i]]) {
                    char resultStr[15] = {0};
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[basicKeys[i]] = resultStr;
                }
            }
            else jsonResult[basicKeys[i]] = "accessDenied";
        }
    }

    for(int i=0; i < 22; i++)
    {
        if(json[advancedKeys[i]])
        {
            const char *jsonchar = json[advancedKeys[i]].as<const char*>();

            if(strlen(jsonchar) == 0)
            {
                jsonResult[advancedKeys[i]] = "noValueSet";
                continue;
            }

            if((int)advancedLockConfigAclPrefs[i] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;

                if(strcmp(advancedKeys[i], "unlockedPositionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= -90 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.unlockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setUnlockedPositionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "lockedPositionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= -180 && keyvalue <= 90)
                    {
                        if(_nukiAdvancedConfig.lockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLockedPositionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "singleLockedPositionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= -180 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.singleLockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setSingleLockedPositionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "unlockedToLockedTransitionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= -180 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.unlockedToLockedTransitionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setUnlockedToLockedTransitionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "lockNgoTimeout") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= 5 && keyvalue <= 60)
                    {
                        if(_nukiAdvancedConfig.lockNgoTimeout == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLockNgoTimeout(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "singleButtonPressAction") == 0)
                {
                    NukiLock::ButtonPressAction sbpa = nukiInst->buttonPressActionToEnum(jsonchar);

                    if((int)sbpa != 0xff)
                    {
                        if(_nukiAdvancedConfig.singleButtonPressAction == sbpa) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setSingleButtonPressAction(sbpa);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "doubleButtonPressAction") == 0)
                {
                    NukiLock::ButtonPressAction dbpa = nukiInst->buttonPressActionToEnum(jsonchar);

                    if((int)dbpa != 0xff)
                    {
                        if(_nukiAdvancedConfig.doubleButtonPressAction == dbpa) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setDoubleButtonPressAction(dbpa);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "detachedCylinder") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.detachedCylinder == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableDetachedCylinder((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "batteryType") == 0)
                {
                    Nuki::BatteryType battype = nukiInst->batteryTypeToEnum(jsonchar);

                    if((int)battype != 0xff)
                    {
                        if(_nukiAdvancedConfig.batteryType == battype) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setBatteryType(battype);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "automaticBatteryTypeDetection") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoBatteryTypeDetection((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "unlatchDuration") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= 1 && keyvalue <= 30)
                    {
                        if(_nukiAdvancedConfig.unlatchDuration == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setUnlatchDuration(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "autoLockTimeOut") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue >= 30 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.autoLockTimeOut == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setAutoLockTimeOut(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "autoUnLockDisabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.autoUnLockDisabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.disableAutoUnlock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "nightModeEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableNightMode((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "nightModeStartTime") == 0)
                {
                    String keystr = jsonchar;
                    unsigned char keyvalue[2];
                    keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                    keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                    if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                    {
                        if(_nukiAdvancedConfig.nightModeStartTime == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setNightModeStartTime(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "nightModeEndTime") == 0)
                {
                    String keystr = jsonchar;
                    unsigned char keyvalue[2];
                    keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                    keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                    if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                    {
                        if(_nukiAdvancedConfig.nightModeEndTime == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setNightModeEndTime(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "nightModeAutoLockEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeAutoLockEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableNightModeAutoLock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "nightModeAutoUnlockDisabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeAutoUnlockDisabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.disableNightModeAutoUnlock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "nightModeImmediateLockOnStart") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeImmediateLockOnStart == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableNightModeImmediateLockOnStart((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "autoLockEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.autoLockEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoLock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "immediateAutoLockEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.immediateAutoLockEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableImmediateAutoLock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }
                else if(strcmp(advancedKeys[i], "autoUpdateEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(jsonchar);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.autoUpdateEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoUpdate((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidValue";
                }

                if(cmdResult == Nuki::CmdResult::Success) advancedUpdated = true;

                if(!jsonResult[advancedKeys[i]]) {
                    char resultStr[15] = {0};
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[advancedKeys[i]] = resultStr;
                }
            }
            else jsonResult[advancedKeys[i]] = "accessDenied";
        }
    }

    nukiLockPreferences->end();

    if(basicUpdated || advancedUpdated)
    {
        jsonResult["general"] = "success";
    }
    else jsonResult["general"] = "noChange";

    _nextConfigUpdateTs = millis() + 300;

    serializeJson(jsonResult, _resbuf, sizeof(_resbuf));
    _network->publishConfigCommandResult(_resbuf);

    return;
}

void NukiWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    nukiInst->onKeypadCommandReceived(command, id, name, code, enabled);
}

void NukiWrapper::onKeypadJsonCommandReceivedCallback(const char *value)
{
    nukiInst->onKeypadJsonCommandReceived(value);
}

void NukiWrapper::onTimeControlCommandReceivedCallback(const char *value)
{
    nukiInst->onTimeControlCommandReceived(value);
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
    if(!isPinValid())
    {
        _network->publishKeypadJsonCommandResult("noValidPinSet");
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

        _network->publishKeypadJsonCommandResult("configNotReady");
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
                Log->print(F("Delete keypad code: "));
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
            else if (strcmp(action, "update") != 0)
            {
                _network->publishKeypadJsonCommandResult("noCodeSet");
                return;
            }

            unsigned int allowedFromAr[6];
            unsigned int allowedUntilAr[6];
            unsigned int allowedFromTimeAr[2];
            unsigned int allowedUntilTimeAr[2];
            uint8_t allowedWeekdaysInt = 0;

            if(timeLimited == 1)
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
                Log->print(F("Add keypad code: "));
                Log->println((int)result);
            }
            else if (strcmp(action, "update") == 0)
            {
                if(!codeId)
                {
                    _network->publishKeypadJsonCommandResult("noCodeIdSet");
                    return;
                }

                if(!idExists)
                {
                    _network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                    return;
                }

                NukiLock::UpdatedKeypadEntry entry;
                memset(&entry, 0, sizeof(entry));
                entry.codeId = codeId;
                size_t nameLen = strlen(name);
                memcpy(&entry.name, name, nameLen > 20 ? 20 : nameLen);
                
                if(code) entry.code = code;
                else
                {
                    auto it = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), codeId);
                    entry.code = _keypadCodes[(it - _keypadCodeIds.begin())];
                }

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
                Log->print(F("Update keypad code: "));
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

void NukiWrapper::onTimeControlCommandReceived(const char *value)
{
    if(!_configRead || !_nukiConfigValid)
    {
        _network->publishTimeControlCommandResult("configNotReady");
        return;
    }

    if(!isPinValid())
    {
        _network->publishTimeControlCommandResult("noValidPinSet");
        return;
    }

    if(!_preferences->getBool(preference_timecontrol_control_enabled))
    {
        _network->publishTimeControlCommandResult("timeControlControlDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        _network->publishTimeControlCommandResult("invalidJson");
        return;
    }

    Nuki::CmdResult result = (Nuki::CmdResult)-1;

    const char *action = json["action"].as<const char*>();
    uint8_t entryId = json["entryId"].as<unsigned int>();
    uint8_t enabled = json["enabled"].as<unsigned int>();
    String weekdays = json["weekdays"].as<String>();
    const char *time = json["time"].as<const char*>();
    const char *lockAct = json["lockAction"].as<const char*>();
    NukiLock::LockAction timeControlLockAction;
  
    if(lockAct)
    {
        timeControlLockAction = nukiInst->lockActionToEnum(lockAct);

        if((int)timeControlLockAction == 0xff)
        {
            _network->publishTimeControlCommandResult("invalidLockAction");
            return;
        }
    }

    if(action)
    {
        bool idExists = false;

        if(entryId)
        {
            idExists = std::find(_timeControlIds.begin(), _timeControlIds.end(), entryId) != _timeControlIds.end();
        }

        if(strcmp(action, "delete") == 0) {
            if(idExists)
            {
                result = _nukiLock.removeTimeControlEntry(entryId);
                Log->print(F("Delete time control: "));
                Log->println((int)result);
            }
            else
            {
                _network->publishTimeControlCommandResult("noExistingEntryIdSet");
                return;
            }
        }
        else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
        {
            uint8_t timeHour;
            uint8_t timeMin;
            uint8_t weekdaysInt = 0;
            unsigned int timeAr[2];

            if(time)
            {
                if(strlen(time) == 5)
                {
                    String timeStr = time;
                    timeAr[0] = (uint8_t)timeStr.substring(0, 2).toInt();
                    timeAr[1] = (uint8_t)timeStr.substring(3, 5).toInt();

                    if(timeAr[0] < 0 || timeAr[0] > 23 || timeAr[1] < 0 || timeAr[1] > 59)
                    {
                        _network->publishTimeControlCommandResult("invalidTime");
                        return;
                    }
                }
                else
                {
                    _network->publishTimeControlCommandResult("invalidTime");
                    return;
                }
            }
            else
            {
                _network->publishTimeControlCommandResult("invalidTime");
                return;
            }

            if(weekdays.indexOf("mon") >= 0) weekdaysInt += 64;
            if(weekdays.indexOf("tue") >= 0) weekdaysInt += 32;
            if(weekdays.indexOf("wed") >= 0) weekdaysInt += 16;
            if(weekdays.indexOf("thu") >= 0) weekdaysInt += 8;
            if(weekdays.indexOf("fri") >= 0) weekdaysInt += 4;
            if(weekdays.indexOf("sat") >= 0) weekdaysInt += 2;
            if(weekdays.indexOf("sun") >= 0) weekdaysInt += 1;

            if(strcmp(action, "add") == 0)
            {
                NukiLock::NewTimeControlEntry entry;
                memset(&entry, 0, sizeof(entry));
                entry.weekdays = weekdaysInt;

                if(time)
                {
                    entry.timeHour = timeAr[0];
                    entry.timeMin = timeAr[1];
                }

                entry.lockAction = timeControlLockAction;

                result = _nukiLock.addTimeControlEntry(entry);
                Log->print(F("Add time control: "));
                Log->println((int)result);
            }
            else if (strcmp(action, "update") == 0)
            {
                if(!idExists)
                {
                    _network->publishTimeControlCommandResult("noExistingEntryIdSet");
                    return;
                }

                NukiLock::TimeControlEntry entry;
                memset(&entry, 0, sizeof(entry));
                entry.entryId = entryId;
                entry.enabled = enabled == 0 ? 0 : 1;
                entry.weekdays = weekdaysInt;

                if(time)
                {
                    entry.timeHour = timeAr[0];
                    entry.timeMin = timeAr[1];
                }

                entry.lockAction = timeControlLockAction;

                result = _nukiLock.updateTimeControlEntry(entry);
                Log->print(F("Update time control: "));
                Log->println((int)result);
            }
        }
        else
        {
            _network->publishTimeControlCommandResult("invalidAction");
            return;
        }

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            NukiLock::cmdResultToString(result, resultStr);
            _network->publishTimeControlCommandResult(resultStr);
        }

        _nextConfigUpdateTs = millis() + 300;
    }
    else
    {
        _network->publishTimeControlCommandResult("noActionSet");
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
        _network->publishStatusUpdated(_statusUpdated);
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

    _network->publishHASSConfig((char*)"SmartLock", baseTopic.c_str(),(char*)_nukiConfig.name, uidString, hasDoorSensor(), _hasKeypad, _publishAuthData, (char*)"lock", (char*)"unlock", (char*)"unlatch");
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

    if(_nukiConfigValid)
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

