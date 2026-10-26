#include "NukiOpenerWrapper.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "Logger.h"
#include "RestartReason.h"
#include <NukiOpenerUtils.h>
#include "Config.h"
#include "enums/NukiPinState.h"
#include "enums/NukiPinState.h"
#include "hal/wdt_hal.h"
#include <time.h>
#include "esp_sntp.h"
#include "util/NukiOpenerHelper.h"
#include "util/NukiHelper.h"

NukiOpenerWrapper* nukiOpenerInst;
Preferences* nukiOpenerPreferences = nullptr;

NukiOpenerWrapper::NukiOpenerWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NukiNetworkOpener* network, Gpio* gpio, Preferences* preferences, char* buffer, size_t bufferSize)
    : _deviceName(deviceName),
      _deviceId(deviceId),
      _nukiOpener(deviceName, _deviceId->get()),
      _bleScanner(scanner),
      _network(network),
      _gpio(gpio),
      _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize)
{
    Log->print("Device id opener: ");
    Log->println(_deviceId->get());

    nukiOpenerInst = this;

    memset(&_lastKeyTurnerState, sizeof(NukiOpener::OpenerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiOpener::BatteryReport), 0);
    memset(&_batteryReport, sizeof(NukiOpener::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiOpener::OpenerState), 0);
    _keyTurnerState.lockState = NukiOpener::LockState::Undefined;

    network->setLockActionReceivedCallback(nukiOpenerInst->onLockActionReceivedCallback);
    network->setConfigUpdateReceivedCallback(nukiOpenerInst->onConfigUpdateReceivedCallback);
    network->setKeypadCommandReceivedCallback(nukiOpenerInst->onKeypadCommandReceivedCallback);
    network->setKeypadJsonCommandReceivedCallback(nukiOpenerInst->onKeypadJsonCommandReceivedCallback);
    network->setTimeControlCommandReceivedCallback(nukiOpenerInst->onTimeControlCommandReceivedCallback);
    network->setAuthCommandReceivedCallback(nukiOpenerInst->onAuthCommandReceivedCallback);

    _gpio->addCallback(NukiOpenerWrapper::gpioActionCallback);
}

NukiOpenerWrapper::~NukiOpenerWrapper()
{
    _bleScanner = nullptr;
}

void NukiOpenerWrapper::initialize()
{
    _keypadJsonQueue = xQueueCreate(KEYPAD_JSON_QUEUE_SIZE, KEYPAD_JSON_MAX_LENGTH);
    _nukiOpener.setDebugConnect(_preferences->getBool(preference_debug_connect, false));
    _nukiOpener.setDebugCommunication(_preferences->getBool(preference_debug_communication, false));
    _nukiOpener.setDebugReadableData(_preferences->getBool(preference_debug_readable_data, false));
    _nukiOpener.setDebugHexData(_preferences->getBool(preference_debug_hex_data, false));
    _nukiOpener.setDebugCommand(_preferences->getBool(preference_debug_command, false));
    _nukiOpener.registerLogger(Log);

    _nukiOpener.initialize();
    _nukiOpener.registerBleScanner(_bleScanner);
    _nukiOpener.setEventHandler(this);
    _nukiOpener.setConnectTimeout(3);
    _nukiOpener.setDisconnectTimeout(2000);
    _nukiOpener.setGeneralTimeout(_preferences->getInt(preference_ble_general_timeout, 10000));
    _nukiOpener.setCommandTimeout(_preferences->getInt(preference_ble_command_timeout, 3000));

    _hassEnabled = _preferences->getBool(preference_mqtt_hass_enabled, false);
    readSettings();
#ifndef NUKI_HUB_UPDATER
    _nukiRetryHandler = new NukiRetryHandler("Opener", _gpio, _gpio->getPinsWithRole(PinRole::OutputHighBluetoothComm), _gpio->getPinsWithRole(PinRole::OutputHighBluetoothCommError), _nrOfRetries, _retryDelay);
#endif
}

void NukiOpenerWrapper::readSettings()
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

    _nukiOpener.setPower(powerLevel);

    _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
    _intervalConfig = _preferences->getInt(preference_query_interval_configuration);
    _intervalBattery = _preferences->getInt(preference_query_interval_battery);
    _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
    _keypadEnabled = _preferences->getBool(preference_keypad_info_enabled);
    _publishAuthData = _preferences->getBool(preference_publish_authdata);
    _maxKeypadCodeCount = _preferences->getUInt(preference_opener_max_keypad_code_count);
    _maxTimeControlEntryCount = _preferences->getUInt(preference_opener_max_timecontrol_entry_count);
    _maxAuthEntryCount = _preferences->getUInt(preference_opener_max_auth_entry_count);
    _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
    _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries, 200);
    _retryDelay = _preferences->getInt(preference_command_retry_delay);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;
    _disableNonJSON = _preferences->getBool(preference_disable_non_json, false);
    _checkKeypadCodes = _preferences->getBool(preference_keypad_check_code_enabled, false);
    _pairedAsApp = _preferences->getBool(preference_register_opener_as_app, false);
    _forceKeypad = _preferences->getBool(preference_opener_force_keypad, false);
    _forceId = _preferences->getBool(preference_opener_force_id, false);

    _preferences->getBytes(preference_conf_opener_basic_acl, &_basicOpenerConfigAclPrefs, sizeof(_basicOpenerConfigAclPrefs));
    _preferences->getBytes(preference_conf_opener_advanced_acl, &_advancedOpenerConfigAclPrefs, sizeof(_advancedOpenerConfigAclPrefs));

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

    Log->print("Opener state interval: ");
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

const uint8_t NukiOpenerWrapper::restartController() const
{
    return _restartController;
}

bool NukiOpenerWrapper::hasConnected()
{
    return _hasConnected;
}

bool NukiOpenerWrapper::checkPaired()
{
    if (_paired) return true;

    Log->println("Nuki opener start pairing");
    _network->publishBleAddress("");

    Nuki::AuthorizationIdType idType = _preferences->getBool(preference_register_opener_as_app) ?
                                       Nuki::AuthorizationIdType::App :
                                       Nuki::AuthorizationIdType::Bridge;

    if(_nukiOpener.pairNuki(idType) == NukiOpener::PairingResult::Success)
    {
        Log->println("Nuki opener paired");
        _network->publishBleAddress(_nukiOpener.getBleAddress().toString());
        return true;
    }

    espDelayAck(200);

    return false;
}

void NukiOpenerWrapper::checkRestartByBeacon(const int64_t& ts)
{
    int64_t lastReceivedBeaconTs = _nukiOpener.getLastReceivedBeaconTs();

    if(_restartBeaconTimeout > 0 &&
        ts > 60000 &&
        lastReceivedBeaconTs > 0 &&
        _disableBleWatchdogTs < ts &&
        (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
    {
        Log->print("No BLE beacon received from the opener for ");
        Log->print((ts - lastReceivedBeaconTs) / 1000);
        Log->println(" seconds, signalling to restart BLE controller.");
        espDelayAck(200);
        _restartController = 2;
    }
}

void NukiOpenerWrapper::checkLockAction(const int64_t& ts)
{
    if(_nextLockAction != (NukiOpener::LockAction)0xff)
    {
        int retryCount = 0;

        Nuki::CmdResult result = _nukiRetryHandler->retryComm([&]()
        {
             Nuki::CmdResult cmdResult;
             cmdResult = _nukiOpener.lockAction(_nextLockAction, 0, 0);
             char resultStr[15] = {0};
             NukiLock::cmdResultToString(cmdResult, resultStr);
             _network->publishCommandResult(resultStr);

             Log->print("Opener lock action result: ");
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
            _nextLockAction = (NukiOpener::LockAction) 0xff;
            _network->publishRetry("--");
            retryCount = 0;
            Log->println("Opener: updating status after action");
            _statusUpdatedTs = ts;
        }
        else
        {
            Log->println("Opener: Maximum number of retries exceeded, aborting.");
            _network->publishRetry("failed");
            retryCount = 0;
            _nextLockAction = (NukiOpener::LockAction) 0xff;
        }
    }
}

void NukiOpenerWrapper::checkQueries(const int64_t& ts, const uint8_t& queryCommands)
{
    if(_statusUpdated) return;

    if(_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs || (queryCommands & QUERY_COMMAND_BATTERY) > 0)
    {
        _nextBatteryReportTs = ts + _intervalBattery * 1000;
        updateBatteryState();
    }
    if(_nextConfigUpdateTs == 0 || ts > _nextConfigUpdateTs || (queryCommands & QUERY_COMMAND_CONFIG) > 0)
    {
        _nextConfigUpdateTs = ts + _intervalConfig * 1000;
        updateConfig();
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
    if(_preferences->getBool(preference_update_time, false) && ts > (120 * 1000) && ts > _nextTimeUpdateTs)
    {
        _nextTimeUpdateTs = ts + (updateTime() ? (12 * 60 * 60 * 1000) : 60000);
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
        _network->setupHASS(2, _nukiConfig.nukiId, (char*)_nukiConfig.name, _firmwareVersion.c_str(), _hardwareVersion.c_str(), false, hasKeypad());
        _hassSetupCompleted = true;
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
    if(hasKeypad() && _keypadEnabled && (_nextKeypadUpdateTs == 0 || ts > _nextKeypadUpdateTs || (queryCommands & QUERY_COMMAND_KEYPAD) > 0))
    {
        if(uxQueueMessagesWaiting(_keypadJsonQueue) > 0)
        {
            _nextKeypadUpdateTs = ts + 3000;
        }
        else
        {
            _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
            updateKeypad(false);
        }
    }
}

void NukiOpenerWrapper::update()
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
    _nukiOpener.updateConnectionState();
    checkLockAction(ts);
    checkKeypadJsonCommand();
    if(_keypadCacheDirty && ts > _nextCachePublishTs)
    {
        _keypadCacheDirty = false;
        _nextCachePublishTs = ts + 2000;
        publishKeypadCache();
    }

    if((_statusUpdated && ts >= _nextStatusPollTs) || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
        _statusUpdated = updateKeyTurnerState();
        _network->publishStatusUpdated(_statusUpdated);

        if(_statusUpdated)
        {
            _nextStatusPollTs = ts + 4000;
        }
    }
    if(_network->mqttConnectionState() == 2)
    {
        checkQueries(ts, queryCommands);

        if(_clearAuthData)
        {
            _network->clearAuthorizationInfo();
            _clearAuthData = false;
        }
        if(_checkKeypadCodes && _invalidCount > 0 && (ts - (120000 * _invalidCount)) > _lastCodeCheck)
        {
            _invalidCount--;
        }
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(NukiOpener::OpenerState));
}

void NukiOpenerWrapper::electricStrikeActuation()
{
    _nextLockAction = NukiOpener::LockAction::ElectricStrikeActuation;
}

void NukiOpenerWrapper::activateRTO()
{
    _nextLockAction = NukiOpener::LockAction::ActivateRTO;
}

void NukiOpenerWrapper::activateCM()
{
    _nextLockAction = NukiOpener::LockAction::ActivateCM;
}

void NukiOpenerWrapper::deactivateRtoCm()
{
    if(_keyTurnerState.nukiState == NukiOpener::State::ContinuousMode)
    {
        _nextLockAction = NukiOpener::LockAction::DeactivateCM;
    }
    else if(_keyTurnerState.lockState == NukiOpener::LockState::RTOactive)
    {
        _nextLockAction = NukiOpener::LockAction::DeactivateRTO;
    }
}

void NukiOpenerWrapper::deactivateRTO()
{
    _nextLockAction = NukiOpener::LockAction::DeactivateRTO;
}

void NukiOpenerWrapper::deactivateCM()
{
    _nextLockAction = NukiOpener::LockAction::DeactivateCM;
}

bool NukiOpenerWrapper::isPinValid()
{
    return _preferences->getInt(preference_opener_pin_status, (int)NukiPinState::NotConfigured) == (int)NukiPinState::Valid;
}

void NukiOpenerWrapper::setPin(const uint16_t pin)
{
    _nukiOpener.saveSecurityPincode(pin);
}

const uint16_t NukiOpenerWrapper::getPin()
{
    return _nukiOpener.getSecurityPincode();
}

void NukiOpenerWrapper::unpair()
{
    _nukiOpener.unPairNuki();
    _preferences->remove(preference_opener_log_num);
    Preferences nukiBlePref;
    nukiBlePref.begin("NukiHubopener", false);
    nukiBlePref.clear();
    nukiBlePref.end();
    _deviceId->assignNewId();
    if(!_forceId)
    {
        _preferences->remove(preference_nuki_id_opener);
    }
    _paired = false;
}

bool NukiOpenerWrapper::updateKeyTurnerState()
{
    bool updateStatus = false;
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    Log->println("Querying opener state");

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiOpener.requestOpenerState(&_keyTurnerState);
    });

    char resultStr[15];
    memset(&resultStr, 0, sizeof(resultStr));
    NukiOpener::cmdResultToString(result, resultStr);
    _network->publishLockstateCommandResult(resultStr);

    if(result != Nuki::CmdResult::Success)
    {
        Log->println("Query opener state failed");
        _retryLockstateCount++;
        postponeBleWatchdog();
        if(_retryLockstateCount < _nrOfRetries + 1)
        {
            Log->print("Query opener state retrying in ");
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
    else if (!_hasConnected)
    {
        _hasConnected = true;
    }

    _retryLockstateCount = 0;
    _network->publishAvailability(true);

    const NukiOpener::LockState& lockState = _keyTurnerState.lockState;

    if(lockState != _lastKeyTurnerState.lockState)
    {
        _statusUpdatedTs = espMillis();
    }

    if(!isPinValid() &&
            _statusUpdated &&
            _keyTurnerState.nukiState == NukiOpener::State::ContinuousMode &&
            _keyTurnerState.trigger == NukiOpener::Trigger::Manual &&
            (_keyTurnerState.lockState == NukiOpener::LockState::Open ||
             _keyTurnerState.lockState == NukiOpener::LockState::Opening) &&
            _lastKeyTurnerState.lockState != NukiOpener::LockState::Open &&
            _lastKeyTurnerState.lockState != NukiOpener::LockState::Opening)
    {
        Log->println("Nuki opener: Ring detected (CM)");
        _network->publishRing(false);
    }
    else if(!isPinValid() &&
            _statusUpdated &&
            _keyTurnerState.trigger == NukiOpener::Trigger::Manual &&
            (_keyTurnerState.lockState == NukiOpener::LockState::Open ||
             _keyTurnerState.lockState == NukiOpener::LockState::Opening) &&
            _lastKeyTurnerState.lockState == NukiOpener::LockState::RTOactive)
    {
        Log->println("Nuki opener: Ring detected (RTO)");
        _network->publishRing(false);
    }
    else if(!isPinValid() &&
            _statusUpdated &&
            _keyTurnerState.lockState == NukiOpener::LockState::Locked &&
            _lastKeyTurnerState.lockState == NukiOpener::LockState::Locked &&
            _lastKeyTurnerState.nukiState == _keyTurnerState.nukiState)
    {
        Log->println("Nuki opener: Ring detected");
        _network->publishRing(true);
    }
    else
    {
        if(isPinValid() && _keyTurnerState.lockState != NukiOpener::LockState::Open && _keyTurnerState.lockState != NukiOpener::LockState::Opening)
        {
            Log->println("Publishing auth data");
            updateAuthData(false);
            Log->println("Done publishing auth data");
        }

        if(_keyTurnerState.lockState == NukiOpener::LockState::Undefined)
        {
            if (_nextLockStateUpdateTs > espMillis() + 60000)
            {
                _nextLockStateUpdateTs = espMillis() + 60000;
            }
        }

        updateGpioOutputs();
        _network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);

        if((_keyTurnerState.lockState == NukiOpener::LockState::Open || _keyTurnerState.lockState == NukiOpener::LockState::Opening) && espMillis() < _statusUpdatedTs + 10000)
        {
            updateStatus = true;
            Log->println("Opener: Keep updating status on intermediate lock state");
        }

        if(_keyTurnerState.nukiState == NukiOpener::State::ContinuousMode)
        {
            Log->println("Continuous Mode");
        }

        char lockStateStr[20];
        lockstateToString(_keyTurnerState.lockState, lockStateStr);
        Log->print("Opener state: ");
        Log->println(lockStateStr);
    }

    postponeBleWatchdog();
    Log->println("Done querying opener state");
    return updateStatus;
}

void NukiOpenerWrapper::updateBatteryState()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiOpener.requestBatteryReport(&_batteryReport);
    });
    Log->print("Querying opener battery state: ");
    NukiOpenerHelper::printCommandResult(result);

    if(result == Nuki::CmdResult::Success)
    {
        _network->publishBatteryReport(_batteryReport);
    }
    postponeBleWatchdog();
    Log->println("Done querying opener battery state");
}

void NukiOpenerWrapper::updateConfig()
{
    bool expectedConfig = true;

    readConfig();

    if(_nukiConfigValid)
    {
        if(!_forceId && (_preferences->getUInt(preference_nuki_id_opener, 0) == 0  || _retryConfigCount == 10))
        {
            char uidString[20];
            itoa(_nukiConfig.nukiId, uidString, 16);
            Log->print("Saving Opener Nuki ID to preferences (");
            Log->print(_nukiConfig.nukiId);
            Log->print(" / ");
            Log->print(uidString);
            Log->println(")");
            _preferences->putUInt(preference_nuki_id_opener, _nukiConfig.nukiId);
        }

        if(_preferences->getUInt(preference_nuki_id_opener, 0) == _nukiConfig.nukiId)
        {
            _hasKeypad = _nukiConfig.hasKeypad == 1 || _nukiConfig.hasKeypadV2 == 1;
            _firmwareVersion = std::to_string(_nukiConfig.firmwareVersion[0]) + "." + std::to_string(_nukiConfig.firmwareVersion[1]) + "." + std::to_string(_nukiConfig.firmwareVersion[2]);
            _hardwareVersion = std::to_string(_nukiConfig.hardwareRevision[0]) + "." + std::to_string(_nukiConfig.hardwareRevision[1]);
            if(_preferences->getBool(preference_conf_info_enabled, true))
            {
                _network->publishConfig(_nukiConfig);
            }
            _retryConfigCount = 0;
            if(_preferences->getBool(preference_timecontrol_info_enabled, false))
            {
                updateTimeControl(false);
            }
            if(_preferences->getBool(preference_auth_info_enabled))
            {
                updateAuth(false);
            }

            const int pinStatus = _preferences->getInt(preference_opener_pin_status, (int)NukiPinState::NotConfigured);

            Nuki::CmdResult result = (Nuki::CmdResult)-1;

            result = _nukiRetryHandler->retryComm([&]()
            {
                return _nukiOpener.verifySecurityPin();
            });

            if(result != Nuki::CmdResult::Success)
            {
                Log->println("Nuki opener PIN is invalid or not set");
                if(result == Nuki::CmdResult::Failed && pinStatus != 2)
                {
                    _preferences->putInt(preference_opener_pin_status, (int)NukiPinState::Invalid);
                }
            }
            else
            {
                Log->println("Nuki opener PIN is valid");
                if(pinStatus != 1)
                {
                    _preferences->putInt(preference_opener_pin_status, (int)NukiPinState::Valid);
                }
            }
        }
        else
        {
            Log->println("Invalid/Unexpected opener config received, ID does not matched saved ID");
            expectedConfig = false;
        }
    }
    else
    {
        Log->println("Invalid/Unexpected opener config received, Config is not valid");
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
            Log->println("Invalid/Unexpected opener advanced config received, Advanced config is not valid");
            expectedConfig = false;
        }
    }

    if(expectedConfig && _nukiConfigValid && _nukiAdvancedConfigValid)
    {
        _retryConfigCount = 0;
        Log->println("Done retrieving opener config and advanced config");
    }
    else
    {
        ++_retryConfigCount;
        Log->println("Invalid/Unexpected opener config and/or advanced config received, retrying in 10 seconds");
        int64_t ts = espMillis();
        _nextConfigUpdateTs = ts + 10000;
    }
}

void NukiOpenerWrapper::updateAuthData(bool retrieved)
{
    if(!isPinValid())
    {
        Log->println("No valid PIN set");
        return;
    }

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;

        result = _nukiRetryHandler->retryComm([&]()
        {
            return _nukiOpener.retrieveLogEntries(0, _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 1, false);
        });

        Log->print("Retrieving log entries: ");
        NukiOpenerHelper::printCommandResult(result);
        if(result == Nuki::CmdResult::Success)
        {
            _waitAuthLogUpdateTs = espMillis() + 5000;
            waitForLogEntries(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));

            std::list<NukiOpener::LogEntry> log;
            _nukiOpener.getLogEntries(&log);

            if(log.size() > _preferences->getInt(preference_authlog_max_entries, 3))
            {
                log.resize(_preferences->getInt(preference_authlog_max_entries, 3));
            }

            log.sort([](const NukiOpener::LogEntry& a, const NukiOpener::LogEntry& b)
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
        std::list<NukiOpener::LogEntry> log;
        _nukiOpener.getLogEntries(&log);

        if(log.size() > _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG))
        {
            log.resize(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
        }

        log.sort([](const NukiOpener::LogEntry& a, const NukiOpener::LogEntry& b)
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

void NukiOpenerWrapper::waitForLogEntries(uint32_t count)
{
    std::list<NukiOpener::LogEntry> log;
    size_t lastSize = 0;
    int64_t startTs = espMillis();
    int64_t changedTs = startTs;

    while(espMillis() - startTs < 5000)
    {
        espDelayAck(100);
        _nukiOpener.getLogEntries(&log);

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

bool NukiOpenerWrapper::retrieveMissingLogEntries(std::list<NukiOpener::LogEntry>& log)
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
        return _nukiOpener.retrieveLogEntries(startIndex, count, 0, false);
    });

    NukiOpenerHelper::printCommandResult(result);

    if(result == Nuki::CmdResult::Success)
    {
        waitForLogEntries(count);

        std::list<NukiOpener::LogEntry> missing;
        _nukiOpener.getLogEntries(&missing);
        Log->print("Missing log entries received: ");
        Log->println(missing.size());
        log.splice(log.begin(), missing);
        log.sort([](const NukiOpener::LogEntry& a, const NukiOpener::LogEntry& b)
        {
            return a.index < b.index;
        });
    }

    return true;
}

void NukiOpenerWrapper::updateKeypad(bool retrieved)
{
    if(!_preferences->getBool(preference_keypad_info_enabled))
    {
        return;
    }

    if(!isPinValid())
    {
        Log->println("No valid Nuki Opener PIN set");
        return;
    }

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;

        result = _nukiRetryHandler->retryComm([&]()
        {
            return _nukiOpener.retrieveKeypadEntries(0, _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
        });

        Log->print("Querying opener keypad: ");
        NukiOpenerHelper::printCommandResult(result);
        if(result == Nuki::CmdResult::Success && _waitKeypadUpdateTs == 0)
        {
            _waitKeypadUpdateTs = espMillis() + 5000;
        }
    }
    else
    {
        std::list<NukiOpener::KeypadEntry> entries;
        _nukiOpener.getKeypadEntries(&entries);
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

        Log->print("Opener keypad codes: ");
        Log->println(entries.size());

        entries.sort([](const NukiOpener::KeypadEntry& a, const NukiOpener::KeypadEntry& b)
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
            _preferences->putUInt(preference_opener_max_keypad_code_count, _maxKeypadCodeCount);
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

void NukiOpenerWrapper::updateTimeControl(bool retrieved)
{
    if(!_preferences->getBool(preference_timecontrol_info_enabled))
    {
        return;
    }

    if(!isPinValid())
    {
        Log->println("No valid Nuki Opener PIN set");
        return;
    }

    if(!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;

        result = _nukiRetryHandler->retryComm([&]()
        {
            return _nukiOpener.retrieveTimeControlEntries();
        });
        Log->print("Querying opener timecontrol: ");
        NukiOpenerHelper::printCommandResult(result);

        if(result == Nuki::CmdResult::Success)
        {
            _waitTimeControlUpdateTs = espMillis() + 5000;
        }
    }
    else
    {
        std::list<NukiOpener::TimeControlEntry> timeControlEntries;
        _nukiOpener.getTimeControlEntries(&timeControlEntries);

        Log->print("Opener timecontrol entries: ");
        Log->println(timeControlEntries.size());

        timeControlEntries.sort([](const NukiOpener::TimeControlEntry& a, const NukiOpener::TimeControlEntry& b)
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
            _preferences->putUInt(preference_opener_max_timecontrol_entry_count, _maxTimeControlEntryCount);
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

void NukiOpenerWrapper::updateAuth(bool retrieved)
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
            return _nukiOpener.retrieveAuthorizationEntries(0, _preferences->getInt(preference_auth_max_entries, MAX_AUTH));
        });
        Log->print("Querying opener authorization: ");
        NukiOpenerHelper::printCommandResult(result);

        if(result == Nuki::CmdResult::Success)
        {
            _waitAuthUpdateTs = espMillis() + 5000;
        }
    }
    else
    {
        std::list<NukiOpener::AuthorizationEntry> authEntries;
        _nukiOpener.getAuthorizationEntries(&authEntries);

        Log->print("Opener authorization entries: ");
        Log->println(authEntries.size());

        authEntries.sort([](const NukiOpener::AuthorizationEntry& a, const NukiOpener::AuthorizationEntry& b)
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
            _preferences->putUInt(preference_opener_max_auth_entry_count, _maxAuthEntryCount);
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

void NukiOpenerWrapper::postponeBleWatchdog()
{
    _disableBleWatchdogTs = espMillis() + 15000;
}

LockActionResult NukiOpenerWrapper::onLockActionReceivedCallback(const char *value)
{
    NukiOpener::LockAction action;

    if(value)
    {
        if(strlen(value) > 0)
        {
            action = NukiOpenerHelper::lockActionToEnum(value);
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

    nukiOpenerPreferences = new Preferences();
    nukiOpenerPreferences->begin("nukihub", true);
    uint32_t aclPrefs[17];
    nukiOpenerPreferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    if((action == NukiOpener::LockAction::ActivateRTO && (int)aclPrefs[9] == 1) || (action == NukiOpener::LockAction::DeactivateRTO && (int)aclPrefs[10] == 1) || (action == NukiOpener::LockAction::ElectricStrikeActuation && (int)aclPrefs[11] == 1) || (action == NukiOpener::LockAction::ActivateCM && (int)aclPrefs[12] == 1) || (action == NukiOpener::LockAction::DeactivateCM && (int)aclPrefs[13] == 1) || (action == NukiOpener::LockAction::FobAction1 && (int)aclPrefs[14] == 1) || (action == NukiOpener::LockAction::FobAction2 && (int)aclPrefs[15] == 1) || (action == NukiOpener::LockAction::FobAction3 && (int)aclPrefs[16] == 1))
    {
        nukiOpenerPreferences->end();
        nukiOpenerInst->_nextLockAction = action;
        return LockActionResult::Success;
    }

    nukiOpenerPreferences->end();
    return LockActionResult::AccessDenied;
}

void NukiOpenerWrapper::onConfigUpdateReceivedCallback(const char *value)
{
    nukiOpenerInst->onConfigUpdateReceived(value);
}

void NukiOpenerWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    nukiOpenerInst->onKeypadCommandReceived(command, id, name, code, enabled);
}

void NukiOpenerWrapper::onKeypadJsonCommandReceivedCallback(const char *value)
{
    nukiOpenerInst->onKeypadJsonCommandReceived(value);
}

void NukiOpenerWrapper::onTimeControlCommandReceivedCallback(const char *value)
{
    nukiOpenerInst->onTimeControlCommandReceived(value);
}

void NukiOpenerWrapper::onAuthCommandReceivedCallback(const char *value)
{
    nukiOpenerInst->onAuthCommandReceived(value);
}

void NukiOpenerWrapper::gpioActionCallback(const GpioAction &action, const int& pin)
{
    switch(action)
    {
    case GpioAction::ElectricStrikeActuation:
        nukiOpenerInst->electricStrikeActuation();
        break;
    case GpioAction::ActivateRTO:
        nukiOpenerInst->activateRTO();
        break;
    case GpioAction::ActivateCM:
        nukiOpenerInst->activateCM();
        break;
    case GpioAction::DeactivateRtoCm:
        nukiOpenerInst->deactivateRtoCm();
        break;
    case GpioAction::DeactivateRTO:
        nukiOpenerInst->deactivateRTO();
        break;
    case GpioAction::DeactivateCM:
        nukiOpenerInst->deactivateCM();
        break;
    }
}

void NukiOpenerWrapper::checkKeypadJsonCommand()
{
    char cmd[KEYPAD_JSON_MAX_LENGTH];

    if(xQueueReceive(_keypadJsonQueue, cmd, 0) != pdTRUE)
    {
        return;
    }

    executeKeypadJsonCommand(cmd);
    postponeBleWatchdog();
}

const NukiOpener::OpenerState &NukiOpenerWrapper::keyTurnerState()
{
    return _keyTurnerState;
}

const bool NukiOpenerWrapper::isPaired() const
{
    return _paired;
}

const bool NukiOpenerWrapper::hasKeypad() const
{
    return (_forceKeypad || _hasKeypad);
}

const BLEAddress NukiOpenerWrapper::getBleAddress() const
{
    return _nukiOpener.getBleAddress();
}

const BleScanner::Scanner *NukiOpenerWrapper::bleScanner()
{
    return _bleScanner;
}

void NukiOpenerWrapper::notify(Nuki::EventType eventType)
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

void NukiOpenerWrapper::readConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiOpener.requestConfig(&_nukiConfig);
    });
    _nukiConfigValid = result == Nuki::CmdResult::Success;

    char resultStr[20];
    NukiOpener::cmdResultToString(result, resultStr);
    Log->print("Opener config result: ");
    Log->print(resultStr);
    Log->print("(");
    Log->print(result);
    Log->println(")");
    postponeBleWatchdog();
}

void NukiOpenerWrapper::readAdvancedConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;

    result = _nukiRetryHandler->retryComm([&]()
    {
        return _nukiOpener.requestAdvancedConfig(&_nukiAdvancedConfig);
    });
    _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;

    char resultStr[20];
    NukiOpener::cmdResultToString(result, resultStr);
    Log->print("Opener advanced config result: ");
    Log->println(resultStr);
    postponeBleWatchdog();
}

const std::string NukiOpenerWrapper::firmwareVersion() const
{
    return _firmwareVersion;
}

NukiOpener::NukiOpener& NukiOpenerWrapper::nukiDevice()
{
    return _nukiOpener;
}

const std::string NukiOpenerWrapper::hardwareVersion() const
{
    return _hardwareVersion;
}

void NukiOpenerWrapper::disableWatchdog()
{
    _restartBeaconTimeout = -1;
}

void NukiOpenerWrapper::updateGpioOutputs()
{
    using namespace NukiOpener;

    const auto& pinConfiguration = _gpio->pinConfiguration();

    const LockState& lockState = _keyTurnerState.lockState;

    bool rtoActive = _keyTurnerState.lockState == LockState::RTOactive;
    bool cmActive = _keyTurnerState.nukiState == State::ContinuousMode;

    for(const auto& entry : pinConfiguration)
    {
        switch(entry.role)
        {
        case PinRole::OutputHighRtoActive:
            _gpio->setPinOutput(entry.pin, rtoActive ? HIGH : LOW);
            break;
        case PinRole::OutputHighCmActive:
            _gpio->setPinOutput(entry.pin, cmActive ? HIGH : LOW);
            break;
        case PinRole::OutputHighRtoOrCmActive:
            _gpio->setPinOutput(entry.pin, rtoActive || cmActive ? HIGH : LOW);
            break;
        }
    }
}

bool NukiOpenerWrapper::updateTime()
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
        return _nukiOpener.updateTime(nukiTime);
    });

    char resultStr[15] = {0};
    NukiOpener::cmdResultToString(cmdResult, resultStr);

    Log->print("Opener time update result: ");
    Log->println(resultStr);
    return true;
}

void NukiOpenerWrapper::publishKeypadCache()
{
    std::list<NukiOpener::KeypadEntry> entries = _keypadEntriesCache;
    entries.sort([](const NukiOpener::KeypadEntry& a, const NukiOpener::KeypadEntry& b)
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
