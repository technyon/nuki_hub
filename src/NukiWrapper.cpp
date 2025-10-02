#include "NukiWrapper.h"
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

    network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
    network->setOfficialUpdateReceivedCallback(nukiInst->onOfficialUpdateReceivedCallback);
    network->setConfigUpdateReceivedCallback(nukiInst->onConfigUpdateReceivedCallback);
    network->setKeypadCommandReceivedCallback(nukiInst->onKeypadCommandReceivedCallback);
    network->setKeypadJsonCommandReceivedCallback(nukiInst->onKeypadJsonCommandReceivedCallback);
    network->setTimeControlCommandReceivedCallback(nukiInst->onTimeControlCommandReceivedCallback);
    network->setAuthCommandReceivedCallback(nukiInst->onAuthCommandReceivedCallback);

    _gpio->addCallback(NukiWrapper::gpioActionCallback);
#ifndef NUKI_HUB_UPDATER
    _pinsCommError = _gpio->getPinsWithRole(PinRole::OutputHighBluetoothCommError);
#endif
}


NukiWrapper::~NukiWrapper()
{
    _bleScanner = nullptr;
}


void NukiWrapper::initialize()
{
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
    _nukiLock.setConnectTimeout(2);
    _nukiLock.setDisconnectTimeout(2000);

    _hassEnabled = _preferences->getBool(preference_mqtt_hass_enabled, false);
    readSettings();
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
        powerLevel = ESP_PWR_LVL_P6;
    }
    else if(pwrLvl >= 0)
    {
        powerLevel = ESP_PWR_LVL_P3;
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
    else if(pwrLvl >= -12)
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

void NukiWrapper::update(bool reboot)
{
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    if(!_paired)
    {
        Log->println("Nuki lock start pairing");
        _preferences->getBool(preference_register_as_app) ? Log->println("Pairing as app") : Log->println("Pairing as bridge");
        _network->publishBleAddress("");

        Nuki::AuthorizationIdType idType = _preferences->getBool(preference_register_as_app) ?
                                           Nuki::AuthorizationIdType::App :
                                           Nuki::AuthorizationIdType::Bridge;

        if(_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success)
        {
            Log->println("Nuki paired");
            _paired = true;
            _network->publishBleAddress(_nukiLock.getBleAddress().toString());
        }
        else
        {
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(200 / portTICK_PERIOD_MS);
            return;
        }
    }

    int64_t lastReceivedBeaconTs = _nukiLock.getLastReceivedBeaconTs();
    int64_t ts = espMillis();
    uint8_t queryCommands = _network->queryCommands();

    if(_restartBeaconTimeout > 0 &&
            ts > 60000 &&
            lastReceivedBeaconTs > 0 &&
            _disableBleWatchdogTs < ts &&
            (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
    {
        Log->print("No BLE beacon received from the lock for ");
        Log->print((ts - lastReceivedBeaconTs) / 1000);
        Log->println(" seconds, signalling to restart BLE controller.");
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
        _restartController = 2;
    }

    _nukiLock.updateConnectionState();

    if(_nukiOfficial->getOffCommandExecutedTs() > 0 && ts >= _nukiOfficial->getOffCommandExecutedTs())
    {
        _nextLockAction = _offCommand;
        _nukiOfficial->clearOffCommandExecutedTs();
    }
    if(_nextLockAction != (NukiLock::LockAction)0xff)
    {
        int retryCount = 0;
        Nuki::CmdResult cmdResult;

        while(retryCount < _nrOfRetries + 1 && cmdResult != Nuki::CmdResult::Success)
        {
            cmdResult = _nukiLock.lockAction(_nextLockAction, 0, 0);
            char resultStr[15] = {0};
            NukiLock::cmdResultToString(cmdResult, resultStr);
            _network->publishCommandResult(resultStr);

            Log->print("Lock action result: ");
            Log->println(resultStr);

            if(cmdResult != Nuki::CmdResult::Success)
            {
                setCommErrorPins(HIGH);
                Log->print("Lock: Last command failed, retrying after ");
                Log->print(_retryDelay);
                Log->print(" milliseconds. Retry ");
                Log->print(retryCount + 1);
                Log->print(" of ");
                Log->println(_nrOfRetries);

                _network->publishRetry(std::to_string(retryCount + 1));

                if (esp_task_wdt_status(NULL) == ESP_OK)
                {
                    esp_task_wdt_reset();
                }
                vTaskDelay(_retryDelay / portTICK_PERIOD_MS);

                ++retryCount;
            }
            postponeBleWatchdog();
        }
        setCommErrorPins(LOW);

        if(cmdResult == Nuki::CmdResult::Success)
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
    if(_nukiOfficial->getStatusUpdated() || _statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        Log->println("Updating Lock state based on status, timer or query");
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        _statusUpdated = updateKeyTurnerState();
        _network->publishStatusUpdated(_statusUpdated);

        if(_statusUpdated)
        {
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    if(_network->mqttConnectionState() == 2)
    {
        if(!_statusUpdated)
        {
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
                _nextTimeUpdateTs = ts + (12 * 60 * 60 * 1000);
                updateTime();
            }
        }
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
        }
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

    while(result != Nuki::CmdResult::Success && retryCount < _nrOfRetries + 1)
    {
        Log->print("Result (attempt ");
        Log->print(retryCount + 1);
        Log->print("): ");
        result =_nukiLock.requestKeyTurnerState(&_keyTurnerState);
        ++retryCount;
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
            Log->print("Query lock state retrying in ");
            Log->print(_retryDelay);
            Log->println("ms");
            _nextLockStateUpdateTs = espMillis() + _retryDelay;
        }
        else
        {
            _nextLockStateUpdateTs = espMillis() + (_retryLockstateCount * 333);
        }
        _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);
        return false;
    }
    else if (!_hasConnected)
    {
        _hasConnected = true;
    }

    _retryLockstateCount = 0;

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

    char lockStateStr[20];
    lockstateToString(lockState, lockStateStr);
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

    while(retryCount < _nrOfRetries + 1)
    {
        Log->print("Result (attempt ");
        Log->print(retryCount + 1);
        Log->print("): ");
        result = _nukiLock.requestBatteryReport(&_batteryReport);

        if(result != Nuki::CmdResult::Success)
        {
            ++retryCount;
        }
        else
        {
            break;
        }
    }

    NukiHelper::printCommandResult(result);
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
            int retryCount = 0;

            while(retryCount < _nrOfRetries + 1)
            {
                result = _nukiLock.verifySecurityPin();
                if(result != Nuki::CmdResult::Success)
                {
                    ++retryCount;
                }
                else
                {
                    break;
                }
            }

            if(result != Nuki::CmdResult::Success)
            {
                Log->println("Nuki Lock PIN is invalid or not set");
                if(pinStatus != 2)
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
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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

    count = 0;
    while (count < 15)
    {
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        count++;
    }

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

    count = 0;
    while (count < 20)
    {
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        count++;
    }

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

        while(retryCount < _nrOfRetries + 1)
        {
            Log->print("Retrieve log entries: ");
            result = _nukiLock.retrieveLogEntries(0, _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 1, false);
            if(result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        NukiHelper::printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitAuthLogUpdateTs = espMillis() + 5000;
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);

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

        while(retryCount < _nrOfRetries + 1)
        {
            Log->print("Querying lock keypad: ");
            result = _nukiLock.retrieveKeypadEntries(0, _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
            if(result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        NukiHelper::printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitKeypadUpdateTs = espMillis() + 5000;
        }
    }
    else
    {
        std::list<NukiLock::KeypadEntry> entries;
        _nukiLock.getKeypadEntries(&entries);

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
        int retryCount = 0;

        while(retryCount < _nrOfRetries + 1)
        {
            Log->print("Querying lock timecontrol: ");
            result = _nukiLock.retrieveTimeControlEntries();
            if(result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

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
        int retryCount = 0;

        while(retryCount < _nrOfRetries)
        {
            Log->print("Querying lock authorization: ");
            result = _nukiLock.retrieveAuthorizationEntries(0, _preferences->getInt(preference_auth_max_entries, MAX_AUTH));
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(250 / portTICK_PERIOD_MS);
            if(result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        NukiHelper::printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitAuthUpdateTs = millis() + 5000;
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

        _authIds.clear();
        _authIds.reserve(authEntries.size());
        for(const auto& entry : authEntries)
        {
            _authIds.push_back(entry.authId);
        }
    }

    postponeBleWatchdog();
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

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

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

void NukiWrapper::onConfigUpdateReceived(const char *value)
{
    JsonDocument jsonResult;

    if(!_nukiConfigValid)
    {
        jsonResult["general"] = "configNotReady";
        serializeJson(jsonResult, _buffer, _bufferSize);
        _network->publishConfigCommandResult(_buffer);
        return;
    }

    if(!isPinValid())
    {
        jsonResult["general"] = "noValidPinSet";
        serializeJson(jsonResult, _buffer, _bufferSize);
        _network->publishConfigCommandResult(_buffer);
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        jsonResult["general"] = "invalidJson";
        serializeJson(jsonResult, _buffer, _bufferSize);
        _network->publishConfigCommandResult(_buffer);
        return;
    }

    Nuki::CmdResult cmdResult;
    const char *basicKeys[16] = {"name", "latitude", "longitude", "autoUnlatch", "pairingEnabled", "buttonEnabled", "ledEnabled", "ledBrightness", "timeZoneOffset", "dstMode", "fobAction1",  "fobAction2", "fobAction3", "singleLock", "advertisingMode", "timeZone"};
    const char *advancedKeys[25] = {"unlockedPositionOffsetDegrees", "lockedPositionOffsetDegrees", "singleLockedPositionOffsetDegrees", "unlockedToLockedTransitionOffsetDegrees", "lockNgoTimeout", "singleButtonPressAction", "doubleButtonPressAction", "detachedCylinder", "batteryType", "automaticBatteryTypeDetection", "unlatchDuration", "autoLockTimeOut",  "autoUnLockDisabled", "nightModeEnabled", "nightModeStartTime", "nightModeEndTime", "nightModeAutoLockEnabled", "nightModeAutoUnlockDisabled", "nightModeImmediateLockOnStart", "autoLockEnabled", "immediateAutoLockEnabled", "autoUpdateEnabled", "rebootNuki", "motorSpeed", "enableSlowSpeedDuringNightMode"};
    bool basicUpdated = false;
    bool advancedUpdated = false;

    for(int i=0; i < 16; i++)
    {
        if(json[basicKeys[i]].is<JsonVariantConst>())
        {
            JsonVariantConst jsonKey = json[basicKeys[i]];
            char *jsonchar;

            if (jsonKey.is<float>())
            {
                itoa(jsonKey, jsonchar, 10);
            }
            else if (jsonKey.is<bool>())
            {
                if (jsonKey)
                {
                    itoa(1, jsonchar, 10);
                }
                else
                {
                    itoa(0, jsonchar, 10);
                }
            }
            else if (jsonKey.is<const char*>())
            {
                jsonchar = (char*)jsonKey.as<const char*>();
            }

            if(strlen(jsonchar) == 0)
            {
                jsonResult[basicKeys[i]] = "noValueSet";
                continue;
            }

            if((int)_basicLockConfigaclPrefs[i] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;
                int retryCount = 0;

                while(retryCount < _nrOfRetries + 1)
                {
                    if(strcmp(basicKeys[i], "name") == 0)
                    {
                        if(strlen(jsonchar) <= 32)
                        {
                            if(strcmp((const char*)_nukiConfig.name, jsonchar) == 0)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setName(std::string(jsonchar));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "valueTooLong";
                        }
                    }
                    else if(strcmp(basicKeys[i], "latitude") == 0)
                    {
                        const float keyvalue = atof(jsonchar);

                        if(keyvalue > 0)
                        {
                            if(_nukiConfig.latitude == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setLatitude(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "longitude") == 0)
                    {
                        const float keyvalue = atof(jsonchar);

                        if(keyvalue > 0)
                        {
                            if(_nukiConfig.longitude == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setLongitude(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "autoUnlatch") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.autoUnlatch == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableAutoUnlatch((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "pairingEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.pairingEnabled == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enablePairing((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "buttonEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.buttonEnabled == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableButton((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "ledEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.ledEnabled == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableLedFlash((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "ledBrightness") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 0 && keyvalue <= 5)
                        {
                            if(_nukiConfig.ledBrightness == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setLedBrightness(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "timeZoneOffset") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 0 && keyvalue <= 60)
                        {
                            if(_nukiConfig.timeZoneOffset == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setTimeZoneOffset(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "dstMode") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.dstMode == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableDst((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "fobAction1") == 0)
                    {
                        const uint8_t fobAct1 = NukiHelper::fobActionToInt(jsonchar);

                        if(fobAct1 != 99)
                        {
                            if(_nukiConfig.fobAction1 == fobAct1)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setFobAction(1, fobAct1);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "fobAction2") == 0)
                    {
                        const uint8_t fobAct2 = NukiHelper::fobActionToInt(jsonchar);

                        if(fobAct2 != 99)
                        {
                            if(_nukiConfig.fobAction2 == fobAct2)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setFobAction(2, fobAct2);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "fobAction3") == 0)
                    {
                        const uint8_t fobAct3 = NukiHelper::fobActionToInt(jsonchar);

                        if(fobAct3 != 99)
                        {
                            if(_nukiConfig.fobAction3 == fobAct3)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setFobAction(3, fobAct3);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "singleLock") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.singleLock == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableSingleLock((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "advertisingMode") == 0)
                    {
                        Nuki::AdvertisingMode advmode = NukiHelper::advertisingModeToEnum(jsonchar);

                        if((int)advmode != 0xff)
                        {
                            if(_nukiConfig.advertisingMode == advmode)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setAdvertisingMode(advmode);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "timeZone") == 0)
                    {
                        Nuki::TimeZoneId tzid = NukiHelper::timeZoneToEnum(jsonchar);

                        if((int)tzid != 0xff)
                        {
                            if(_nukiConfig.timeZoneId == tzid)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setTimeZoneId(tzid);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }

                    if(cmdResult != Nuki::CmdResult::Success)
                    {
                        ++retryCount;
                    }
                    else
                    {
                        break;
                    }
                }

                if(cmdResult == Nuki::CmdResult::Success)
                {
                    basicUpdated = true;
                }

                if(!jsonResult[basicKeys[i]])
                {
                    char resultStr[15] = {0};
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[basicKeys[i]] = resultStr;
                }
            }
            else
            {
                jsonResult[basicKeys[i]] = "accessDenied";
            }
        }
    }

    for(int j=0; j < 25; j++)
    {
        if(json[advancedKeys[j]].is<JsonVariantConst>())
        {
            JsonVariantConst jsonKey = json[advancedKeys[j]];
            char *jsonchar;

            if (jsonKey.is<float>())
            {
                itoa(jsonKey, jsonchar, 10);
            }
            else if (jsonKey.is<bool>())
            {
                if (jsonKey)
                {
                    itoa(1, jsonchar, 10);
                }
                else
                {
                    itoa(0, jsonchar, 10);
                }
            }
            else if (jsonKey.is<const char*>())
            {
                jsonchar = (char*)jsonKey.as<const char*>();
            }

            if(strlen(jsonchar) == 0)
            {
                jsonResult[advancedKeys[j]] = "noValueSet";
                continue;
            }

            if((int)_advancedLockConfigaclPrefs[j] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;
                int retryCount = 0;

                while(retryCount < _nrOfRetries + 1)
                {
                    if(strcmp(advancedKeys[j], "unlockedPositionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -90 && keyvalue <= 180)
                        {
                            if(_nukiAdvancedConfig.unlockedPositionOffsetDegrees == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setUnlockedPositionOffsetDegrees(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "lockedPositionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -180 && keyvalue <= 90)
                        {
                            if(_nukiAdvancedConfig.lockedPositionOffsetDegrees == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setLockedPositionOffsetDegrees(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "singleLockedPositionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -180 && keyvalue <= 180)
                        {
                            if(_nukiAdvancedConfig.singleLockedPositionOffsetDegrees == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setSingleLockedPositionOffsetDegrees(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "unlockedToLockedTransitionOffsetDegrees") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= -180 && keyvalue <= 180)
                        {
                            if(_nukiAdvancedConfig.unlockedToLockedTransitionOffsetDegrees == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setUnlockedToLockedTransitionOffsetDegrees(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "lockNgoTimeout") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 5 && keyvalue <= 60)
                        {
                            if(_nukiAdvancedConfig.lockNgoTimeout == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setLockNgoTimeout(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "singleButtonPressAction") == 0)
                    {
                        NukiLock::ButtonPressAction sbpa = NukiHelper::buttonPressActionToEnum(jsonchar);

                        if((int)sbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.singleButtonPressAction == sbpa)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setSingleButtonPressAction(sbpa);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "doubleButtonPressAction") == 0)
                    {
                        NukiLock::ButtonPressAction dbpa = NukiHelper::buttonPressActionToEnum(jsonchar);

                        if((int)dbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.doubleButtonPressAction == dbpa)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setDoubleButtonPressAction(dbpa);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "detachedCylinder") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.detachedCylinder == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableDetachedCylinder((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "batteryType") == 0)
                    {
                        Nuki::BatteryType battype = NukiHelper::batteryTypeToEnum(jsonchar);

                        if((int)battype != 0xff && !_isUltra)
                        {
                            if(_nukiAdvancedConfig.batteryType == battype)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setBatteryType(battype);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "automaticBatteryTypeDetection") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if((keyvalue == 0 || keyvalue == 1) && !_isUltra)
                        {
                            if(_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableAutoBatteryTypeDetection((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "unlatchDuration") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 1 && keyvalue <= 30)
                        {
                            if(_nukiAdvancedConfig.unlatchDuration == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setUnlatchDuration(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "autoLockTimeOut") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 30 && keyvalue <= 1800)
                        {
                            if(_nukiAdvancedConfig.autoLockTimeOut == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setAutoLockTimeOut(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "autoUnLockDisabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.autoUnLockDisabled == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.disableAutoUnlock((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeEnabled == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableNightMode((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeStartTime") == 0)
                    {
                        String keystr = jsonchar;
                        unsigned char keyvalue[2];
                        keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                        keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                        if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                        {
                            if(_nukiAdvancedConfig.nightModeStartTime == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setNightModeStartTime(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeEndTime") == 0)
                    {
                        String keystr = jsonchar;
                        unsigned char keyvalue[2];
                        keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                        keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                        if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                        {
                            if(_nukiAdvancedConfig.nightModeEndTime == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setNightModeEndTime(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeAutoLockEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeAutoLockEnabled == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableNightModeAutoLock((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeAutoUnlockDisabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeAutoUnlockDisabled == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.disableNightModeAutoUnlock((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeImmediateLockOnStart") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.nightModeImmediateLockOnStart == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableNightModeImmediateLockOnStart((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "autoLockEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.autoLockEnabled == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableAutoLock((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "immediateAutoLockEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.immediateAutoLockEnabled == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableImmediateAutoLock((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "autoUpdateEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.autoUpdateEnabled == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableAutoUpdate((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "rebootNuki") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 1)
                        {
                            cmdResult = _nukiLock.requestReboot();
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "motorSpeed") == 0)
                    {
                        NukiLock::MotorSpeed motorSpeed = NukiHelper::motorSpeedToEnum(jsonchar);

                        if((int)motorSpeed != 0xff)
                        {
                            if(_nukiAdvancedConfig.motorSpeed == motorSpeed)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setMotorSpeed(motorSpeed);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "enableSlowSpeedDuringNightMode") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.enableSlowSpeedDuringNightMode == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableSlowSpeedDuringNightMode((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }

                    if(cmdResult != Nuki::CmdResult::Success)
                    {
                        ++retryCount;
                    }
                    else
                    {
                        break;
                    }
                }

                if(cmdResult == Nuki::CmdResult::Success)
                {
                    advancedUpdated = true;
                }

                if(!jsonResult[advancedKeys[j]])
                {
                    char resultStr[15] = {0};
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[advancedKeys[j]] = resultStr;
                }
            }
            else
            {
                jsonResult[advancedKeys[j]] = "accessDenied";
            }
        }
    }

    if(basicUpdated || advancedUpdated)
    {
        jsonResult["general"] = "success";
    }
    else
    {
        jsonResult["general"] = "noChange";
    }

    _nextConfigUpdateTs = espMillis() + 300;

    serializeJson(jsonResult, _buffer, _bufferSize);
    _network->publishConfigCommandResult(_buffer);

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

void NukiWrapper::onAuthCommandReceivedCallback(const char *value)
{
    nukiInst->onAuthCommandReceived(value);
}


void NukiWrapper::gpioActionCallback(const GpioAction &action, const int& pin)
{
    nukiInst->onGpioActionReceived(action, pin);
}

void NukiWrapper::onGpioActionReceived(const GpioAction &action, const int &pin)
{
    switch(action)
    {
    case GpioAction::Lock:
        if(!_nukiOfficial->getOffConnected())
        {
            nukiInst->lock();
        }
        else
        {
            _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
            _offCommand = NukiLock::LockAction::Lock;
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
            _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
            _offCommand = NukiLock::LockAction::Unlock;
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
            _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
            _offCommand = NukiLock::LockAction::Unlatch;
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
            _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
            _offCommand = NukiLock::LockAction::LockNgo;
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
            _nukiOfficial->setOffCommandExecutedTs(espMillis() + 2000);
            _offCommand = NukiLock::LockAction::LockNgoUnlatch;
            _network->publishOffAction(5);
        }
        break;
    }
}

void NukiWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    if(_disableNonJSON)
    {
        return;
    }

    if(!_preferences->getBool(preference_keypad_control_enabled))
    {
        _network->publishKeypadCommandResult("KeypadControlDisabled");
        return;
    }

    if(!hasKeypad())
    {
        if(_nukiConfigValid)
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
    int retryCount = 0;

    while(retryCount < _nrOfRetries + 1)
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
            Log->print("Add keypad code: ");
            Log->println((int)result);
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
            Log->print("Delete keypad code: ");
            Log->println((int)result);
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
            Log->print("Update keypad code: ");
            Log->println((int)result);
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

        if(result != Nuki::CmdResult::Success)
        {
            ++retryCount;
        }
        else
        {
            break;
        }
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

    if(!hasKeypad())
    {
        if(_nukiConfigValid)
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

    if(json["code"].is<JsonVariant>())
    {
        code = json["code"].as<unsigned int>();
    }
    else
    {
        code = 12;
    }

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["timeLimited"].is<JsonVariant>())
    {
        timeLimited = json["timeLimited"].as<unsigned int>();
    }
    else
    {
        timeLimited = 2;
    }

    if(json["name"].is<JsonVariant>())
    {
        name = json["name"].as<String>();
    }
    if(json["allowedFrom"].is<JsonVariant>())
    {
        allowedFrom = json["allowedFrom"].as<String>();
    }
    if(json["allowedUntil"].is<JsonVariant>())
    {
        allowedUntil = json["allowedUntil"].as<String>();
    }
    if(json["allowedWeekdays"].is<JsonVariant>())
    {
        allowedWeekdays = json["allowedWeekdays"].as<String>();
    }
    if(json["allowedFromTime"].is<JsonVariant>())
    {
        allowedFromTime = json["allowedFromTime"].as<String>();
    }
    if(json["allowedUntilTime"].is<JsonVariant>())
    {
        allowedUntilTime = json["allowedUntilTime"].as<String>();
    }

    if(action)
    {
        bool idExists = false;

        if(codeId)
        {
            idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), codeId) != _keypadCodeIds.end();
        }

        if(strcmp(action, "check") == 0)
        {
            if(!_preferences->getBool(preference_keypad_check_code_enabled, false))
            {
                _network->publishKeypadJsonCommandResult("checkingKeypadCodesDisabled");
                return;
            }

            if((pow(_invalidCount, 5) + _lastCodeCheck) > espMillis())
            {
                _network->publishKeypadJsonCommandResult("checkingCodesBlockedTooManyInvalid");
                _lastCodeCheck = espMillis();
                return;
            }

            _lastCodeCheck = espMillis();

            if(idExists)
            {
                auto it1 = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), codeId);
                int index = it1 - _keypadCodeIds.begin();
                Log->print("Check keypad code: ");

                if(code == _keypadCodes[index])
                {
                    _invalidCount = 0;
                    _network->publishKeypadJsonCommandResult("codeValid");
                    Log->println("Valid");
                    return;
                }
                else
                {
                    _invalidCount++;
                    _network->publishKeypadJsonCommandResult("codeInvalid");
                    Log->print("Invalid\nInvalid count: ");
                    Log->println(_invalidCount);
                    return;
                }
            }
            else
            {
                _invalidCount++;
                _network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                Log->print("Invalid count: ");
                Log->println(_invalidCount);
                return;
            }
        }
        else
        {

            Nuki::CmdResult result = (Nuki::CmdResult)-1;
            int retryCount = 0;

            while(retryCount < _nrOfRetries + 1)
            {
                if(strcmp(action, "delete") == 0)
                {
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
                    if(name.length() < 1)
                    {
                        if (strcmp(action, "update") != 0)
                        {
                            _network->publishKeypadJsonCommandResult("noNameSet");
                            return;
                        }
                    }

                    if(code != 12)
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
                            if(allowedUntil.length() == 19)
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

                        if(allowedWeekdays.indexOf("mon") >= 0)
                        {
                            allowedWeekdaysInt += 64;
                        }
                        if(allowedWeekdays.indexOf("tue") >= 0)
                        {
                            allowedWeekdaysInt += 32;
                        }
                        if(allowedWeekdays.indexOf("wed") >= 0)
                        {
                            allowedWeekdaysInt += 16;
                        }
                        if(allowedWeekdays.indexOf("thu") >= 0)
                        {
                            allowedWeekdaysInt += 8;
                        }
                        if(allowedWeekdays.indexOf("fri") >= 0)
                        {
                            allowedWeekdaysInt += 4;
                        }
                        if(allowedWeekdays.indexOf("sat") >= 0)
                        {
                            allowedWeekdaysInt += 2;
                        }
                        if(allowedWeekdays.indexOf("sun") >= 0)
                        {
                            allowedWeekdaysInt += 1;
                        }
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
                        Log->print("Add keypad code: ");
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
                        bool foundExisting = false;

                        if(resultKp == Nuki::CmdResult::Success)
                        {
                            if (esp_task_wdt_status(NULL) == ESP_OK)
                            {
                                esp_task_wdt_reset();
                            }
                            vTaskDelay(5000 / portTICK_PERIOD_MS);
                            std::list<NukiLock::KeypadEntry> entries;
                            _nukiLock.getKeypadEntries(&entries);

                            for(const auto& entry : entries)
                            {
                                if (codeId != entry.codeId)
                                {
                                    continue;
                                }
                                else
                                {
                                    foundExisting = true;
                                }

                                if(name.length() < 1)
                                {
                                    memset(oldName, 0, sizeof(oldName));
                                    memcpy(oldName, entry.name, sizeof(entry.name));
                                }
                                if(code == 12)
                                {
                                    code = entry.code;
                                }
                                if(enabled == 2)
                                {
                                    enabled = entry.enabled;
                                }
                                if(timeLimited == 2)
                                {
                                    timeLimited = entry.timeLimited;
                                }
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
                                if(allowedWeekdays.length() < 1)
                                {
                                    allowedWeekdaysInt = entry.allowedWeekdays;
                                }
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
                        Log->print("Update keypad code: ");
                        Log->println((int)result);
                    }
                }
                else
                {
                    _network->publishKeypadJsonCommandResult("invalidAction");
                    return;
                }

                if(result != Nuki::CmdResult::Success)
                {
                    ++retryCount;
                }
                else
                {
                    break;
                }
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
    }
    else
    {
        _network->publishKeypadJsonCommandResult("noActionSet");
        return;
    }
}

void NukiWrapper::onTimeControlCommandReceived(const char *value)
{
    if(!_nukiConfigValid)
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

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["weekdays"].is<JsonVariant>())
    {
        weekdays = json["weekdays"].as<String>();
    }
    if(json["time"].is<JsonVariant>())
    {
        time = json["time"].as<String>();
    }
    if(json["lockAction"].is<JsonVariant>())
    {
        lockAction = json["lockAction"].as<String>();
    }

    if(lockAction.length() > 0)
    {
        timeControlLockAction = NukiHelper::lockActionToEnum(lockAction.c_str());

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
        int retryCount = 0;

        while(retryCount < _nrOfRetries + 1)
        {
            if(strcmp(action, "delete") == 0)
            {
                if(idExists)
                {
                    result = _nukiLock.removeTimeControlEntry(entryId);
                    Log->print("Delete timecontrol: ");
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

                if(weekdays.indexOf("mon") >= 0)
                {
                    weekdaysInt += 64;
                }
                if(weekdays.indexOf("tue") >= 0)
                {
                    weekdaysInt += 32;
                }
                if(weekdays.indexOf("wed") >= 0)
                {
                    weekdaysInt += 16;
                }
                if(weekdays.indexOf("thu") >= 0)
                {
                    weekdaysInt += 8;
                }
                if(weekdays.indexOf("fri") >= 0)
                {
                    weekdaysInt += 4;
                }
                if(weekdays.indexOf("sat") >= 0)
                {
                    weekdaysInt += 2;
                }
                if(weekdays.indexOf("sun") >= 0)
                {
                    weekdaysInt += 1;
                }

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
                    Log->print("Add timecontrol: ");
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
                    bool foundExisting = false;

                    if(resultTc == Nuki::CmdResult::Success)
                    {
                        if (esp_task_wdt_status(NULL) == ESP_OK)
                        {
                            esp_task_wdt_reset();
                        }
                        vTaskDelay(5000 / portTICK_PERIOD_MS);
                        std::list<NukiLock::TimeControlEntry> timeControlEntries;
                        _nukiLock.getTimeControlEntries(&timeControlEntries);

                        for(const auto& entry : timeControlEntries)
                        {
                            if (entryId != entry.entryId)
                            {
                                continue;
                            }
                            else
                            {
                                foundExisting = true;
                            }

                            if(enabled == 2)
                            {
                                enabled = entry.enabled;
                            }
                            if(weekdays.length() < 1)
                            {
                                weekdaysInt = entry.weekdays;
                            }
                            if(time.length() < 1)
                            {
                                time = "old";
                                timeAr[0] = entry.timeHour;
                                timeAr[1] = entry.timeMin;
                            }
                            if(lockAction.length() < 1)
                            {
                                timeControlLockAction = entry.lockAction;
                            }
                        }

                        if(!foundExisting)
                        {
                            _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
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
                    Log->print("Update timecontrol: ");
                    Log->println((int)result);
                }
            }
            else
            {
                _network->publishTimeControlCommandResult("invalidAction");
                return;
            }

            if(result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            NukiLock::cmdResultToString(result, resultStr);
            _network->publishTimeControlCommandResult(resultStr);
        }

        _nextConfigUpdateTs = espMillis() + 300;
    }
    else
    {
        _network->publishTimeControlCommandResult("noActionSet");
        return;
    }
}

void NukiWrapper::onAuthCommandReceived(const char *value)
{
    if(!_nukiConfigValid)
    {
        _network->publishAuthCommandResult("configNotReady");
        return;
    }

    if(!isPinValid())
    {
        _network->publishAuthCommandResult("noValidPinSet");
        return;
    }

    if(!_preferences->getBool(preference_auth_control_enabled))
    {
        _network->publishAuthCommandResult("keypadControlDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        _network->publishAuthCommandResult("invalidJson");
        return;
    }

    char oldName[33];
    const char *action = json["action"].as<const char*>();
    uint32_t authId = json["authId"].as<unsigned int>();
    //uint8_t idType = json["idType"].as<unsigned int>();
    //unsigned char secretKeyK[32] = {0x00};
    uint8_t remoteAllowed;
    uint8_t enabled;
    uint8_t timeLimited;
    String name;
    //String sharedKey;
    String allowedFrom;
    String allowedUntil;
    String allowedWeekdays;
    String allowedFromTime;
    String allowedUntilTime;

    if(json["remoteAllowed"].is<JsonVariant>())
    {
        remoteAllowed = json["remoteAllowed"].as<unsigned int>();
    }
    else
    {
        remoteAllowed = 2;
    }

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["timeLimited"].is<JsonVariant>())
    {
        timeLimited = json["timeLimited"].as<unsigned int>();
    }
    else
    {
        timeLimited = 2;
    }

    if(json["name"].is<JsonVariant>())
    {
        name = json["name"].as<String>();
    }
    //if(json["sharedKey"].is<JsonVariant>()) sharedKey = json["sharedKey"].as<String>();
    if(json["allowedFrom"].is<JsonVariant>())
    {
        allowedFrom = json["allowedFrom"].as<String>();
    }
    if(json["allowedUntil"].is<JsonVariant>())
    {
        allowedUntil = json["allowedUntil"].as<String>();
    }
    if(json["allowedWeekdays"].is<JsonVariant>())
    {
        allowedWeekdays = json["allowedWeekdays"].as<String>();
    }
    if(json["allowedFromTime"].is<JsonVariant>())
    {
        allowedFromTime = json["allowedFromTime"].as<String>();
    }
    if(json["allowedUntilTime"].is<JsonVariant>())
    {
        allowedUntilTime = json["allowedUntilTime"].as<String>();
    }

    if(action)
    {
        bool idExists = false;

        if(authId)
        {
            idExists = std::find(_authIds.begin(), _authIds.end(), authId) != _authIds.end();
        }

        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while(retryCount < _nrOfRetries)
        {
            if(strcmp(action, "delete") == 0)
            {
                if(idExists)
                {
                    result = _nukiLock.deleteAuthorizationEntry(authId);
                    if (esp_task_wdt_status(NULL) == ESP_OK)
                    {
                        esp_task_wdt_reset();
                    }
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                    Log->print("Delete authorization: ");
                    Log->println((int)result);
                }
                else
                {
                    _network->publishAuthCommandResult("noExistingAuthIdSet");
                    return;
                }
            }
            else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
            {
                if(name.length() < 1)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        _network->publishAuthCommandResult("noNameSet");
                        return;
                    }
                }

                /*
                if(sharedKey.length() != 64)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        _network->publishAuthCommandResult("noSharedKeySet");
                        return;
                    }
                }
                else
                {
                    for(int i=0; i<sharedKey.length();i+=2) secretKeyK[(i/2)] = std::stoi(sharedKey.substring(i, i+2).c_str(), nullptr, 16);
                }
                */

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
                                _network->publishAuthCommandResult("invalidAllowedFrom");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedFrom");
                            return;
                        }
                    }

                    if(allowedUntil.length() > 0)
                    {
                        if(allowedUntil.length() == 19)
                        {
                            allowedUntilAr[0] = (uint16_t)allowedUntil.substring(0, 4).toInt();
                            allowedUntilAr[1] = (uint8_t)allowedUntil.substring(5, 7).toInt();
                            allowedUntilAr[2] = (uint8_t)allowedUntil.substring(8, 10).toInt();
                            allowedUntilAr[3] = (uint8_t)allowedUntil.substring(11, 13).toInt();
                            allowedUntilAr[4] = (uint8_t)allowedUntil.substring(14, 16).toInt();
                            allowedUntilAr[5] = (uint8_t)allowedUntil.substring(17, 19).toInt();

                            if(allowedUntilAr[0] < 2000 || allowedUntilAr[0] > 3000 || allowedUntilAr[1] < 1 || allowedUntilAr[1] > 12 || allowedUntilAr[2] < 1 || allowedUntilAr[2] > 31 || allowedUntilAr[3] < 0 || allowedUntilAr[3] > 23 || allowedUntilAr[4] < 0 || allowedUntilAr[4] > 59 || allowedUntilAr[5] < 0 || allowedUntilAr[5] > 59)
                            {
                                _network->publishAuthCommandResult("invalidAllowedUntil");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedUntil");
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
                                _network->publishAuthCommandResult("invalidAllowedFromTime");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedFromTime");
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
                                _network->publishAuthCommandResult("invalidAllowedUntilTime");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedUntilTime");
                            return;
                        }
                    }

                    if(allowedWeekdays.indexOf("mon") >= 0)
                    {
                        allowedWeekdaysInt += 64;
                    }
                    if(allowedWeekdays.indexOf("tue") >= 0)
                    {
                        allowedWeekdaysInt += 32;
                    }
                    if(allowedWeekdays.indexOf("wed") >= 0)
                    {
                        allowedWeekdaysInt += 16;
                    }
                    if(allowedWeekdays.indexOf("thu") >= 0)
                    {
                        allowedWeekdaysInt += 8;
                    }
                    if(allowedWeekdays.indexOf("fri") >= 0)
                    {
                        allowedWeekdaysInt += 4;
                    }
                    if(allowedWeekdays.indexOf("sat") >= 0)
                    {
                        allowedWeekdaysInt += 2;
                    }
                    if(allowedWeekdays.indexOf("sun") >= 0)
                    {
                        allowedWeekdaysInt += 1;
                    }
                }

                if(strcmp(action, "add") == 0)
                {
                    _network->publishAuthCommandResult("addActionNotSupported");
                    return;

                    NukiLock::NewAuthorizationEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    size_t nameLen = name.length();
                    memcpy(&entry.name, name.c_str(), nameLen > 32 ? 32 : nameLen);
                    /*
                    memcpy(&entry.sharedKey, secretKeyK, 32);

                    if(idType != 1)
                    {
                        _network->publishAuthCommandResult("invalidIdType");
                        return;
                    }

                    entry.idType = idType;
                    */
                    entry.remoteAllowed = remoteAllowed == 1 ? 1 : 0;
                    entry.timeLimited = timeLimited == 1 ? 1 : 0;

                    if(allowedFrom.length() > 0)
                    {
                        entry.allowedFromYear = allowedFromAr[0];
                        entry.allowedFromMonth = allowedFromAr[1];
                        entry.allowedFromDay = allowedFromAr[2];
                        entry.allowedFromHour = allowedFromAr[3];
                        entry.allowedFromMinute = allowedFromAr[4];
                        entry.allowedFromSecond = allowedFromAr[5];
                    }

                    if(allowedUntil.length() > 0)
                    {
                        entry.allowedUntilYear = allowedUntilAr[0];
                        entry.allowedUntilMonth = allowedUntilAr[1];
                        entry.allowedUntilDay = allowedUntilAr[2];
                        entry.allowedUntilHour = allowedUntilAr[3];
                        entry.allowedUntilMinute = allowedUntilAr[4];
                        entry.allowedUntilSecond = allowedUntilAr[5];
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

                    result = _nukiLock.addAuthorizationEntry(entry);
                    if (esp_task_wdt_status(NULL) == ESP_OK)
                    {
                        esp_task_wdt_reset();
                    }
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                    Log->print("Add authorization: ");
                    Log->println((int)result);
                }
                else if (strcmp(action, "update") == 0)
                {
                    if(!authId)
                    {
                        _network->publishAuthCommandResult("noAuthIdSet");
                        return;
                    }

                    if(!idExists)
                    {
                        _network->publishAuthCommandResult("noExistingAuthIdSet");
                        return;
                    }

                    Nuki::CmdResult resultAuth = _nukiLock.retrieveAuthorizationEntries(0, _preferences->getInt(preference_auth_max_entries, MAX_AUTH));
                    bool foundExisting = false;

                    if(resultAuth == Nuki::CmdResult::Success)
                    {
                        if (esp_task_wdt_status(NULL) == ESP_OK)
                        {
                            esp_task_wdt_reset();
                        }
                        vTaskDelay(5000 / portTICK_PERIOD_MS);
                        std::list<NukiLock::AuthorizationEntry> entries;
                        _nukiLock.getAuthorizationEntries(&entries);

                        for(const auto& entry : entries)
                        {
                            if (authId != entry.authId)
                            {
                                continue;
                            }
                            else
                            {
                                foundExisting = true;
                            }

                            if(name.length() < 1)
                            {
                                memset(oldName, 0, sizeof(oldName));
                                memcpy(oldName, entry.name, sizeof(entry.name));
                            }
                            if(remoteAllowed == 2)
                            {
                                remoteAllowed = entry.remoteAllowed;
                            }
                            if(enabled == 2)
                            {
                                enabled = entry.enabled;
                            }
                            if(timeLimited == 2)
                            {
                                timeLimited = entry.timeLimited;
                            }
                            if(allowedFrom.length() < 1)
                            {
                                allowedFrom = "old";
                                allowedFromAr[0] = entry.allowedFromYear;
                                allowedFromAr[1] = entry.allowedFromMonth;
                                allowedFromAr[2] = entry.allowedFromDay;
                                allowedFromAr[3] = entry.allowedFromHour;
                                allowedFromAr[4] = entry.allowedFromMinute;
                                allowedFromAr[5] = entry.allowedFromSecond;
                            }
                            if(allowedUntil.length() < 1)
                            {
                                allowedUntil = "old";
                                allowedUntilAr[0] = entry.allowedUntilYear;
                                allowedUntilAr[1] = entry.allowedUntilMonth;
                                allowedUntilAr[2] = entry.allowedUntilDay;
                                allowedUntilAr[3] = entry.allowedUntilHour;
                                allowedUntilAr[4] = entry.allowedUntilMinute;
                                allowedUntilAr[5] = entry.allowedUntilSecond;
                            }
                            if(allowedWeekdays.length() < 1)
                            {
                                allowedWeekdaysInt = entry.allowedWeekdays;
                            }
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
                            _network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
                        return;
                    }

                    NukiLock::UpdatedAuthorizationEntry entry;

                    memset(&entry, 0, sizeof(entry));
                    entry.authId = authId;

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
                    entry.remoteAllowed = remoteAllowed;
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
                                entry.allowedFromMinute = allowedFromAr[4];
                                entry.allowedFromSecond = allowedFromAr[5];
                            }

                            if(allowedUntil.length() > 0)
                            {
                                entry.allowedUntilYear = allowedUntilAr[0];
                                entry.allowedUntilMonth = allowedUntilAr[1];
                                entry.allowedUntilDay = allowedUntilAr[2];
                                entry.allowedUntilHour = allowedUntilAr[3];
                                entry.allowedUntilMinute = allowedUntilAr[4];
                                entry.allowedUntilSecond = allowedUntilAr[5];
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

                    result = _nukiLock.updateAuthorizationEntry(entry);
                    if (esp_task_wdt_status(NULL) == ESP_OK)
                    {
                        esp_task_wdt_reset();
                    }
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                    Log->print("Update authorization: ");
                    Log->println((int)result);
                }
            }
            else
            {
                _network->publishAuthCommandResult("invalidAction");
                return;
            }

            if(result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        updateAuth(false);

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            NukiLock::cmdResultToString(result, resultStr);
            _network->publishAuthCommandResult(resultStr);
        }
    }
    else
    {
        _network->publishAuthCommandResult("noActionSet");
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
    int retryCount = 0;

    while(retryCount < _nrOfRetries + 1)
    {
        result = _nukiLock.requestConfig(&_nukiConfig);
        _nukiConfigValid = result == Nuki::CmdResult::Success;

        char resultStr[20];
        NukiLock::cmdResultToString(result, resultStr);
        Log->print("Lock config result: ");
        Log->println(resultStr);

        if(result != Nuki::CmdResult::Success)
        {
            ++retryCount;
            Log->println("Failed to retrieve lock config, retrying in 1s");
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        else
        {
            break;
        }
    }
}

void NukiWrapper::readAdvancedConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while(retryCount < _nrOfRetries + 1)
    {
        result = _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
        _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;

        char resultStr[20];
        NukiLock::cmdResultToString(result, resultStr);
        Log->print("Lock advanced config result: ");
        Log->println(resultStr);

        if(result != Nuki::CmdResult::Success)
        {
            ++retryCount;
            Log->println("Failed to retrieve lock advanced config, retrying in 1s");
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        else
        {
            break;
        }
    }
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

const std::string NukiWrapper::hardwareVersion() const
{
    return _hardwareVersion;
}

void NukiWrapper::setCommErrorPins(const uint8_t& value)
{
    for (uint8_t pin : _pinsCommError)
    {
        _gpio->setPinOutput(pin, value);
    }
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

void NukiWrapper::updateTime()
{
    if(!isPinValid())
    {
        Log->println("No valid PIN set");
        return;
    }

    time_t now;
    tm tm;
    time(&now);
    localtime_r(&now, &tm);

    if (int(tm.tm_year + 1900) < int(2025))
    {
        Log->println("NTP Time not valid, not updating Nuki device");
        return;
    }

    Nuki::TimeValue nukiTime;
    nukiTime.year = tm.tm_year + 1900;
    nukiTime.month = tm.tm_mon + 1;
    nukiTime.day = tm.tm_mday;
    nukiTime.hour = tm.tm_hour;
    nukiTime.minute = tm.tm_min;
    nukiTime.second = tm.tm_sec;

    Nuki::CmdResult cmdResult = _nukiLock.updateTime(nukiTime);

    char resultStr[15] = {0};
    NukiLock::cmdResultToString(cmdResult, resultStr);

    Log->print("Lock time update result: ");
    Log->println(resultStr);
}