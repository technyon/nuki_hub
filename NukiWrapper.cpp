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
            _hasKeypad = _nukiConfig.hasKeypad > 0 || _nukiConfig.hasKeypadV2;
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
    if(_preferences->getBool(preference_keypad_info_enabled)) return;

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
    DynamicJsonDocument jsonResult(2048);
    char _resbuf[2048];

    DynamicJsonDocument json(2048);
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        jsonResult["general"] = "invalidjson";
        serializeJson(jsonResult, _resbuf, sizeof(_resbuf));
        _network->publishConfigCommandResult(_resbuf);
        return;
    }

    updateConfig();

    if(!_nukiConfigValid || !_nukiAdvancedConfigValid)
    {
        jsonResult["general"] = "invalidconfig";
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
            if((int)basicLockConfigAclPrefs[i] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;

                if(strcmp(basicKeys[i], "name") == 0)
                {
                    const char* keyvalue = json[basicKeys[i]];

                    if(strlen(keyvalue) <= 32)
                    {
                        if(strcmp((const char*)_nukiConfig.name, keyvalue) == 0) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setName(std::string(keyvalue));
                    }
                    else jsonResult[basicKeys[i]] = "valuetoolong";
                }
                else if(strcmp(basicKeys[i], "latitude") == 0)
                {
                    const float keyvalue = atof(json[basicKeys[i]]);

                    if(keyvalue > 0)
                    {
                        if(_nukiConfig.latitude == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLatitude(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "longitude") == 0)
                {
                    const float keyvalue = atof(json[basicKeys[i]]);

                    if(keyvalue > 0)
                    {
                        if(_nukiConfig.longitude == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLongitude(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "autoUnlatch") == 0)
                {
                    const uint8_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.autoUnlatch == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoUnlatch((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "pairingEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.pairingEnabled == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enablePairing((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "buttonEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.buttonEnabled == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableButton((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "ledEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.ledEnabled == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableLedFlash((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "ledBrightness") == 0)
                {
                    const uint8_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue >= 0 && keyvalue <= 5)
                    {
                        if(_nukiConfig.ledBrightness == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLedBrightness(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "timeZoneOffset") == 0)
                {
                    const int16_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue >= 0 && keyvalue <= 60)
                    {
                        if(_nukiConfig.timeZoneOffset == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setTimeZoneOffset(keyvalue);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "dstMode") == 0)
                {
                    const uint8_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.dstMode == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableDst((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "fobAction1") == 0)
                {
                    const uint8_t fobAct1 = nukiInst->fobActionToInt(json[basicKeys[i]]);

                    if(fobAct1 != 99)
                    {
                        if(_nukiConfig.fobAction1 == fobAct1) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setFobAction(1, fobAct1);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "fobAction2") == 0)
                {
                    const uint8_t fobAct2 = nukiInst->fobActionToInt(json[basicKeys[i]]);

                    if(fobAct2 != 99)
                    {
                        if(_nukiConfig.fobAction2 == fobAct2) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setFobAction(2, fobAct2);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "fobAction3") == 0)
                {
                    const uint8_t fobAct3 = nukiInst->fobActionToInt(json[basicKeys[i]]);

                    if(fobAct3 != 99)
                    {
                        if(_nukiConfig.fobAction3 == fobAct3) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setFobAction(3, fobAct3);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "singleLock") == 0)
                {
                    const uint8_t keyvalue = atoi(json[basicKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiConfig.singleLock == keyvalue) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableSingleLock((keyvalue > 0));
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "advertisingMode") == 0)
                {
                    Nuki::AdvertisingMode advmode = nukiInst->advertisingModeToEnum(json[basicKeys[i]]);

                    if((int)advmode != 0xff)
                    {
                        if(_nukiConfig.advertisingMode == advmode) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setAdvertisingMode(advmode);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }
                else if(strcmp(basicKeys[i], "timeZone") == 0)
                {
                    Nuki::TimeZoneId tzid = nukiInst->timeZoneToEnum(json[basicKeys[i]]);

                    if((int)tzid != 0xff)
                    {
                        if(_nukiConfig.timeZoneId == tzid) jsonResult[basicKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setTimeZoneId(tzid);
                    }
                    else jsonResult[basicKeys[i]] = "invalidvalue";
                }

                if(cmdResult == Nuki::CmdResult::Success) basicUpdated = true;

                if(!jsonResult[basicKeys[i]]) {
                    char resultStr[15] = {0};
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[basicKeys[i]] = resultStr;
                }
            }
            else jsonResult[basicKeys[i]] = "accessdenied";
        }
    }

    for(int i=0; i < 22; i++)
    {
        if(json[advancedKeys[i]])
        {
            if((int)advancedLockConfigAclPrefs[i] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;

                if(strcmp(advancedKeys[i], "unlockedPositionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue >= -90 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.unlockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setUnlockedPositionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "lockedPositionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue >= -180 && keyvalue <= 90)
                    {
                        if(_nukiAdvancedConfig.lockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLockedPositionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "singleLockedPositionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue >= -180 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.singleLockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setSingleLockedPositionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "unlockedToLockedTransitionOffsetDegrees") == 0)
                {
                    const int16_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue >= -180 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.unlockedToLockedTransitionOffsetDegrees == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setUnlockedToLockedTransitionOffsetDegrees(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "lockNgoTimeout") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue >= 5 && keyvalue <= 60)
                    {
                        if(_nukiAdvancedConfig.lockNgoTimeout == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setLockNgoTimeout(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "singleButtonPressAction") == 0)
                {
                    NukiLock::ButtonPressAction sbpa = nukiInst->buttonPressActionToEnum(json[advancedKeys[i]]);

                    if((int)sbpa != 0xff)
                    {
                        if(_nukiAdvancedConfig.singleButtonPressAction == sbpa) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setSingleButtonPressAction(sbpa);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "doubleButtonPressAction") == 0)
                {
                    NukiLock::ButtonPressAction dbpa = nukiInst->buttonPressActionToEnum(json[advancedKeys[i]]);

                    if((int)dbpa != 0xff)
                    {
                        if(_nukiAdvancedConfig.doubleButtonPressAction == dbpa) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setDoubleButtonPressAction(dbpa);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "detachedCylinder") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.detachedCylinder == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableDetachedCylinder((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "batteryType") == 0)
                {
                    Nuki::BatteryType battype = nukiInst->batteryTypeToEnum(json[advancedKeys[i]]);

                    if((int)battype != 0xff)
                    {
                        if(_nukiAdvancedConfig.batteryType == battype) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setBatteryType(battype);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "automaticBatteryTypeDetection") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoBatteryTypeDetection((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "unlatchDuration") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue >= 1 && keyvalue <= 30)
                    {
                        if(_nukiAdvancedConfig.unlatchDuration == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setUnlatchDuration(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "autoLockTimeOut") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue >= 30 && keyvalue <= 180)
                    {
                        if(_nukiAdvancedConfig.autoLockTimeOut == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setAutoLockTimeOut(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "autoUnLockDisabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.autoUnLockDisabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.disableAutoUnlock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "nightModeEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableNightMode((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "nightModeStartTime") == 0)
                {
                    String keystr = json[advancedKeys[i]];
                    unsigned char keyvalue[2];
                    keyvalue[0] = (uint8_t)atoi(keystr.substring(0, 2).c_str());
                    keyvalue[1] = (uint8_t)atoi(keystr.substring(3, 5).c_str());
                    if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                    {
                        if(_nukiAdvancedConfig.nightModeStartTime == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setNightModeStartTime(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "nightModeEndTime") == 0)
                {
                    String keystr = json[advancedKeys[i]];
                    unsigned char keyvalue[2];
                    keyvalue[0] = (uint8_t)atoi(keystr.substring(0, 2).c_str());
                    keyvalue[1] = (uint8_t)atoi(keystr.substring(3, 5).c_str());
                    if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                    {
                        if(_nukiAdvancedConfig.nightModeEndTime == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.setNightModeEndTime(keyvalue);
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";                
                }
                else if(strcmp(advancedKeys[i], "nightModeAutoLockEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeAutoLockEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableNightModeAutoLock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "nightModeAutoUnlockDisabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeAutoUnlockDisabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.disableNightModeAutoUnlock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "nightModeImmediateLockOnStart") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.nightModeImmediateLockOnStart == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableNightModeImmediateLockOnStart((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "autoLockEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.autoLockEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoLock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "immediateAutoLockEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.immediateAutoLockEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableImmediateAutoLock((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }
                else if(strcmp(advancedKeys[i], "autoUpdateEnabled") == 0)
                {
                    const uint8_t keyvalue = atoi(json[advancedKeys[i]]);

                    if(keyvalue == 0 || keyvalue == 1)
                    {
                        if(_nukiAdvancedConfig.autoUpdateEnabled == keyvalue) jsonResult[advancedKeys[i]] = "unchanged";
                        else cmdResult = _nukiLock.enableAutoUpdate((keyvalue > 0));
                    }
                    else jsonResult[advancedKeys[i]] = "invalidvalue";
                }

                if(cmdResult == Nuki::CmdResult::Success) advancedUpdated = true;

                if(!jsonResult[advancedKeys[i]]) {
                    char resultStr[15] = {0};
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[advancedKeys[i]] = resultStr;
                }
            }
            else jsonResult[advancedKeys[i]] = "accessdenied";
        }
    }

    nukiLockPreferences->end();

    if(basicUpdated || advancedUpdated)
    {
        _nextConfigUpdateTs = millis() + 300;
        jsonResult["general"] = "success";
    }
    else jsonResult["general"] = "nochange";

    serializeJson(jsonResult, _resbuf, sizeof(_resbuf));
    _network->publishConfigCommandResult(_resbuf);

    return;
}

void NukiWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    nukiInst->onKeypadCommandReceived(command, id, name, code, enabled);
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

