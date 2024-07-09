#include "NukiWrapper.h"
#include <RTOS.h>
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "Logger.h"
#include "RestartReason.h"
#include <NukiLockUtils.h>
#include "Config.h"

NukiWrapper* nukiInst;
NukiNetworkLock* networkInst;
Preferences* nukiLockPreferences = nullptr;

NukiWrapper::NukiWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NukiNetworkLock* network, Gpio* gpio, Preferences* preferences)
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
    networkInst = _network;

    memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    _keyTurnerState.lockState = NukiLock::LockState::Undefined;

    network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
    network->setOfficialUpdateReceivedCallback(nukiInst->onOfficialUpdateReceivedCallback);
    network->setConfigUpdateReceivedCallback(nukiInst->onConfigUpdateReceivedCallback);
    if(_preferences->getBool(preference_disable_non_json, false)) network->setKeypadCommandReceivedCallback(nukiInst->onKeypadCommandReceivedCallback);
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
    _intervalHybridLockstate = _preferences->getInt(preference_query_interval_hybrid_lockstate);
    _intervalConfig = _preferences->getInt(preference_query_interval_configuration);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);
    _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
    _keypadEnabled = _preferences->getBool(preference_keypad_info_enabled);
    _publishAuthData = _preferences->getBool(preference_publish_authdata);
    _maxKeypadCodeCount = _preferences->getUInt(preference_lock_max_keypad_code_count);
    _maxTimeControlEntryCount = _preferences->getUInt(preference_lock_max_timecontrol_entry_count);
    _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
    _hassEnabled = _preferences->getString(preference_mqtt_hass_discovery) != "";
    _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries, 200);
    _retryDelay = _preferences->getInt(preference_command_retry_delay);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;

    if(firstStart)
    {
        Log->println("First start, setting preference defaults");
        _preferences->putBool(preference_network_wifi_fallback_disabled, false);
        _preferences->putBool(preference_find_best_rssi, false);
        _preferences->putBool(preference_check_updates, true);        
        _preferences->putBool(preference_opener_continuous_mode, false);
        _preferences->putBool(preference_network_wifi_fallback_disabled, false);
        _preferences->putBool(preference_official_hybrid, false);
        _preferences->putBool(preference_official_hybrid_actions, false);
        _preferences->putBool(preference_official_hybrid_retry, false);
        _preferences->putBool(preference_disable_non_json, false);
        _preferences->putBool(preference_update_from_mqtt, false);
        _preferences->putBool(preference_ip_dhcp_enabled, true);        
        _preferences->putBool(preference_enable_bootloop_reset, false);
        _preferences->putBool(preference_show_secrets, false);

        _preferences->putBool(preference_conf_info_enabled, true);
        _preferences->putBool(preference_keypad_info_enabled, false);
        _preferences->putBool(preference_keypad_topic_per_entry, false);
        _preferences->putBool(preference_keypad_publish_code, false);
        _preferences->putBool(preference_keypad_control_enabled, false);
        _preferences->putBool(preference_timecontrol_info_enabled, false);
        _preferences->putBool(preference_timecontrol_topic_per_entry, false);
        _preferences->putBool(preference_timecontrol_control_enabled, false);
        _preferences->putBool(preference_publish_authdata, false);
        _preferences->putBool(preference_register_as_app, false);
        _preferences->putBool(preference_register_opener_as_app, false);

        _preferences->putInt(preference_mqtt_broker_port, 1883);
        _preferences->putInt(preference_buffer_size, CHAR_BUFFER_SIZE);
        _preferences->putInt(preference_task_size_network, NETWORK_TASK_SIZE);
        _preferences->putInt(preference_task_size_nuki, NUKI_TASK_SIZE);
        _preferences->putInt(preference_authlog_max_entries, MAX_AUTHLOG);
        _preferences->putInt(preference_keypad_max_entries, MAX_KEYPAD);
        _preferences->putInt(preference_timecontrol_max_entries, MAX_TIMECONTROL);        
        _preferences->putInt(preference_query_interval_hybrid_lockstate, 600);
        _preferences->putInt(preference_rssi_publish_interval, 60);
        _preferences->putInt(preference_network_timeout, 60);
        _preferences->putInt(preference_command_nr_of_retries, 3);
        _preferences->putInt(preference_command_retry_delay, 1000);
        _preferences->putInt(preference_restart_ble_beacon_lost, 60);
        _preferences->putInt(preference_query_interval_lockstate, 1800);
        _preferences->putInt(preference_query_interval_configuration, 3600);
        _preferences->putInt(preference_query_interval_battery, 1800);
        _preferences->putInt(preference_query_interval_keypad, 1800);
#if PRESENCE_DETECTION_ENABLED
        _preferences->putInt(preference_presence_detection_timeout, -1);
#endif
    }

    if(_nrOfRetries < 0 || _nrOfRetries == 200)
    {
        Log->println("Invalid nrOfRetries, revert to default (3)");
        _nrOfRetries = 3;
        _preferences->putInt(preference_command_nr_of_retries, _nrOfRetries);
    }

    if(_retryDelay <= 100)
    {
        Log->println("Invalid retryDelay, revert to default (100)");
        _retryDelay = 100;
        _preferences->putInt(preference_command_retry_delay, _retryDelay);
    }

    if(_intervalLockstate == 0)
    {
        Log->println("Invalid intervalLockstate, revert to default (1800)");
        _intervalLockstate = 60 * 30;
        _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
    }
    if(_intervalHybridLockstate == 0)
    {
        Log->println("Invalid intervalHybridLockstate, revert to default (600)");
        _intervalHybridLockstate = 60 * 10;
        _preferences->putInt(preference_query_interval_hybrid_lockstate, _intervalHybridLockstate);
    }
    if(_intervalConfig == 0)
    {
        Log->println("Invalid intervalConfig, revert to default (3600)");
        _intervalConfig = 60 * 60;
        _preferences->putInt(preference_query_interval_configuration, _intervalConfig);
    }
    if(_intervalBattery == 0)
    {
        Log->println("Invalid intervalBattery, revert to default (1800)");
        _intervalBattery = 60 * 30;
        _preferences->putInt(preference_query_interval_battery, _intervalBattery);
    }
    if(_intervalKeypad == 0)
    {
        Log->println("Invalid intervalKeypad, revert to default (1800)");
        _intervalKeypad = 60 * 30;
        _preferences->putInt(preference_query_interval_keypad, _intervalKeypad);
    }
    if(_restartBeaconTimeout < 10)
    {
        Log->println("Invalid restartBeaconTimeout, revert to default (-1)");
        _restartBeaconTimeout = -1;
        _preferences->putInt(preference_restart_ble_beacon_lost, _restartBeaconTimeout);
    }
    
    _nukiLock.setEventHandler(this);
    _nukiLock.setDisonnectTimeout(5000);

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
        _preferences->getBool(preference_register_as_app) ? Log->println(F("Pairing as app")) : Log->println(F("Pairing as bridge"));
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

    int64_t lastReceivedBeaconTs = _nukiLock.getLastReceivedBeaconTs();
    int64_t ts = (esp_timer_get_time() / 1000);
    uint8_t queryCommands = _network->queryCommands();

    if(_restartBeaconTimeout > 0 &&
       ts > 60000 &&
       lastReceivedBeaconTs > 0 &&
       _disableBleWatchdogTs < ts &&
       (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
    {
        Log->print("No BLE beacon received from the lock for ");
        Log->print((ts - lastReceivedBeaconTs) / 1000);
        Log->println(" seconds, restarting device.");
        delay(200);
        restartEsp(RestartReason::BLEBeaconWatchdog);
    }

    _nukiLock.updateConnectionState();

    if(_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        Log->println("Updating Lock state based on timer or query");
        _statusUpdated = false;
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        updateKeyTurnerState();
        _network->publishStatusUpdated(_statusUpdated);
    }
    if(networkInst->_offCommandExecutedTs>0 && ts >= networkInst->_offCommandExecutedTs)
    {
        nukiInst->_nextLockAction = networkInst->_offCommand;
        networkInst->_offCommandExecutedTs = 0;
    }
    if(_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs || (queryCommands & QUERY_COMMAND_BATTERY) > 0)
    {
        Log->println("Updating Lock battery state based on timer or query");        
        _nextBatteryReportTs = ts + _intervalBattery * 1000;
        updateBatteryState();
    }
    if(_nextConfigUpdateTs == 0 || ts > _nextConfigUpdateTs || (queryCommands & QUERY_COMMAND_CONFIG) > 0)
    {
        Log->println("Updating Lock config based on timer or query");
        _nextConfigUpdateTs = ts + _intervalConfig * 1000;
        updateConfig();
        if(_hassEnabled && !_hassSetupCompleted)
        {
            setupHASS();
        }
    }
    if(_waitAuthLogUpdateTs != 0 && ts > _waitAuthLogUpdateTs)
    {
        _waitAuthLogUpdateTs = 0;
        updateAuthData(true);
    }
    if(_waitKeypadUpdateTs != 0 && ts > _waitKeypadUpdateTs)
    {
        _waitKeypadUpdateTs = 0;
        updateKeypad(true);
    }
    if(_waitTimeControlUpdateTs != 0 && ts > _waitTimeControlUpdateTs)
    {
        _waitTimeControlUpdateTs = 0;
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
        Log->println("Updating Lock keypad based on timer or query");
        _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
        updateKeypad(false);
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

                _nextRetryTs = (esp_timer_get_time() / 1000) + _retryDelay;

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
        Log->println("Clearing Lock auth data");
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
    Preferences nukiBlePref;
    nukiBlePref.begin("NukiHub", false);
    nukiBlePref.clear();
    nukiBlePref.end();
    _deviceId->assignNewId();
    _preferences->remove(preference_nuki_id_lock);
    _paired = false;
}

void NukiWrapper::updateKeyTurnerState()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    _retryCount = 0;
    
    Log->println("Querying lock state");

    while(_retryCount < _nrOfRetries + 1)
    {
        Log->print(F("Result (attempt "));
        Log->print(_retryCount + 1);
        Log->print("): ");
        result =_nukiLock.requestKeyTurnerState(&_keyTurnerState);
        delay(250);

        if(result != Nuki::CmdResult::Success) {
            ++_retryCount;
        }
        else break;
    }

    char resultStr[15];
    memset(&resultStr, 0, sizeof(resultStr));
    NukiLock::cmdResultToString(result, resultStr);
    _network->publishLockstateCommandResult(resultStr);

    if(result != Nuki::CmdResult::Success)
    {
        Log->println("Query lock state failed");
        _retryLockstateCount++;
        postponeBleWatchdog();
        if(_retryLockstateCount < _nrOfRetries + 1)
        {
            Log->print(F("Query lock state retrying in "));
            Log->print(_retryDelay);
            Log->println("ms");
            _nextLockStateUpdateTs = (esp_timer_get_time() / 1000) + _retryDelay;
        }
        return;
    }

    _retryLockstateCount = 0;

    if(_publishAuthData)
    {
        Log->println(F("Publishing auth data"));
        updateAuthData(false);
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
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    _retryCount = 0;
    
    Log->println("Querying lock battery state");

    while(_retryCount < _nrOfRetries + 1)
    {
        Log->print(F("Result (attempt "));
        Log->print(_retryCount + 1);
        Log->print("): ");
        result = _nukiLock.requestBatteryReport(&_batteryReport);
        delay(250);
        if(result != Nuki::CmdResult::Success) {
            ++_retryCount;
        }
        else break;
    }

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
            char uidString[20];
            itoa(_nukiConfig.nukiId, uidString, 16);
            Log->print(F("Saving Nuki ID to preferences ("));
            Log->print(_nukiConfig.nukiId);
            Log->print(" / ");
            Log->print(uidString);            
            Log->println(")");
            _preferences->putUInt(preference_nuki_id_lock, _nukiConfig.nukiId);
        }

        if(_preferences->getUInt(preference_nuki_id_lock, 0) == _nukiConfig.nukiId)
        {
            _hasKeypad = _nukiConfig.hasKeypad > 0 || _nukiConfig.hasKeypadV2 > 0;
            _firmwareVersion = std::to_string(_nukiConfig.firmwareVersion[0]) + "." + std::to_string(_nukiConfig.firmwareVersion[1]) + "." + std::to_string(_nukiConfig.firmwareVersion[2]);
            _hardwareVersion = std::to_string(_nukiConfig.hardwareRevision[0]) + "." + std::to_string(_nukiConfig.hardwareRevision[1]);
            if(_preferences->getBool(preference_conf_info_enabled, true)) _network->publishConfig(_nukiConfig);
            _retryConfigCount = 0;

            if(_preferences->getBool(preference_timecontrol_info_enabled)) updateTimeControl(false);

            const int pinStatus = _preferences->getInt(preference_lock_pin_status, 4);

            if(isPinSet()) {
                Nuki::CmdResult result = (Nuki::CmdResult)-1;
                _retryCount = 0;
                Log->println(F("Nuki Lock PIN is set"));

                while(_retryCount < _nrOfRetries + 1)
                {
                    result = _nukiLock.verifySecurityPin();
                    delay(250);
                    if(result != Nuki::CmdResult::Success) {
                        ++_retryCount;
                    }
                    else break;
                }

                if(result != Nuki::CmdResult::Success)
                {
                    Log->println(F("Nuki Lock PIN is invalid"));
                    if(pinStatus != 2) {                        
                        _preferences->putInt(preference_lock_pin_status, 2);
                    }
                }
                else
                {
                    Log->println(F("Nuki Lock PIN is valid"));
                    if(pinStatus != 1) {
                        _preferences->putInt(preference_lock_pin_status, 1);
                    }
                }
            }
            else
            {
                Log->println(F("Nuki Lock PIN is not set"));
                if(pinStatus != 0) {
                    _preferences->putInt(preference_lock_pin_status, 0);
                }
            }
        }
        else
        {
            Log->println(F("Invalid/Unexpected config recieved, retrying"));
            expectedConfig = false;
            ++_retryConfigCount;
        }
    }
    else
    {
        Log->println(F("Invalid/Unexpected config recieved, retrying"));
        expectedConfig = false;
        ++_retryConfigCount;
    }
    if(_nukiAdvancedConfigValid && _preferences->getUInt(preference_nuki_id_lock, 0) == _nukiConfig.nukiId)
    {
        if(_preferences->getBool(preference_conf_info_enabled, true)) _network->publishAdvancedConfig(_nukiAdvancedConfig);
        _retryConfigCount = 0;
    }
    else
    {
        Log->println(F("Invalid/Unexpected advanced config recieved, retrying"));
        expectedConfig = false;
        ++_retryConfigCount;
    }
    if(!expectedConfig && _retryConfigCount < 11)
    {
        int64_t ts = (esp_timer_get_time() / 1000);
        _nextConfigUpdateTs = ts + 60000;
    }
}

void NukiWrapper::updateAuthData(bool retrieved)
{
    if(!isPinValid())
    {
        Log->println(F("No valid Nuki Lock PIN set"));
        return;
    }

    if(!retrieved)
    {
        delay(250);
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        _retryCount = 0;

        while(_retryCount < _nrOfRetries + 1)
        {
            Log->print(F("Retrieve log entries: "));
            result = _nukiLock.retrieveLogEntries(0, _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 1, false);
            delay(250);
            if(result != Nuki::CmdResult::Success) {
                ++_retryCount;
            }
            else break;
        }

        printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitAuthLogUpdateTs = (esp_timer_get_time() / 1000) + 5000;
            delay(100);

            std::list<NukiLock::LogEntry> log;
            _nukiLock.getLogEntries(&log);

            if(log.size() > _preferences->getInt(preference_authlog_max_entries, 3))
            {
                log.resize(_preferences->getInt(preference_authlog_max_entries, 3));
            }

            log.sort([](const NukiLock::LogEntry& a, const NukiLock::LogEntry& b) { return a.index < b.index; });

            if(log.size() > 0)
            {
                 _network->publishAuthorizationInfo(log, true);
            }
        }
    }
    else
    {
        std::list<NukiLock::LogEntry> log;
        _nukiLock.getLogEntries(&log);

        if(log.size() > _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG))
        {
            log.resize(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
        }

        log.sort([](const NukiLock::LogEntry& a, const NukiLock::LogEntry& b) { return a.index < b.index; });

        Log->print(F("Log size: "));
        Log->println(log.size());

        if(log.size() > 0)
        {
             _network->publishAuthorizationInfo(log, false);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::updateKeypad(bool retrieved)
{
    if(!_preferences->getBool(preference_keypad_info_enabled)) return;

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        _retryCount = 0;

        while(_retryCount < _nrOfRetries + 1)
        {
            Log->print(F("Querying lock keypad: "));
            result = _nukiLock.retrieveKeypadEntries(0, _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
            delay(250);
            if(result != Nuki::CmdResult::Success) {
                ++_retryCount;
            }
            else break;
        }

        printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitKeypadUpdateTs = (esp_timer_get_time() / 1000) + 5000;
        }
    }
    else
    {
        std::list<NukiLock::KeypadEntry> entries;
        _nukiLock.getKeypadEntries(&entries);

        Log->print(F("Lock keypad codes: "));
        Log->println(entries.size());

        entries.sort([](const NukiLock::KeypadEntry& a, const NukiLock::KeypadEntry& b) { return a.codeId < b.codeId; });

        if(entries.size() > _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD))
        {
            entries.resize(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
        }

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

void NukiWrapper::updateTimeControl(bool retrieved)
{
    if(!_preferences->getBool(preference_timecontrol_info_enabled)) return;

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        _retryCount = 0;

        while(_retryCount < _nrOfRetries + 1)
        {
            Log->print(F("Querying lock time control: "));
            result = _nukiLock.retrieveTimeControlEntries();
            delay(250);
            if(result != Nuki::CmdResult::Success) {
                ++_retryCount;
            }
            else break;
        }

        printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitTimeControlUpdateTs = (esp_timer_get_time() / 1000) + 5000;
        }
    }
    else
    {
        std::list<NukiLock::TimeControlEntry> timeControlEntries;
        _nukiLock.getTimeControlEntries(&timeControlEntries);

        Log->print(F("Lock time control entries: "));
        Log->println(timeControlEntries.size());

        timeControlEntries.sort([](const NukiLock::TimeControlEntry& a, const NukiLock::TimeControlEntry& b) { return a.entryId < b.entryId; });

        if(timeControlEntries.size() > _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL))
        {
            timeControlEntries.resize(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL));
        }

        uint timeControlCount = timeControlEntries.size();
        if(timeControlCount > _maxTimeControlEntryCount)
        {
            _maxTimeControlEntryCount = timeControlCount;
            _preferences->putUInt(preference_lock_max_timecontrol_entry_count, _maxTimeControlEntryCount);
        }

        _network->publishTimeControl(timeControlEntries, _maxTimeControlEntryCount);

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
    _disableBleWatchdogTs = (esp_timer_get_time() / 1000) + 15000;
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
            if((int)action == 0xff) return LockActionResult::UnknownAction;
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
        if(!networkInst->_offConnected) nukiInst->_nextLockAction = action;
        else
        {
            networkInst->_offCommandExecutedTs = (esp_timer_get_time() / 1000) + 2000;
            networkInst->_offCommand = action;
            networkInst->publishOffAction((int)action);
        }
        nukiLockPreferences->end();
        return LockActionResult::Success;
    }

    nukiLockPreferences->end();
    return LockActionResult::AccessDenied;
}

void NukiWrapper::onOfficialUpdateReceivedCallback(const char *topic, const char *value)
{
    nukiInst->onOfficialUpdateReceived(topic, value);
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

void NukiWrapper::onOfficialUpdateReceived(const char *topic, const char *value)
{
    char str[50];
    bool publishBatteryJson = false;
    memset(&str, 0, sizeof(str));

    Log->println("Official Nuki change recieved");
    Log->print(F("Topic: "));
    Log->println(topic);
    Log->print(F("Value: "));
    Log->println(value);

    if(strcmp(topic, mqtt_topic_official_connected) == 0)
    {
        Log->print(F("Connected: "));
        Log->println((strcmp(value, "true") == 0 ? 1 : 0));
        _network->_offConnected = (strcmp(value, "true") == 0 ? 1 : 0);

        if(!_network->_offConnected) _nextHybridLockStateUpdateTs = (esp_timer_get_time() / 1000) + _intervalHybridLockstate * 1000;
        else _nextHybridLockStateUpdateTs = 0;
    }
    else if(strcmp(topic, mqtt_topic_official_state) == 0)
    {
        _network->_offState = atoi(value);
        _statusUpdated = true;
        _network->publishStatusUpdated(_statusUpdated);
        NukiLock::lockstateToString((NukiLock::LockState)_network->_offState, str);
        _network->publishString(mqtt_topic_lock_state, str, true);

        Log->print(F("Lockstate: "));
        Log->println(str);

        if(_preferences->getString(preference_mqtt_hass_discovery) != "")
        {
            _network->publishState((NukiLock::LockState)_network->_offState);
        }
    }
    else if(strcmp(topic, mqtt_topic_official_doorsensorState) == 0)
    {
        _network->_offDoorsensorState = atoi(value);
        _statusUpdated = true;
        _network->publishStatusUpdated(_statusUpdated);
        NukiLock::doorSensorStateToString((NukiLock::DoorSensorState)_network->_offDoorsensorState, str);

        Log->print(F("Doorsensor state: "));
        Log->println(str);

        _network->publishString(mqtt_topic_lock_door_sensor_state, str, true);
    }
    else if(strcmp(topic, mqtt_topic_official_batteryCritical) == 0)
    {
        _network->_offCritical = (strcmp(value, "true") == 0 ? 1 : 0);

        Log->print(F("Battery critical: "));
        Log->println(_network->_offCritical);

        if(!_preferences->getBool(preference_disable_non_json, false)) _network->publishBool(mqtt_topic_battery_critical, _network->_offCritical, true);
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_batteryCharging) == 0)
    {
        _network->_offCharging = (strcmp(value, "true") == 0 ? 1 : 0);

        Log->print(F("Battery charging: "));
        Log->println(_network->_offCharging);

        if(!_preferences->getBool(preference_disable_non_json, false)) _network->publishBool(mqtt_topic_battery_charging, _network->_offCharging, true);
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_batteryChargeState) == 0)
    {
        _network->_offChargeState = atoi(value);

        Log->print(F("Battery level: "));
        Log->println(_network->_offChargeState);

        if(!_preferences->getBool(preference_disable_non_json, false)) _network->publishInt(mqtt_topic_battery_level, _network->_offChargeState, true);
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_keypadBatteryCritical) == 0)
    {
        _network->_offKeypadCritical = (strcmp(value, "true") == 0 ? 1 : 0);
        if(!_preferences->getBool(preference_disable_non_json, false)) _network->publishBool(mqtt_topic_battery_keypad_critical, _network->_offKeypadCritical, true);
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_doorsensorBatteryCritical) == 0)
    {
        _network->_offDoorsensorCritical = (strcmp(value, "true") == 0 ? 1 : 0);
        if(!_preferences->getBool(preference_disable_non_json, false)) _network->publishBool(mqtt_topic_battery_doorsensor_critical, _network->_offDoorsensorCritical, true);
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_commandResponse) == 0)
    {
        _network->_offCommandResponse = atoi(value);
        if(_network->_offCommandResponse == 0) networkInst->_offCommandExecutedTs = 0;
        char resultStr[15] = {0};
        NukiLock::cmdResultToString((Nuki::CmdResult)_network->_offCommandResponse, resultStr);
        _network->publishCommandResult(resultStr);
    }
    else if(strcmp(topic, mqtt_topic_official_lockActionEvent) == 0)
    {
        networkInst->_offCommandExecutedTs = 0;
        _network->_offLockActionEvent = (char*)value;
        String LockActionEvent = _network->_offLockActionEvent;
        const int ind1 = LockActionEvent.indexOf(',');
        const int ind2 = LockActionEvent.indexOf(',', ind1+1);
        const int ind3 = LockActionEvent.indexOf(',', ind2+1);
        const int ind4 = LockActionEvent.indexOf(',', ind3+1);
        const int ind5 = LockActionEvent.indexOf(',', ind4+1);

        _network->_offLockAction = atoi(LockActionEvent.substring(0, ind1).c_str());
        _network->_offTrigger = atoi(LockActionEvent.substring(ind1+1, ind2+1).c_str());
        _network->_offAuthId = atoi(LockActionEvent.substring(ind2+1, ind3+1).c_str());
        _network->_offCodeId = atoi(LockActionEvent.substring(ind3+1, ind4+1).c_str());
        _network->_offContext = atoi(LockActionEvent.substring(ind4+1, ind5+1).c_str());

        memset(&str, 0, sizeof(str));
        lockactionToString((NukiLock::LockAction)_network->_offLockAction, str);
        _network->publishString(mqtt_topic_lock_last_lock_action, str, true);

        memset(&str, 0, sizeof(str));
        triggerToString((NukiLock::Trigger)_network->_offTrigger, str);
        _network->publishString(mqtt_topic_lock_trigger, str, true);

        if(_network->_offAuthId > 0 || _network->_offCodeId > 0)
        {
            _network->_authFound = true;

            if(_network->_offCodeId > 0) _network->_authId = _network->_offCodeId;
            else _network->_authId = _network->_offAuthId;

            /*
            _network->_authName = RETRIEVE FROM VECTOR AFTER AUTHORIZATION ENTRIES ARE IMPLEMENTED;
            _network->_offContext = BASE ON CONTEXT OF TRIGGER AND PUBLISH TO MQTT;
            */
        }
    }

    if(publishBatteryJson)
    {
        JsonDocument jsonBattery;
        char _resbuf[2048];
        jsonBattery["critical"] = _network->_offCritical ? "1" : "0";
        jsonBattery["charging"] = _network->_offCharging ? "1" : "0";
        jsonBattery["level"] = _network->_offChargeState;
        jsonBattery["keypadCritical"] = _network->_offKeypadCritical ? "1" : "0";
        jsonBattery["doorSensorCritical"] = _network->_offDoorsensorCritical ? "1" : "0";
        serializeJson(jsonBattery, _resbuf, sizeof(_resbuf));
        _network->publishString(mqtt_topic_battery_basic_json, _resbuf, true);
    }
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
    const char *basicKeys[16] = {"name", "latitude", "longitude", "autoUnlatch", "pairingEnabled", "buttonEnabled", "ledEnabled", "ledBrightness", "timeZoneOffset", "dstMode", "fobAction1",  "fobAction2", "fobAction3", "singleLock", "advertisingMode", "timeZone"};
    const char *advancedKeys[22] = {"unlockedPositionOffsetDegrees", "lockedPositionOffsetDegrees", "singleLockedPositionOffsetDegrees", "unlockedToLockedTransitionOffsetDegrees", "lockNgoTimeout", "singleButtonPressAction", "doubleButtonPressAction", "detachedCylinder", "batteryType", "automaticBatteryTypeDetection", "unlatchDuration", "autoLockTimeOut",  "autoUnLockDisabled", "nightModeEnabled", "nightModeStartTime", "nightModeEndTime", "nightModeAutoLockEnabled", "nightModeAutoUnlockDisabled", "nightModeImmediateLockOnStart", "autoLockEnabled", "immediateAutoLockEnabled", "autoUpdateEnabled"};
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
                _retryCount = 0;

                while(_retryCount < _nrOfRetries + 1)
                {
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

                    delay(250);
                    if(!cmdResult == Nuki::CmdResult::Success) {
                        ++_retryCount;
                    }
                    else break;
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

    for(int j=0; j < 22; j++)
    {
        if(json[advancedKeys[j]])
        {
            const char *jsonchar = json[advancedKeys[j]].as<const char*>();

            if(strlen(jsonchar) == 0)
            {
                jsonResult[advancedKeys[j]] = "noValueSet";
                continue;
            }

            if((int)advancedLockConfigAclPrefs[j] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;
                _retryCount = 0;

                while(_retryCount < _nrOfRetries + 1)
                {
                    if(strcmp(advancedKeys[j], "unlockedPositionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -90 && keyvalue <= 180)
                        {
                            if(_nukiAdvancedConfig.unlockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setUnlockedPositionOffsetDegrees(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "lockedPositionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -180 && keyvalue <= 90)
                        {
                            if(_nukiAdvancedConfig.lockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setLockedPositionOffsetDegrees(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "singleLockedPositionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -180 && keyvalue <= 180)
                        {
                            if(_nukiAdvancedConfig.singleLockedPositionOffsetDegrees == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setSingleLockedPositionOffsetDegrees(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "unlockedToLockedTransitionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -180 && keyvalue <= 180)
                        {
                            if(_nukiAdvancedConfig.unlockedToLockedTransitionOffsetDegrees == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setUnlockedToLockedTransitionOffsetDegrees(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "lockNgoTimeout") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 5 && keyvalue <= 60)
                        {
                            if(_nukiAdvancedConfig.lockNgoTimeout == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setLockNgoTimeout(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "singleButtonPressAction") == 0)
                    {
                        NukiLock::ButtonPressAction sbpa = nukiInst->buttonPressActionToEnum(jsonchar);

                        if((int)sbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.singleButtonPressAction == sbpa) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setSingleButtonPressAction(sbpa);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "doubleButtonPressAction") == 0)
                    {
                        NukiLock::ButtonPressAction dbpa = nukiInst->buttonPressActionToEnum(jsonchar);

                        if((int)dbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.doubleButtonPressAction == dbpa) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setDoubleButtonPressAction(dbpa);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "detachedCylinder") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.detachedCylinder == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableDetachedCylinder((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "batteryType") == 0)
                    {
                        Nuki::BatteryType battype = nukiInst->batteryTypeToEnum(jsonchar);

                        if((int)battype != 0xff)
                        {
                            if(_nukiAdvancedConfig.batteryType == battype) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setBatteryType(battype);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "automaticBatteryTypeDetection") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableAutoBatteryTypeDetection((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "unlatchDuration") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 1 && keyvalue <= 30)
                        {
                            if(_nukiAdvancedConfig.unlatchDuration == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setUnlatchDuration(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "autoLockTimeOut") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 30 && keyvalue <= 180)
                        {
                            if(_nukiAdvancedConfig.autoLockTimeOut == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setAutoLockTimeOut(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "autoUnLockDisabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.autoUnLockDisabled == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.disableAutoUnlock((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "nightModeEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeEnabled == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableNightMode((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "nightModeStartTime") == 0)
                    {
                        String keystr = jsonchar;
                        unsigned char keyvalue[2];
                        keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                        keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                        if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                        {
                            if(_nukiAdvancedConfig.nightModeStartTime == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setNightModeStartTime(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "nightModeEndTime") == 0)
                    {
                        String keystr = jsonchar;
                        unsigned char keyvalue[2];
                        keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                        keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                        if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                        {
                            if(_nukiAdvancedConfig.nightModeEndTime == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.setNightModeEndTime(keyvalue);
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "nightModeAutoLockEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeAutoLockEnabled == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableNightModeAutoLock((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "nightModeAutoUnlockDisabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeAutoUnlockDisabled == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.disableNightModeAutoUnlock((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "nightModeImmediateLockOnStart") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeImmediateLockOnStart == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableNightModeImmediateLockOnStart((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "autoLockEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.autoLockEnabled == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableAutoLock((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "immediateAutoLockEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.immediateAutoLockEnabled == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableImmediateAutoLock((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }
                    else if(strcmp(advancedKeys[j], "autoUpdateEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.autoUpdateEnabled == keyvalue) jsonResult[advancedKeys[j]] = "unchanged";
                            else cmdResult = _nukiLock.enableAutoUpdate((keyvalue > 0));
                        }
                        else jsonResult[advancedKeys[j]] = "invalidValue";
                    }

                    delay(250);
                    if(!cmdResult == Nuki::CmdResult::Success) {
                        ++_retryCount;
                    }
                    else break;
                }

                if(cmdResult == Nuki::CmdResult::Success) advancedUpdated = true;

                if(!jsonResult[advancedKeys[j]]) {
                    char resultStr[15] = {0};
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[advancedKeys[j]] = resultStr;
                }
            }
            else jsonResult[advancedKeys[j]] = "accessDenied";
        }
    }

    nukiLockPreferences->end();

    if(basicUpdated || advancedUpdated) jsonResult["general"] = "success";
    else jsonResult["general"] = "noChange";

    _nextConfigUpdateTs = (esp_timer_get_time() / 1000) + 300;

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
    nukiLockPreferences = new Preferences();
    nukiLockPreferences->begin("nukihub", true);

    switch(action)
    {
        case GpioAction::Lock:
            if(!networkInst->_offConnected) nukiInst->lock();
            else
            {
                networkInst->_offCommandExecutedTs = (esp_timer_get_time() / 1000) + 2000;
                networkInst->_offCommand = NukiLock::LockAction::Lock;
                networkInst->publishOffAction(2);
            }
            break;
        case GpioAction::Unlock:
            if(!networkInst->_offConnected) nukiInst->unlock();
            else
            {
                networkInst->_offCommandExecutedTs = (esp_timer_get_time() / 1000) + 2000;
                networkInst->_offCommand = NukiLock::LockAction::Unlock;
                networkInst->publishOffAction(1);
            }
            break;
        case GpioAction::Unlatch:
            if(!networkInst->_offConnected) nukiInst->unlatch();
            else
            {
                networkInst->_offCommandExecutedTs = (esp_timer_get_time() / 1000) + 2000;
                networkInst->_offCommand = NukiLock::LockAction::Unlatch;
                networkInst->publishOffAction(3);
            }
            break;
        case GpioAction::LockNgo:
            if(!networkInst->_offConnected) nukiInst->lockngo();
            else
            {
                networkInst->_offCommandExecutedTs = (esp_timer_get_time() / 1000) + 2000;
                networkInst->_offCommand = NukiLock::LockAction::LockNgo;
                networkInst->publishOffAction(4);
            }
            break;
        case GpioAction::LockNgoUnlatch:
            if(!networkInst->_offConnected) nukiInst->lockngounlatch();
            else
            {
                networkInst->_offCommandExecutedTs = (esp_timer_get_time() / 1000) + 2000;
                networkInst->_offCommand = NukiLock::LockAction::LockNgoUnlatch;
                networkInst->publishOffAction(5);
            }
            break;
    }

    nukiLockPreferences->end();
}

void NukiWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    if(_preferences->getBool(preference_disable_non_json, false)) return;

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
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    _retryCount = 0;

    while(_retryCount < _nrOfRetries + 1)
    {
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
            delay(250);
            Log->print("Add keypad code: "); Log->println((int)result);
            updateKeypad(false);
        }
        else if(strcmp(command, "delete") == 0)
        {
            if(!idExists)
            {
                _network->publishKeypadCommandResult("UnknownId");
                return;
            }

            result = _nukiLock.deleteKeypadEntry(id);
            delay(250);
            Log->print("Delete keypad code: "); Log->println((int)result);
            updateKeypad(false);
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
            delay(250);
            Log->print("Update keypad code: "); Log->println((int)result);
            updateKeypad(false);
        }
        else if(strcmp(command, "--") == 0)
        {
            return;
        }
        else
        {
            _network->publishKeypadCommandResult("UnknownCommand");
            return;
        }

        if(result != Nuki::CmdResult::Success) {
            ++_retryCount;
        }
        else break;
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

    char oldName[21];
    const char *action = json["action"].as<const char*>();
    uint16_t codeId = json["codeId"].as<unsigned int>();
    uint32_t code;
    uint8_t enabled;
    uint8_t timeLimited;
    String name;
    String allowedFrom;
    String allowedUntil;
    String allowedWeekdays;
    String allowedFromTime;
    String allowedUntilTime;

    if(json.containsKey("code")) code = json["code"].as<unsigned int>();
    else code = 12;

    if(json.containsKey("enabled")) enabled = json["enabled"].as<unsigned int>();
    else enabled = 2;

    if(json.containsKey("timeLimited")) timeLimited = json["timeLimited"].as<unsigned int>();
    else timeLimited = 2;

    if(json.containsKey("name")) name = json["name"].as<String>();
    if(json.containsKey("allowedFrom")) allowedFrom = json["allowedFrom"].as<String>();
    if(json.containsKey("allowedUntil")) allowedUntil = json["allowedUntil"].as<String>();
    if(json.containsKey("allowedWeekdays")) allowedWeekdays = json["allowedWeekdays"].as<String>();
    if(json.containsKey("allowedFromTime")) allowedFromTime = json["allowedFromTime"].as<String>();
    if(json.containsKey("allowedUntilTime")) allowedUntilTime = json["allowedUntilTime"].as<String>();

    if(action)
    {
        bool idExists = false;

        if(codeId)
        {
            idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), codeId) != _keypadCodeIds.end();
        }

        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        _retryCount = 0;

        while(_retryCount < _nrOfRetries + 1)
        {
            if(strcmp(action, "delete") == 0) {
                if(idExists)
                {
                    result = _nukiLock.deleteKeypadEntry(codeId);
                    delay(250);
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
                if(name.length() < 1)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        _network->publishKeypadJsonCommandResult("noNameSet");
                        return;
                    }
                }

                if(!code == 12)
                {
                    String codeStr = json["code"].as<String>();
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
                    if(allowedFrom.length() > 0)
                    {
                        if(allowedFrom.length() == 19)
                        {
                            allowedFromAr[0] = (uint16_t)allowedFrom.substring(0, 4).toInt();
                            allowedFromAr[1] = (uint8_t)allowedFrom.substring(5, 7).toInt();
                            allowedFromAr[2] = (uint8_t)allowedFrom.substring(8, 10).toInt();
                            allowedFromAr[3] = (uint8_t)allowedFrom.substring(11, 13).toInt();
                            allowedFromAr[4] = (uint8_t)allowedFrom.substring(14, 16).toInt();
                            allowedFromAr[5] = (uint8_t)allowedFrom.substring(17, 19).toInt();

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

                    if(allowedUntil.length() > 0)
                    {
                        if(allowedUntil.length() > 0 == 19)
                        {
                            allowedUntilAr[0] = (uint16_t)allowedUntil.substring(0, 4).toInt();
                            allowedUntilAr[1] = (uint8_t)allowedUntil.substring(5, 7).toInt();
                            allowedUntilAr[2] = (uint8_t)allowedUntil.substring(8, 10).toInt();
                            allowedUntilAr[3] = (uint8_t)allowedUntil.substring(11, 13).toInt();
                            allowedUntilAr[4] = (uint8_t)allowedUntil.substring(14, 16).toInt();
                            allowedUntilAr[5] = (uint8_t)allowedUntil.substring(17, 19).toInt();

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

                    if(allowedFromTime.length() > 0)
                    {
                        if(allowedFromTime.length() == 5)
                        {
                            allowedFromTimeAr[0] = (uint8_t)allowedFromTime.substring(0, 2).toInt();
                            allowedFromTimeAr[1] = (uint8_t)allowedFromTime.substring(3, 5).toInt();

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

                    if(allowedUntilTime.length() > 0)
                    {
                        if(allowedUntilTime.length() == 5)
                        {
                            allowedUntilTimeAr[0] = (uint8_t)allowedUntilTime.substring(0, 2).toInt();
                            allowedUntilTimeAr[1] = (uint8_t)allowedUntilTime.substring(3, 5).toInt();

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
                    size_t nameLen = name.length();
                    memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                    entry.code = code;
                    entry.timeLimited = timeLimited == 1 ? 1 : 0;

                    if(allowedFrom.length() > 0)
                    {
                        entry.allowedFromYear = allowedFromAr[0];
                        entry.allowedFromMonth = allowedFromAr[1];
                        entry.allowedFromDay = allowedFromAr[2];
                        entry.allowedFromHour = allowedFromAr[3];
                        entry.allowedFromMin = allowedFromAr[4];
                        entry.allowedFromSec = allowedFromAr[5];
                    }

                    if(allowedUntil.length() > 0)
                    {
                        entry.allowedUntilYear = allowedUntilAr[0];
                        entry.allowedUntilMonth = allowedUntilAr[1];
                        entry.allowedUntilDay = allowedUntilAr[2];
                        entry.allowedUntilHour = allowedUntilAr[3];
                        entry.allowedUntilMin = allowedUntilAr[4];
                        entry.allowedUntilSec = allowedUntilAr[5];
                    }

                    entry.allowedWeekdays = allowedWeekdaysInt;

                    if(allowedFromTime.length() > 0)
                    {
                        entry.allowedFromTimeHour = allowedFromTimeAr[0];
                        entry.allowedFromTimeMin = allowedFromTimeAr[1];
                    }

                    if(allowedUntilTime.length() > 0)
                    {
                        entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                        entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                    }

                    result = _nukiLock.addKeypadEntry(entry);
                    delay(250);
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

                    Nuki::CmdResult resultKp = _nukiLock.retrieveKeypadEntries(0, _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
                    delay(250);
                    bool foundExisting = false;

                    if(resultKp == Nuki::CmdResult::Success)
                    {
                        std::list<NukiLock::KeypadEntry> entries;
                        _nukiLock.getKeypadEntries(&entries);

                        for(const auto& entry : entries)
                        {
                            if (codeId != entry.codeId) continue;
                            else foundExisting = true;

                            if(name.length() < 1)
                            {
                                memset(oldName, 0, sizeof(oldName));
                                memcpy(oldName, entry.name, sizeof(entry.name));
                            }
                            if(code == 12) code = entry.code;
                            if(enabled == 2) enabled = entry.enabled;
                            if(timeLimited == 2) timeLimited = entry.timeLimited;
                            if(allowedFrom.length() < 1)
                            {
                                allowedFrom = "old";
                                allowedFromAr[0] = entry.allowedFromYear;
                                allowedFromAr[1] = entry.allowedFromMonth;
                                allowedFromAr[2] = entry.allowedFromDay;
                                allowedFromAr[3] = entry.allowedFromHour;
                                allowedFromAr[4] = entry.allowedFromMin;
                                allowedFromAr[5] = entry.allowedFromSec;
                            }
                            if(allowedUntil.length() < 1)
                            {
                                allowedUntil = "old";
                                allowedUntilAr[0] = entry.allowedUntilYear;
                                allowedUntilAr[1] = entry.allowedUntilMonth;
                                allowedUntilAr[2] = entry.allowedUntilDay;
                                allowedUntilAr[3] = entry.allowedUntilHour;
                                allowedUntilAr[4] = entry.allowedUntilMin;
                                allowedUntilAr[5] = entry.allowedUntilSec;
                            }
                            if(allowedWeekdays.length() < 1) allowedWeekdaysInt = entry.allowedWeekdays;
                            if(allowedFromTime.length() < 1)
                            {
                                allowedFromTime = "old";
                                allowedFromTimeAr[0] = entry.allowedFromTimeHour;
                                allowedFromTimeAr[1] = entry.allowedFromTimeMin;
                            }

                            if(allowedUntilTime.length() < 1)
                            {
                                allowedUntilTime = "old";
                                allowedUntilTimeAr[0] = entry.allowedUntilTimeHour;
                                allowedUntilTimeAr[1] = entry.allowedUntilTimeMin;
                            }
                        }

                        if(!foundExisting)
                        {
                            _network->publishKeypadJsonCommandResult("failedToRetrieveExistingKeypadEntry");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishKeypadJsonCommandResult("failedToRetrieveExistingKeypadEntry");
                        return;
                    }

                    NukiLock::UpdatedKeypadEntry entry;

                    memset(&entry, 0, sizeof(entry));
                    entry.codeId = codeId;
                    entry.code = code;

                    if(name.length() < 1)
                    {
                        size_t nameLen = strlen(oldName);
                        memcpy(&entry.name, oldName, nameLen > 20 ? 20 : nameLen);
                    }
                    else
                    {
                        size_t nameLen = name.length();
                        memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                    }
                    entry.enabled = enabled;
                    entry.timeLimited = timeLimited;

                    if(enabled == 1)
                    {
                        if(timeLimited == 1)
                        {
                            if(allowedFrom.length() > 0)
                            {
                                entry.allowedFromYear = allowedFromAr[0];
                                entry.allowedFromMonth = allowedFromAr[1];
                                entry.allowedFromDay = allowedFromAr[2];
                                entry.allowedFromHour = allowedFromAr[3];
                                entry.allowedFromMin = allowedFromAr[4];
                                entry.allowedFromSec = allowedFromAr[5];
                            }

                            if(allowedUntil.length() > 0)
                            {
                                entry.allowedUntilYear = allowedUntilAr[0];
                                entry.allowedUntilMonth = allowedUntilAr[1];
                                entry.allowedUntilDay = allowedUntilAr[2];
                                entry.allowedUntilHour = allowedUntilAr[3];
                                entry.allowedUntilMin = allowedUntilAr[4];
                                entry.allowedUntilSec = allowedUntilAr[5];
                            }

                            entry.allowedWeekdays = allowedWeekdaysInt;

                            if(allowedFromTime.length() > 0)
                            {
                                entry.allowedFromTimeHour = allowedFromTimeAr[0];
                                entry.allowedFromTimeMin = allowedFromTimeAr[1];
                            }

                            if(allowedUntilTime.length() > 0)
                            {
                                entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                                entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                            }
                        }
                    }

                    result = _nukiLock.updateKeypadEntry(entry);
                    delay(250);
                    Log->print(F("Update keypad code: "));
                    Log->println((int)result);
                }
            }
            else
            {
                _network->publishKeypadJsonCommandResult("invalidAction");
                return;
            }

            if(result != Nuki::CmdResult::Success) {
                ++_retryCount;
            }
            else break;
        }

        updateKeypad(false);

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

    const char *action = json["action"].as<const char*>();
    uint8_t entryId = json["entryId"].as<unsigned int>();
    uint8_t enabled;
    String weekdays;
    String time;
    String lockAction;
    NukiLock::LockAction timeControlLockAction;

    if(json.containsKey("enabled")) enabled = json["enabled"].as<unsigned int>();
    else enabled = 2;

    if(json.containsKey("weekdays")) weekdays = json["weekdays"].as<String>();
    if(json.containsKey("time")) time = json["time"].as<String>();
    if(json.containsKey("lockAction")) lockAction = json["lockAction"].as<String>();

    if(lockAction.length() > 0)
    {
        timeControlLockAction = nukiInst->lockActionToEnum(lockAction.c_str());

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

        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        _retryCount = 0;

        while(_retryCount < _nrOfRetries + 1)
        {
            if(strcmp(action, "delete") == 0) {
                if(idExists)
                {
                    result = _nukiLock.removeTimeControlEntry(entryId);
                    delay(250);
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

                if(time.length() > 0)
                {
                    if(time.length() == 5)
                    {
                        timeAr[0] = (uint8_t)time.substring(0, 2).toInt();
                        timeAr[1] = (uint8_t)time.substring(3, 5).toInt();

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

                    if(time.length() > 0)
                    {
                        entry.timeHour = timeAr[0];
                        entry.timeMin = timeAr[1];
                    }

                    entry.lockAction = timeControlLockAction;

                    result = _nukiLock.addTimeControlEntry(entry);
                    delay(250);
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

                    Nuki::CmdResult resultTc = _nukiLock.retrieveTimeControlEntries();
                    delay(250);
                    bool foundExisting = false;

                    if(resultTc == Nuki::CmdResult::Success)
                    {
                        std::list<NukiLock::TimeControlEntry> timeControlEntries;
                        _nukiLock.getTimeControlEntries(&timeControlEntries);

                        for(const auto& entry : timeControlEntries)
                        {
                            if (entryId != entry.entryId) continue;
                            else foundExisting = true;

                            if(enabled == 2) enabled = entry.enabled;
                            if(weekdays.length() < 1) weekdaysInt = entry.weekdays;
                            if(time.length() < 1)
                            {
                                time = "old";
                                timeAr[0] = entry.timeHour;
                                timeAr[1] = entry.timeMin;
                            }
                            if(lockAction.length() < 1) timeControlLockAction = entry.lockAction;
                        }

                        if(!foundExisting)
                        {
                            _network->publishTimeControlCommandResult("failedToRetrieveExistingKeypadEntry");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishTimeControlCommandResult("failedToRetrieveExistingKeypadEntry");
                        return;
                    }

                    NukiLock::TimeControlEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    entry.entryId = entryId;
                    entry.enabled = enabled;
                    entry.weekdays = weekdaysInt;

                    if(time.length() > 0)
                    {
                        entry.timeHour = timeAr[0];
                        entry.timeMin = timeAr[1];
                    }

                    entry.lockAction = timeControlLockAction;

                    result = _nukiLock.updateTimeControlEntry(entry);
                    delay(250);
                    Log->print(F("Update time control: "));
                    Log->println((int)result);
                }
            }
            else
            {
                _network->publishTimeControlCommandResult("invalidAction");
                return;
            }

            if(result != Nuki::CmdResult::Success) {
                ++_retryCount;
            }
            else break;
        }

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            NukiLock::cmdResultToString(result, resultStr);
            _network->publishTimeControlCommandResult(resultStr);
        }

        _nextConfigUpdateTs = (esp_timer_get_time() / 1000) + 300;
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
    if(!_network->_offConnected)
    {
        if(_preferences->getBool(preference_official_hybrid, false) && _intervalHybridLockstate > 0 && (esp_timer_get_time() / 1000) > (_intervalHybridLockstate * 1000))
        {
            Log->println("OffKeyTurnerStatusUpdated");
            _statusUpdated = true;
            _nextHybridLockStateUpdateTs = (esp_timer_get_time() / 1000) + _intervalHybridLockstate * 1000;
        }
        else
        {
            if(eventType == Nuki::EventType::KeyTurnerStatusUpdated)
            {
                Log->println("KeyTurnerStatusUpdated");
                _statusUpdated = true;
                _network->publishStatusUpdated(_statusUpdated);
            }
        }
    }
}

void NukiWrapper::readConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    _retryCount = 0;

    while(_retryCount < _nrOfRetries + 1)
    {
        result = _nukiLock.requestConfig(&_nukiConfig);
        delay(250);
        _nukiConfigValid = result == Nuki::CmdResult::Success;

        Log->print(F("Config valid: "));
        Log->println(_nukiConfigValid);
        if(!_nukiConfigValid) {
            ++_retryCount;
            Log->println("Retrying in 1s");
        }
        else break;
    }

    char resultStr[20];
    NukiLock::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}

void NukiWrapper::readAdvancedConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    _retryCount = 0;

    while(_retryCount < _nrOfRetries + 1)
    {
         result = _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
        delay(250);
        _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;

        if(!_nukiAdvancedConfigValid) {
            ++_retryCount;
        }
        else break;
    }

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

    _network->publishHASSConfig((char*)"SmartLock", baseTopic.c_str(),(char*)_nukiConfig.name, uidString, _firmwareVersion.c_str(), _hardwareVersion.c_str(), hasDoorSensor(), _hasKeypad, _publishAuthData, (char*)"lock", (char*)"unlock", (char*)"unlatch");
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
    if(!_nukiConfigValid)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        _retryCount = 0;

        while(_retryCount < _nrOfRetries + 1)
        {
            result = _nukiLock.requestConfig(&_nukiConfig);
            delay(250);
            _nukiConfigValid = result == Nuki::CmdResult::Success;

            if(!_nukiConfigValid) {
                ++_retryCount;
            }
            else break;
        }
    }

    if(_nukiConfigValid)
    {
        char uidString[20];
        itoa(_nukiConfig.nukiId, uidString, 16);
        _network->removeHASSConfig(uidString);
    }
    else Log->println(F("Unable to disable HASS. Invalid config received."));
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

