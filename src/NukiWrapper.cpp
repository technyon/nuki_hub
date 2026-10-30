#include "NukiWrapper.h"
#include "UpdateRequest.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "Logger.h"
#include "RestartReason.h"
#include <NukiLockUtils.h>
#include "Config.h"
#include "enums/NukiPinState.h"
#include "hal/wdt_hal.h"
#include <time.h>
#include "esp_sntp.h"
#include "util/NukiHelper.h"
#include "util/NukiRetryHandler.h"

NukiWrapper* nukiInst = nullptr;

NukiWrapper::NukiWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NukiNetworkLock* network, NukiOfficial* nukiOfficial, Gpio* gpio, Preferences* preferences, char* buffer, size_t bufferSize)
    : _deviceName(deviceName),
      _deviceId(deviceId),
      _bleScanner(scanner),
      _nukiLock(deviceName, _deviceId->get()),
      _network(network),
      _nukiOfficial(nukiOfficial),
      _gpio(gpio),
      _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize)
{

    Log->print("Device id lock: ");
    Log->println(_deviceId->get());

    nukiInst = this;

    memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    _keyTurnerState.lockState = NukiLock::LockState::Undefined;

#ifndef NUKI_HUB_UPDATER
    _pinsHighWhenDoorClosed = _gpio->getPinsWithRole(PinRole::OutputHighDoorClosed);
    _pinsHighWhenDoorOpen = _gpio->getPinsWithRole(PinRole::OutputHighDoorOpen);
#endif

    network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
    network->setOfficialUpdateReceivedCallback(nukiInst->onOfficialUpdateReceivedCallback);
    network->setConfigUpdateReceivedCallback(nukiInst->onConfigUpdateReceivedCallback);
    network->setKeypadCommandReceivedCallback(nukiInst->onKeypadCommandReceivedCallback);
    network->setKeypadJsonCommandReceivedCallback(nukiInst->onKeypadJsonCommandReceivedCallback);
    network->setTimeControlCommandReceivedCallback(nukiInst->onTimeControlCommandReceivedCallback);
    network->setAuthCommandReceivedCallback(nukiInst->onAuthCommandReceivedCallback);

    _gpio->addCallback(NukiWrapper::gpioActionCallback);
}

NukiWrapper::~NukiWrapper()
{
    _bleScanner = nullptr;
}

void NukiWrapper::initialize()
{
    _commandQueue = xQueueCreate(NUKI_COMMAND_QUEUE_SIZE, sizeof(NukiQueuedCommand));
    _nukiLock.setDebugConnect(_preferences->getBool(preference_debug_connect, false));
    _nukiLock.setDebugCommunication(_preferences->getBool(preference_debug_communication, false));
    _nukiLock.setDebugReadableData(_preferences->getBool(preference_debug_readable_data, false));
    _nukiLock.setDebugHexData(_preferences->getBool(preference_debug_hex_data, false));
    _nukiLock.setDebugCommand(_preferences->getBool(preference_debug_command, false));
    _nukiLock.registerLogger(Log);

    if (_preferences->getInt(preference_lock_gemini_pin, 0) > 0 && _preferences->getBool(preference_lock_gemini_enabled, false))
    {
        _nukiLock.saveUltraPincode(_preferences->getInt(preference_lock_gemini_pin, 0), false);
    }

    _nukiLock.initialize();
    _nukiLock.registerBleScanner(_bleScanner);
    _nukiLock.setEventHandler(this);
    _nukiLock.setConnectTimeout(3);
    _nukiLock.setDisconnectTimeout(2000);
    _nukiLock.setGeneralTimeout(_preferences->getInt(preference_ble_general_timeout, 10000));
    _nukiLock.setCommandTimeout(_preferences->getInt(preference_ble_command_timeout, 3000));

    _hassEnabled = _preferences->getBool(preference_mqtt_hass_enabled, false);
    readSettings();

#ifndef NUKI_HUB_UPDATER
    _nukiRetryHandler = new NukiRetryHandler("Lock", _gpio, _gpio->getPinsWithRole(PinRole::OutputHighBluetoothComm), _gpio->getPinsWithRole(PinRole::OutputHighBluetoothCommError), _nrOfRetries, _retryDelay);
#endif
}

void NukiWrapper::readSettings()
{
    esp_power_level_t powerLevel;
    int pwrLvl = _preferences->getInt(preference_ble_tx_power, 9);

    if(pwrLvl >= 9)
    {
#if defined(CONFIG_IDF_TARGET_ESP32)
        powerLevel = ESP_PWR_LVL_P9;
#else
        if(pwrLvl >= 20)
        {
            powerLevel = ESP_PWR_LVL_P20;
        }
        else if(pwrLvl >= 18)
        {
            powerLevel = ESP_PWR_LVL_P18;
        }
        else if(pwrLvl >= 15)
        {
            powerLevel = ESP_PWR_LVL_P15;
        }
        else if(pwrLvl >= 12)
        {
            powerLevel = ESP_PWR_LVL_P12;
        }
        else
        {
            powerLevel = ESP_PWR_LVL_P9;
        }
#endif
    }
    else if(pwrLvl >= 6)
    {
        powerLevel = ESP_PWR_LVL_P6;
    }
    else if(pwrLvl >= 3)
    {
        powerLevel = ESP_PWR_LVL_P3;
    }
    else if(pwrLvl >= 0)
    {
        powerLevel = ESP_PWR_LVL_N0;
    }
    else if(pwrLvl >= -3)
    {
        powerLevel = ESP_PWR_LVL_N3;
    }
    else if(pwrLvl >= -6)
    {
        powerLevel = ESP_PWR_LVL_N6;
    }
    else if(pwrLvl >= -9)
    {
        powerLevel = ESP_PWR_LVL_N9;
    }
    else
    {
        powerLevel = ESP_PWR_LVL_N12;
    }

    _nukiLock.setPower(powerLevel);

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalHybridLockstate = _preferences->getInt(preference_query_interval_hybrid_lockstate);
    _intervalConfig = _preferences->getInt(preference_query_interval_configuration);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);
    _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
    _keypadEnabled = _preferences->getBool(preference_keypad_info_enabled);
    _publishAuthData = _preferences->getBool(preference_publish_authdata);
    _maxKeypadCodeCount = _preferences->getUInt(preference_lock_max_keypad_code_count);
    _maxTimeControlEntryCount = _preferences->getUInt(preference_lock_max_timecontrol_entry_count);
    _maxAuthEntryCount = _preferences->getUInt(preference_lock_max_auth_entry_count);
    _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
    _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries, 200);
    _retryDelay = _preferences->getInt(preference_command_retry_delay);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;
    _disableNonJSON = _preferences->getBool(preference_disable_non_json, false);
    _checkKeypadCodes = _preferences->getBool(preference_keypad_check_code_enabled, false);
    _pairedAsApp = _preferences->getBool(preference_register_as_app, false);
    _forceDoorsensor = _preferences->getBool(preference_lock_force_doorsensor, false);
    _forceKeypad = _preferences->getBool(preference_lock_force_keypad, false);
    _forceId = _preferences->getBool(preference_lock_force_id, false);
    _isUltra = _preferences->getBool(preference_lock_gemini_enabled, false);
    _isDebugging = _preferences->getBool(preference_debug_hex_data, false);

    _preferences->getBytes(preference_acl, &_aclPrefs, sizeof(_aclPrefs));
    _preferences->getBytes(preference_conf_lock_basic_acl, &_basicLockConfigaclPrefs, sizeof(_basicLockConfigaclPrefs));
    _preferences->getBytes(preference_conf_lock_advanced_acl, &_advancedLockConfigaclPrefs, sizeof(_advancedLockConfigaclPrefs));

    if(_nrOfRetries < 0 || _nrOfRetries == 200)
    {
        Log->println("Invalid nrOfRetries, revert to default (3)");
        _nrOfRetries = 3;
        _preferences->putInt(preference_command_nr_of_retries, _nrOfRetries);
    }
    if(_retryDelay < 100)
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
    if(_restartBeaconTimeout != -1 && _restartBeaconTimeout < 10)
    {
        Log->println("Invalid restartBeaconTimeout, revert to default (-1)");
        _restartBeaconTimeout = -1;
        _preferences->putInt(preference_restart_ble_beacon_lost, _restartBeaconTimeout);
    }

    Log->print("Lock state interval: ");
    Log->print(_intervalLockstate);
    Log->print(" | Battery interval: ");
    Log->print(_intervalBattery);
    Log->print(" | Publish auth data: ");
    Log->println(_publishAuthData ? "yes" : "no");

    if(!_publishAuthData)
    {
        _clearAuthData = true;
    }
}

const uint8_t NukiWrapper::restartController() const
{
    return _restartController;
}

const bool NukiWrapper::hasConnected() const
{
    return _hasConnected;
}

bool NukiWrapper::checkPaired()
{
    if (_paired) return true;

    Log->println("Nuki lock start pairing");
    _preferences->getBool(preference_register_as_app) ? Log->println("Pairing as app") : Log->println("Pairing as bridge");
    _network->publishBleAddress("");

    Nuki::AuthorizationIdType idType = _preferences->getBool(preference_register_as_app) ?
                                       Nuki::AuthorizationIdType::App :
                                       Nuki::AuthorizationIdType::Bridge;

    if(_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success)
    {
        Log->println("Nuki paired");
        _network->publishBleAddress(_nukiLock.getBleAddress().toString());
        return true;
    }

    espDelayAck(200);
    return false;
}

void NukiWrapper::checkRestartByBeacon(const int64_t& ts)
{
    int64_t lastReceivedBeaconTs = _nukiLock.getLastReceivedBeaconTs();

    if(_restartBeaconTimeout > 0 &&
        ts > 60000 &&
        lastReceivedBeaconTs > 0 &&
        _disableBleWatchdogTs < ts &&
        (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
    {
        Log->print("No BLE beacon received from the lock for ");
        Log->print((ts - lastReceivedBeaconTs) / 1000);
        Log->println(" seconds, signalling to restart BLE controller.");
        espDelayAck(200);
        _restartController = 2;
    }
}

void NukiWrapper::checkLockAction(const int64_t& ts)
{
    if(_nukiOfficial->getOffCommandExecutedTs() > 0 && ts >= _nukiOfficial->getOffCommandExecutedTs())
    {
        _nextLockAction = _offCommand;
        _nukiOfficial->clearOffCommandExecutedTs();
    }
    if(_nextLockAction != (NukiLock::LockAction)0xff)
    {
        int retryCount = 0;

        Nuki::CmdResult result = _nukiRetryHandler->retryComm([&]()
        {
             Nuki::CmdResult cmdResult;
             cmdResult = _nukiLock.lockAction(_nextLockAction, 0, 0);
             char resultStr[15] = {0};
             NukiLock::cmdResultToString(cmdResult, resultStr);
             _network->publishCommandResult(resultStr);

             Log->print("Lock action result: ");
             Log->println(resultStr);

             if(cmdResult != Nuki::CmdResult::Success)
             {
                 _network->publishRetry(std::to_string(retryCount + 1));
                 ++retryCount;
             }
             postponeBleWatchdog();

            return cmdResult;
        });

        if(result == Nuki::CmdResult::Success)
        {
            _nextLockAction = (NukiLock::LockAction) 0xff;
            _network->publishRetry("--");
            retryCount = 0;
            if(!_nukiOfficial->getOffConnected())
            {
                _statusUpdated = true;
            }
            Log->println("Lock: updating status after action");
            _statusUpdatedTs = ts;
            if(_intervalLockstate > 10)
            {
                _nextLockStateUpdateTs = ts + 10 * 1000;
            }
        }
        else
        {
            Log->println("Lock: Maximum number of retries exceeded, aborting.");
            _network->publishRetry("failed");
            retryCount = 0;
            _nextLockAction = (NukiLock::LockAction) 0xff;
        }
    }
}

void NukiWrapper::checkDoorSensorOverride()
{
    _requestDoorSensorOverride = _requestDoorSensorOverride == DoorSensorOverride::NoOverride ? _network->getRequestDoorSensorOverride() : _requestDoorSensorOverride;
    if (_requestDoorSensorOverride != DoorSensorOverride::NoOverride)
    {
        Log->print("Door sensor override requested: ");
        Log->print(_requestDoorSensorOverride == DoorSensorOverride::DoorOpen ? "open" : "closed");
        Log->print(" ... ");

        Nuki::CmdResult r = _nukiLock.setDoorSensorState(_requestDoorSensorOverride == DoorSensorOverride::DoorOpen);
        Log->println(r == Nuki::CmdResult::Success ? "success" : "failed");
        _network->publishOverrideDoorSensorOverrideResult(r == Nuki::CmdResult::Success ? "success" : "failed");
        _requestDoorSensorOverride = DoorSensorOverride::NoOverride;
    }
}

void NukiWrapper::checkLockStateUpdate(const int64_t& ts, const uint8_t& queryCommands)
{
    if(_nukiOfficial->getStatusUpdated())
    {
        _statusUpdated = true;
    }

    if(_requestedUpdates != 0)
    {
        uint8_t requested = _requestedUpdates;
        _requestedUpdates = 0;
        if((requested & UPDATE_REQUEST_LOCKSTATE) > 0)
        {
            _nextLockStateUpdateTs = 0;
        }
        if((requested & UPDATE_REQUEST_CONFIG) > 0)
        {
            _nextConfigUpdateTs = 0;
        }
        if((requested & UPDATE_REQUEST_KEYPAD) > 0)
        {
            _nextKeypadUpdateTs = 0;
        }
        if((requested & UPDATE_REQUEST_TIMECONTROL) > 0)
        {
            updateTimeControl(false);
        }
        if((requested & UPDATE_REQUEST_AUTH) > 0)
        {
            updateAuth(false);
        }
    }
    if((_statusUpdated && ts >= _nextStatusPollTs) || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        Log->println("Updating Lock state based on status, timer or query");
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        _statusUpdated = updateKeyTurnerState();
        _network->publishStatusUpdated(_statusUpdated);

        if(_statusUpdated)
        {
            _nextStatusPollTs = ts + 4000;
        }
    }
}

void NukiWrapper::checkQueries(const int64_t& ts, const uint8_t& queryCommands)
{
    if(_statusUpdated) return;

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
        if(_isDebugging)
        {
            //updateDebug();
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
    if(_waitAuthUpdateTs != 0 && ts > _waitAuthUpdateTs)
    {
        _waitAuthUpdateTs = 0;
        updateAuth(true);
    }
    if(_hassEnabled && _nukiConfigValid && _nukiAdvancedConfigValid && !_hassSetupCompleted)
    {
        _network->setupHASS(1, _nukiConfig.nukiId, (char*)_nukiConfig.name, _firmwareVersion.c_str(), _hardwareVersion.c_str(), hasDoorSensor(), hasKeypad());
        _hassSetupCompleted = true;
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
    if(hasKeypad() && _keypadEnabled && (_nextKeypadUpdateTs == 0 || ts > _nextKeypadUpdateTs || (queryCommands & QUERY_COMMAND_KEYPAD) > 0))
    {
        Log->println("Updating Lock keypad based on timer or query");
        _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
        updateKeypad(false);
    }
    if(_preferences->getBool(preference_update_time, false) && ts > (120 * 1000) && ts > _nextTimeUpdateTs)
    {
        _nextTimeUpdateTs = ts + (updateTime() ? (12 * 60 * 60 * 1000) : 60000);
    }
}

void NukiWrapper::update(bool reboot)
{
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);

    _paired = checkPaired();
    if (!_paired) return;

    int64_t ts = espMillis();
    uint8_t queryCommands = _network->queryCommands();

    checkRestartByBeacon(ts);
    _nukiLock.updateConnectionState();
    checkLockAction(ts);
    processCommandQueue();
    if(_keypadCacheDirty && ts > _nextCachePublishTs)
    {
        _keypadCacheDirty = false;
        _nextCachePublishTs = ts + 2000;
        publishKeypadCache();
    }
    checkDoorSensorOverride();
    checkLockStateUpdate(ts, queryCommands);

    if(_network->mqttConnectionState() == 2)
    {
        checkQueries(ts, queryCommands);
        if(_clearAuthData)
        {
            Log->println("Clearing Lock auth data");
            _network->clearAuthorizationInfo();
            _clearAuthData = false;
        }
        if(_checkKeypadCodes && _invalidCount > 0 && (ts - (120000 * _invalidCount)) > _lastCodeCheck)
        {
            _invalidCount--;
        }
        if(reboot && isPinValid())
        {
            Nuki::CmdResult cmdResult = _nukiLock.requestReboot();
            Log->print("Lock reboot was ");
            Log->println(cmdResult == Nuki::CmdResult::Success ? "successful" : "unsuccessful");
        }
    }

    checkGpioAction();

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

const bool NukiWrapper::isPinValid()
{
    return _preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured) == (int)NukiPinState::Valid;
}

void NukiWrapper::setPin(const uint16_t pin)
{
    _nukiLock.saveSecurityPincode(pin);
}

void NukiWrapper::setUltraPin(const uint32_t pin)
{
    _nukiLock.saveUltraPincode(pin);
}

const uint16_t NukiWrapper::getPin()
{
    return _nukiLock.getSecurityPincode();
}

const uint32_t NukiWrapper::getUltraPin()
{
    return _nukiLock.getUltraPincode();
}

void NukiWrapper::unpair()
{
    _nukiLock.unPairNuki();
    _preferences->remove(preference_lock_log_num);    
    Preferences nukiBlePref;
    nukiBlePref.begin("NukiHub", false);
    nukiBlePref.clear();
    nukiBlePref.end();
    _deviceId->assignNewId();
    if (!_forceId)
    {
        _preferences->remove(preference_nuki_id_lock);
    }
    _paired = false;
}

bool NukiWrapper::updateKeyTurnerState()
{
    bool updateStatus = false;
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    Log->println("Querying lock state");

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiLock.requestKeyTurnerState(&_keyTurnerState);
    });

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
            Log->print("Query lock state retrying in ");
            Log->print(_retryDelay);
            Log->println("ms");
            _nextLockStateUpdateTs = espMillis() + _retryDelay;
        }
        else
        {
            _nextLockStateUpdateTs = espMillis() + (_retryLockstateCount * 333);
            _network->publishAvailability(false);
        }
        if(_hasConnected)
        {
            _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);
        }
        return false;
    }

    _hasConnected = true;

    _retryLockstateCount = 0;
    _network->publishAvailability(true);

    const NukiLock::LockState& lockState = _keyTurnerState.lockState;

    if(lockState != _lastKeyTurnerState.lockState)
    {
        _statusUpdatedTs = espMillis();
    }

    if(lockState == NukiLock::LockState::Locked ||
            lockState == NukiLock::LockState::Unlocked ||
            lockState == NukiLock::LockState::Calibration ||
            lockState == NukiLock::LockState::BootRun ||
            lockState == NukiLock::LockState::MotorBlocked)
    {
        if(_publishAuthData && (lockState == NukiLock::LockState::Locked || lockState == NukiLock::LockState::Unlocked))
        {
            Log->println("Publishing auth data");
            updateAuthData(false);
            Log->println("Done publishing auth data");
        }

        updateGpioOutputs();
    }
    else if(!_nukiOfficial->getOffConnected() && espMillis() < _statusUpdatedTs + 10000)
    {
        updateStatus = true;
        Log->println("Lock: Keep updating status on intermediate lock state");
    }
    else if(lockState == NukiLock::LockState::Undefined)
    {
        if (_nextLockStateUpdateTs > espMillis() + 60000)
        {
            _nextLockStateUpdateTs = espMillis() + 60000;
        }
    }
    _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);
    _gpio->setPinOutput(_pinsHighWhenDoorOpen, _keyTurnerState.doorSensorState == NukiLock::DoorSensorState::DoorOpened ? HIGH : LOW);
    _gpio->setPinOutput(_pinsHighWhenDoorClosed, _keyTurnerState.doorSensorState == NukiLock::DoorSensorState::DoorClosed ? HIGH : LOW);

    char lockStateStr[20];
    lockstateToString(lockState, lockStateStr);
    Log->print("Lock state: ");
    Log->println(lockStateStr);

    postponeBleWatchdog();
    Log->println("Done querying lock state");
    return updateStatus;
}

void NukiWrapper::updateBatteryState()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    Log->println("Querying lock battery state");

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiLock.requestBatteryReport(&_batteryReport);
    });

    if(result == Nuki::CmdResult::Success)
    {
        _network->publishBatteryReport(_batteryReport);
    }
    postponeBleWatchdog();
    Log->println("Done querying lock battery state");
}

void NukiWrapper::updateConfig()
{
    bool expectedConfig = true;

    readConfig();

    if(_nukiConfigValid)
    {
        if(!_forceId && (_preferences->getUInt(preference_nuki_id_lock, 0) == 0  || _retryConfigCount == 10))
        {
            char uidString[20];
            itoa(_nukiConfig.nukiId, uidString, 16);
            Log->print("Saving Lock Nuki ID to preferences (");
            Log->print(_nukiConfig.nukiId);
            Log->print(" / ");
            Log->print(uidString);
            Log->println(")");
            _preferences->putUInt(preference_nuki_id_lock, _nukiConfig.nukiId);
        }

        if(_preferences->getUInt(preference_nuki_id_lock, 0) == _nukiConfig.nukiId)
        {
            _hasKeypad = _nukiConfig.hasKeypad == 1 || _nukiConfig.hasKeypadV2 == 1;
            _firmwareVersion = std::to_string(_nukiConfig.firmwareVersion[0]) + "." + std::to_string(_nukiConfig.firmwareVersion[1]) + "." + std::to_string(_nukiConfig.firmwareVersion[2]);
            _hardwareVersion = std::to_string(_nukiConfig.hardwareRevision[0]) + "." + std::to_string(_nukiConfig.hardwareRevision[1]);
            if(_preferences->getBool(preference_conf_info_enabled, true))
            {
                _network->publishConfig(_nukiConfig);
            }
            if(_preferences->getBool(preference_timecontrol_info_enabled))
            {
                updateTimeControl(false);
            }
            if(_preferences->getBool(preference_auth_info_enabled))
            {
                updateAuth(false);
            }

            const int pinStatus = _preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured);

            Nuki::CmdResult result = (Nuki::CmdResult)-1;

            result = _nukiRetryHandler->retryComm([&]()
            {
                return _nukiLock.verifySecurityPin();
            });

            if(result != Nuki::CmdResult::Success)
            {
                Log->println("Nuki Lock PIN is invalid or not set");
                if(result == Nuki::CmdResult::Failed && pinStatus != 2)
                {
                    _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::Invalid);
                }
            }
            else
            {
                Log->println("Nuki Lock PIN is valid");
                if(pinStatus != 1)
                {
                    _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::Valid);
                }
            }
        }
        else
        {
            Log->println("Invalid/Unexpected lock config received, ID does not matched saved ID");
            expectedConfig = false;
        }
    }
    else
    {
        Log->println("Invalid/Unexpected lock config received, Config is not valid");
        expectedConfig = false;
    }

    if(expectedConfig)
    {
        readAdvancedConfig();

        if(_nukiAdvancedConfigValid)
        {
            if(_preferences->getBool(preference_conf_info_enabled, true))
            {
                _network->publishAdvancedConfig(_nukiAdvancedConfig);
            }
        }
        else
        {
            Log->println("Invalid/Unexpected lock advanced config received, Advanced config is not valid");
            expectedConfig = false;
        }
    }

    if(expectedConfig && _nukiConfigValid && _nukiAdvancedConfigValid)
    {
        _retryConfigCount = 0;
        Log->println("Done retrieving lock config and advanced config");
    }
    else
    {
        ++_retryConfigCount;
        Log->println("Invalid/Unexpected lock config and/or advanced config received, retrying in 10 seconds");
        int64_t ts = espMillis();
        _nextConfigUpdateTs = ts + 10000;
    }
}

void NukiWrapper::updateDebug()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int count = 0;

    Log->println("Running debug command - RequestGeneralStatistics");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.genericCommand(Nuki::Command::RequestGeneralStatistics);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");
    Log->println("Running debug command - RequestDailyStatistics");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.requestDailyStatistics();
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");
    Log->println("Running debug command - RequestMqttConfig");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.genericCommand(Nuki::Command::RequestMqttConfig);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");
    Log->println("Running debug command - RequestMqttConfigForMigration");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.genericCommand(Nuki::Command::RequestMqttConfigForMigration);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");
    Log->println("Running debug command - ReadWifiConfig");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.genericCommand(Nuki::Command::ReadWifiConfig);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");
    Log->println("Running debug command - ReadWifiConfigForMigration");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.genericCommand(Nuki::Command::ReadWifiConfigForMigration);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");
    Log->println("Running debug command - GetKeypad2Config");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.genericCommand(Nuki::Command::GetKeypad2Config, false);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");
    Log->println("Running debug command - RequestFingerprintEntries");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.retrieveFingerprintEntries();
    Log->print("Result: ");
    Log->println(result);
    count = 0;
    while (count < 5)
    {
        espDelayAck(1000);
        count++;
    }

    std::list<NukiLock::FingerprintEntry> fingerprintEntries;
    _nukiLock.getFingerprintEntries(&fingerprintEntries);

    Log->print("Fingerprint entries: ");
    Log->println(fingerprintEntries.size());
    Log->println("Debug command complete");

    Log->println("Running debug command - RequestInternalLogEntries");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.retrieveInternalLogEntries(0, 10, 1, false);
    Log->print("Result: ");
    Log->println(result);

    espDelayAck(15000);

    std::list<NukiLock::InternalLogEntry> internalLogEntries;
    _nukiLock.getInternalLogEntries(&internalLogEntries);

    Log->print("InternalLog entries: ");
    Log->println(internalLogEntries.size());
    Log->println("Debug command complete");

    Log->println("Running debug command - scanWifi");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.scanWifi(15);
    Log->print("Result: ");
    Log->println(result);

    espDelayAck(20000);

    std::list<NukiLock::WifiScanEntry> wifiScanEntries;
    _nukiLock.getWifiScanEntries(&wifiScanEntries);

    Log->print("WifiScan entries: ");
    Log->println(wifiScanEntries.size());
    Log->println("Debug command complete");

    Log->println("Running debug command - getAccessoryInfo, type = 4");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.getAccessoryInfo(4);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");

    Log->println("Running debug command - getAccessoryInfo, type = 5");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.getAccessoryInfo(5);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");

    Log->println("Running debug command - RequestDoorSensorConfig");
    result = (Nuki::CmdResult)-1;
    result = _nukiLock.genericCommand(Nuki::Command::RequestDoorSensorConfig, false);
    Log->print("Result: ");
    Log->println(result);
    Log->println("Debug command complete");

    /*
    CheckKeypadCode             = 0x006E  keypadCode (int), (nonce, PIN)

    RequestMatterPairings       = 0x0112  requestTotalCount (bool), (nonce, PIN)
    MatterPairing               = 0x0113
    MatterPairingCount          = 0x0114

    ConnectWifi                 = 0x0082
    SetWifiConfig               = 0x0087
    SetMqttConfig               = 0x008D
    SetKeypad2Config            = 0x009C
    EnableMatterCommissioning   = 0x0110
    SetMatterState              = 0x0111
    */
}

void NukiWrapper::updateAuthData(bool retrieved)
{
    if(!isPinValid())
    {
        Log->println("No valid Nuki Lock PIN set");
        return;
    }

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        result = _nukiRetryHandler->retryComm([&]()
        {
            return _nukiLock.retrieveLogEntries(0, _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 1, false);
        });

        Log->print("Retrieving log entries: ");
        NukiHelper::printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitAuthLogUpdateTs = espMillis() + 5000;
            waitForLogEntries(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));

            std::list<NukiLock::LogEntry> log;
            _nukiLock.getLogEntries(&log);

            if(log.size() > _preferences->getInt(preference_authlog_max_entries, 3))
            {
                log.resize(_preferences->getInt(preference_authlog_max_entries, 3));
            }

            log.sort([](const NukiLock::LogEntry& a, const NukiLock::LogEntry& b)
            {
                return a.index < b.index;
            });

            if(log.size() > 0)
            {
                if(retrieveMissingLogEntries(log))
                {
                    _waitAuthLogUpdateTs = 0;
                    _network->publishAuthorizationInfo(log, false);
                }
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

        log.sort([](const NukiLock::LogEntry& a, const NukiLock::LogEntry& b)
        {
            return a.index < b.index;
        });

        Log->print("Log size: ");
        Log->println(log.size());

        if(log.size() > 0)
        {
            _network->publishAuthorizationInfo(log, false);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::waitForLogEntries(uint32_t count)
{
    std::list<NukiLock::LogEntry> log;
    size_t lastSize = 0;
    int64_t startTs = espMillis();
    int64_t changedTs = startTs;

    while(espMillis() - startTs < 5000)
    {
        espDelayAck(100);
        _nukiLock.getLogEntries(&log);

        if(log.size() >= count)
        {
            break;
        }

        if(log.size() != lastSize)
        {
            lastSize = log.size();
            changedTs = espMillis();
        }
        else if(espMillis() - changedTs > 500)
        {
            break;
        }
    }
}

bool NukiWrapper::retrieveMissingLogEntries(std::list<NukiLock::LogEntry>& log)
{
    uint32_t lastRollingLog = _network->getLastRollingLog();

    if(log.size() == 0 || lastRollingLog == 0 || log.front().index <= lastRollingLog + 1)
    {
        return false;
    }

    uint32_t count = log.front().index - lastRollingLog - 1;
    uint32_t maxEntries = _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG);

    if(count > maxEntries)
    {
        count = maxEntries;
    }

    uint32_t startIndex = log.front().index - count;
    Log->print("Retrieving missing log entries: ");

    Nuki::CmdResult result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiLock.retrieveLogEntries(startIndex, count, 0, false);
    });

    NukiHelper::printCommandResult(result);

    if(result == Nuki::CmdResult::Success)
    {
        waitForLogEntries(count);

        std::list<NukiLock::LogEntry> missing;
        _nukiLock.getLogEntries(&missing);
        Log->print("Missing log entries received: ");
        Log->println(missing.size());
        log.splice(log.begin(), missing);
        log.sort([](const NukiLock::LogEntry& a, const NukiLock::LogEntry& b)
        {
            return a.index < b.index;
        });
    }

    return true;
}

void NukiWrapper::updateKeypad(bool retrieved)
{
    if(!_preferences->getBool(preference_keypad_info_enabled))
    {
        return;
    }

    if(!isPinValid())
    {
        Log->println("No valid Nuki Lock PIN set");
        return;
    }

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        result = _nukiRetryHandler->retryComm([&]()
        {
            return _nukiLock.retrieveKeypadEntries(0, _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
        });
        Log->print("Querying lock keypad: ");
        NukiHelper::printCommandResult(result);

        if(result == Nuki::CmdResult::Success && _waitKeypadUpdateTs == 0)
        {
            _waitKeypadUpdateTs = espMillis() + 5000;
        }
    }
    else
    {
        std::list<NukiLock::KeypadEntry> entries;
        _nukiLock.getKeypadEntries(&entries);
        {
            int64_t nowTs = espMillis();
            auto d = _keypadDeleted.begin();
            while(d != _keypadDeleted.end())
            {
                if(nowTs - d->second > 300000)
                {
                    d = _keypadDeleted.erase(d);
                }
                else
                {
                    ++d;
                }
            }
            auto e = entries.begin();
            while(e != entries.end())
            {
                bool deleted = false;
                for(const auto& del : _keypadDeleted)
                {
                    if(del.first == e->codeId)
                    {
                        deleted = true;
                        break;
                    }
                }
                if(deleted)
                {
                    e = entries.erase(e);
                }
                else
                {
                    ++e;
                }
            }
        }

        Log->print("Lock keypad codes: ");
        Log->println(entries.size());

        entries.sort([](const NukiLock::KeypadEntry& a, const NukiLock::KeypadEntry& b)
        {
            return a.codeId < b.codeId;
        });

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
        _keypadEntriesCache = entries;

        _keypadCodeIds.clear();
        _keypadCodes.clear();
        _keypadCodeIds.reserve(entries.size());
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
    if(!_preferences->getBool(preference_timecontrol_info_enabled))
    {
        return;
    }

    if(!isPinValid())
    {
        Log->println("No valid Nuki Lock PIN set");
        return;
    }

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;

        result = _nukiRetryHandler->retryComm([&]()
        {
            return _nukiLock.retrieveTimeControlEntries();
        });
        Log->print("Querying lock timecontrol: ");
        NukiHelper::printCommandResult(result);

        if(result == Nuki::CmdResult::Success)
        {
            _waitTimeControlUpdateTs = espMillis() + 5000;
        }
    }
    else
    {
        std::list<NukiLock::TimeControlEntry> timeControlEntries;
        _nukiLock.getTimeControlEntries(&timeControlEntries);

        Log->print("Lock timecontrol entries: ");
        Log->println(timeControlEntries.size());

        timeControlEntries.sort([](const NukiLock::TimeControlEntry& a, const NukiLock::TimeControlEntry& b)
        {
            return a.entryId < b.entryId;
        });

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
        _timeControlEntriesCache = timeControlEntries;

        _timeControlIds.clear();
        _timeControlIds.reserve(timeControlEntries.size());
        for(const auto& entry : timeControlEntries)
        {
            _timeControlIds.push_back(entry.entryId);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::updateAuth(bool retrieved)
{
    if(!isPinValid())
    {
        Log->println("No valid Nuki Lock PIN set");
        return;
    }

    if(!_preferences->getBool(preference_auth_info_enabled))
    {
        return;
    }

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;

        result = _nukiRetryHandler->retryComm([&]()
        {
            return _nukiLock.retrieveAuthorizationEntries(0, _preferences->getInt(preference_auth_max_entries, MAX_AUTH));
        });
        Log->print("Querying lock authorization: ");
        NukiHelper::printCommandResult(result);

        NukiHelper::printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitAuthUpdateTs = espMillis() + 5000;
        }
    }
    else
    {
        std::list<NukiLock::AuthorizationEntry> authEntries;
        _nukiLock.getAuthorizationEntries(&authEntries);

        Log->print("Lock authorization entries: ");
        Log->println(authEntries.size());

        authEntries.sort([](const NukiLock::AuthorizationEntry& a, const NukiLock::AuthorizationEntry& b)
        {
            return a.authId < b.authId;
        });

        if(authEntries.size() > _preferences->getInt(preference_auth_max_entries, MAX_AUTH))
        {
            authEntries.resize(_preferences->getInt(preference_auth_max_entries, MAX_AUTH));
        }

        uint authCount = authEntries.size();
        if(authCount > _maxAuthEntryCount)
        {
            _maxAuthEntryCount = authCount;
            _preferences->putUInt(preference_lock_max_auth_entry_count, _maxAuthEntryCount);
        }

        _network->publishAuth(authEntries, _maxAuthEntryCount);
        _authEntriesCache = authEntries;

        _authIds.clear();
        _authIds.reserve(authEntries.size());
        for(const auto& entry : authEntries)
        {
            _authIds.push_back(entry.authId);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::requestUpdate(uint8_t updates)
{
    _requestedUpdates |= updates;
}

const std::list<NukiLock::KeypadEntry>& NukiWrapper::keypadEntries() const
{
    return _keypadEntriesCache;
}

const std::list<NukiLock::TimeControlEntry>& NukiWrapper::timeControlEntries() const
{
    return _timeControlEntriesCache;
}

const std::list<NukiLock::AuthorizationEntry>& NukiWrapper::authEntries() const
{
    return _authEntriesCache;
}

void NukiWrapper::postponeBleWatchdog()
{
    _disableBleWatchdogTs = espMillis() + 15000;
}

LockActionResult NukiWrapper::onLockActionReceivedCallback(const char *value)
{
    return nukiInst->onLockActionReceived(value);
}

LockActionResult NukiWrapper::onLockActionReceived(const char *value)
{
    NukiLock::LockAction action;

    if(value)
    {
        if(strlen(value) > 0)
        {
            action = NukiHelper::lockActionToEnum(value);
            if((int)action == 0xff)
            {
                return LockActionResult::UnknownAction;
            }
        }
        else
        {
            return LockActionResult::UnknownAction;
        }
    }
    else
    {
        return LockActionResult::UnknownAction;
    }

    const uint32_t* aclPrefs = nukiInst->_aclPrefs;

    if((action == NukiLock::LockAction::Lock && (int)aclPrefs[0] == 1) || (action == NukiLock::LockAction::Unlock && (int)aclPrefs[1] == 1) || (action == NukiLock::LockAction::Unlatch && (int)aclPrefs[2] == 1) || (action == NukiLock::LockAction::LockNgo && (int)aclPrefs[3] == 1) || (action == NukiLock::LockAction::LockNgoUnlatch && (int)aclPrefs[4] == 1) || (action == NukiLock::LockAction::FullLock && (int)aclPrefs[5] == 1) || (action == NukiLock::LockAction::FobAction1 && (int)aclPrefs[6] == 1) || (action == NukiLock::LockAction::FobAction2 && (int)aclPrefs[7] == 1) || (action == NukiLock::LockAction::FobAction3 && (int)aclPrefs[8] == 1))
    {
        if(!_nukiOfficial->getOffConnected())
        {
            nukiInst->_nextLockAction = action;
        }
        else
        {
            if(_preferences->getBool(preference_official_hybrid_actions, false))
            {
                if(_preferences->getBool(preference_official_hybrid_retry, false))
                {
                    _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
                    _offCommand = action;
                }
                _network->publishOffAction((int)action);
            }
            else
            {
                nukiInst->_nextLockAction = action;
            }
        }
        return LockActionResult::Success;
    }

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

const bool NukiWrapper::offConnected()
{
    return _nukiOfficial->getOffConnected();
}

void NukiWrapper::onOfficialUpdateReceived(const char *topic, const char *value)
{
    _nukiOfficial->onOfficialUpdateReceived(topic, value);
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

void NukiWrapper::onAuthCommandReceivedCallback(const char *value)
{
    nukiInst->onAuthCommandReceived(value);
}

void NukiWrapper::gpioActionCallback(const GpioAction &action, const int& pin)
{
    nukiInst->onGpioActionReceived(action);
}

void NukiWrapper::onGpioActionReceived(const GpioAction &action)
{
    gpioAction = action;
}

void NukiWrapper::checkGpioAction()
{
    switch(gpioAction)
    {
    case GpioAction::Lock:
        if(!_nukiOfficial->getOffConnected())
        {
            nukiInst->lock();
        }
        else
        {
            if(_preferences->getBool(preference_official_hybrid_retry, false))
            {
                _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
                _offCommand = NukiLock::LockAction::Lock;
            }
            _network->publishOffAction(2);
        }
        break;
    case GpioAction::Unlock:
        if(!_nukiOfficial->getOffConnected())
        {
            nukiInst->unlock();
        }
        else
        {
            if(_preferences->getBool(preference_official_hybrid_retry, false))
            {
                _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
                _offCommand = NukiLock::LockAction::Unlock;
            }
            _network->publishOffAction(1);
        }
        break;
    case GpioAction::Unlatch:
        if(!_nukiOfficial->getOffConnected())
        {
            nukiInst->unlatch();
        }
        else
        {
            if(_preferences->getBool(preference_official_hybrid_retry, false))
            {
                _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
                _offCommand = NukiLock::LockAction::Unlatch;
            }
            _network->publishOffAction(3);
        }
        break;
    case GpioAction::LockNgo:
        if(!_nukiOfficial->getOffConnected())
        {
            nukiInst->lockngo();
        }
        else
        {
            if(_preferences->getBool(preference_official_hybrid_retry, false))
            {
                _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
                _offCommand = NukiLock::LockAction::LockNgo;
            }
            _network->publishOffAction(4);
        }
        break;
    case GpioAction::LockNgoUnlatch:
        if(!_nukiOfficial->getOffConnected())
        {
            nukiInst->lockngounlatch();
        }
        else
        {
            if(_preferences->getBool(preference_official_hybrid_retry, false))
            {
                _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
                _offCommand = NukiLock::LockAction::LockNgoUnlatch;
            }
            _network->publishOffAction(5);
        }
        break;
    case GpioAction::DoorSensorOpen:
        _requestDoorSensorOverride = DoorSensorOverride::DoorOpen;
        break;
    case GpioAction::DoorSensorClosed:
        _requestDoorSensorOverride = DoorSensorOverride::DoorClosed;
        break;
    }

    gpioAction = GpioAction::None;
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
    return (_forceKeypad || _hasKeypad);
}

void NukiWrapper::notify(Nuki::EventType eventType)
{
    if(!_nukiOfficial->getOffConnected())
    {
        if(_nukiOfficial->getOffEnabled() && _intervalHybridLockstate > 0 && espMillis() > (_intervalHybridLockstate * 1000))
        {
            Log->println("OffKeyTurnerStatusUpdated");
            _statusUpdated = true;
        }
        else
        {
            if(eventType == Nuki::EventType::KeyTurnerStatusReset)
            {
                _newSignal = 0;
                Log->println("KeyTurnerStatusReset");
            }
            else if(eventType == Nuki::EventType::KeyTurnerStatusUpdated)
            {
                if(!_statusUpdated && _newSignal < 5)
                {
                    _newSignal++;
                    Log->println("KeyTurnerStatusUpdated");
                    _statusUpdated = true;
                    _statusUpdatedTs = espMillis();
                    _network->publishStatusUpdated(_statusUpdated);
                }
            }
            else if(eventType == Nuki::EventType::ERROR_BAD_PIN)
            {
                _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::Invalid);
            }
            else if(eventType == Nuki::EventType::BLE_ERROR_ON_DISCONNECT)
            {
                Log->println("Error in disconnecting BLE client, signalling to restart BLE controller");
                _restartController = 1;
            }
        }
    }
}

void NukiWrapper::readConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiLock.requestConfig(&_nukiConfig);
    });

    _nukiConfigValid = result == Nuki::CmdResult::Success;
}

void NukiWrapper::readAdvancedConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
    });
    _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;
}

bool NukiWrapper::hasDoorSensor() const
{
    return (_forceDoorsensor ||
            _keyTurnerState.doorSensorState == Nuki::DoorSensorState::DoorClosed ||
            _keyTurnerState.doorSensorState == Nuki::DoorSensorState::DoorOpened ||
            _keyTurnerState.doorSensorState == Nuki::DoorSensorState::Calibrating);
}

const BLEAddress NukiWrapper::getBleAddress() const
{
    return _nukiLock.getBleAddress();
}

const std::string NukiWrapper::firmwareVersion() const
{
    return _firmwareVersion;
}

NukiLock::NukiLock& NukiWrapper::nukiDevice()
{
    return _nukiLock;
}

const std::string NukiWrapper::name() const
{
    return std::string((const char*)_nukiConfig.name);
}

const std::string NukiWrapper::hardwareVersion() const
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

bool NukiWrapper::updateTime()
{
    if(!isPinValid())
    {
        Log->println("No valid PIN set");
        return false;
    }

    time_t now;
    tm tm;
    time(&now);
    localtime_r(&now, &tm);

    if (int(tm.tm_year + 1900) < int(2025))
    {
        Log->println("NTP Time not valid, not updating Nuki device");
        return false;
    }

    Nuki::TimeValue nukiTime;
    nukiTime.year = tm.tm_year + 1900;
    nukiTime.month = tm.tm_mon + 1;
    nukiTime.day = tm.tm_mday;
    nukiTime.hour = tm.tm_hour;
    nukiTime.minute = tm.tm_min;
    nukiTime.second = tm.tm_sec;

    Nuki::CmdResult cmdResult = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiLock.updateTime(nukiTime);
    });
    if (cmdResult== Nuki::CmdResult::Success)
    {
        Log->println("Nuki lock time updated.");
    }
    else
    {
        char resultStr[15] = {0};
        NukiLock::cmdResultToString(cmdResult, resultStr);
        Log->print("Failed to update Nuki lock time. Command result: ");
        Log->println(resultStr);
    }
    return true;
}

void NukiWrapper::publishKeypadCache()
{
    std::list<NukiLock::KeypadEntry> entries = _keypadEntriesCache;
    entries.sort([](const NukiLock::KeypadEntry& a, const NukiLock::KeypadEntry& b)
    {
        return a.codeId < b.codeId;
    });
    _network->publishKeypad(entries, _maxKeypadCodeCount);
    _keypadCodeIds.clear();
    _keypadCodes.clear();
    for(const auto& entry : entries)
    {
        _keypadCodeIds.push_back(entry.codeId);
        _keypadCodes.push_back(entry.code);
    }
}
