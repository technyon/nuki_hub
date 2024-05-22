#include "NetworkOpener.h"
#include "Arduino.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include <ArduinoJson.h>

NetworkOpener::NetworkOpener(Network* network, Preferences* preferences, char* buffer, size_t bufferSize)
        : _preferences(preferences),
          _network(network),
          _buffer(buffer),
          _bufferSize(bufferSize)
{
    memset(_authName, 0, sizeof(_authName));
    _authName[0] = '\0';

    _network->registerMqttReceiver(this);
}

void NetworkOpener::initialize()
{
    String mqttPath = _preferences->getString(preference_mqtt_opener_path);
    if(mqttPath.length() > 0)
    {
        size_t len = mqttPath.length();
        for(int i=0; i < len; i++)
        {
            _mqttPath[i] = mqttPath.charAt(i);
        }
    }
    else
    {
        strcpy(_mqttPath, "nukiopener");
        _preferences->putString(preference_mqtt_opener_path, _mqttPath);
    }

    _haEnabled = _preferences->getString(preference_mqtt_hass_discovery) != "";

    _network->initTopic(_mqttPath, mqtt_topic_lock_action, "--");
    _network->subscribe(_mqttPath, mqtt_topic_lock_action);
    _network->initTopic(_mqttPath, mqtt_topic_config_action, "--");
    _network->subscribe(_mqttPath, mqtt_topic_config_action);

    _network->initTopic(_mqttPath, mqtt_topic_query_config, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_lockstate, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_battery, "0");
    _network->initTopic(_mqttPath, mqtt_topic_lock_binary_ring, "standby");
    _network->subscribe(_mqttPath, mqtt_topic_query_config);
    _network->subscribe(_mqttPath, mqtt_topic_query_lockstate);
    _network->subscribe(_mqttPath, mqtt_topic_query_battery);

    if(_preferences->getBool(preference_keypad_control_enabled))
    {
        _network->subscribe(_mqttPath, mqtt_topic_keypad_command_action);
        _network->subscribe(_mqttPath, mqtt_topic_keypad_command_id);
        _network->subscribe(_mqttPath, mqtt_topic_keypad_command_name);
        _network->subscribe(_mqttPath, mqtt_topic_keypad_command_code);
        _network->subscribe(_mqttPath, mqtt_topic_keypad_command_enabled);
        _network->subscribe(_mqttPath, mqtt_topic_query_keypad);
        _network->subscribe(_mqttPath, mqtt_topic_keypad_json_action);
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_action, "--");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_id, "0");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_name, "--");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_code, "000000");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_enabled, "1");
        _network->initTopic(_mqttPath, mqtt_topic_query_keypad, "0");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_json_action, "--");
    }

    if(_preferences->getBool(preference_timecontrol_control_enabled))
    {
        _network->subscribe(_mqttPath, mqtt_topic_timecontrol_action);
        _network->initTopic(_mqttPath, mqtt_topic_timecontrol_action, "--");
    }

    _network->addReconnectedCallback([&]()
     {
         _reconnected = true;
     });
}

void NetworkOpener::update()
{
    if(_resetRingStateTs != 0 && millis() >= _resetRingStateTs)
    {
        _resetRingStateTs = 0;
        publishString(mqtt_topic_lock_binary_ring, "standby");
    }
}

void NetworkOpener::onMqttDataReceived(const char* topic, byte* payload, const unsigned int length)
{
    char* value = (char*)payload;

    if(comparePrefixedPath(topic, mqtt_topic_lock_action))
    {
        if(strcmp(value, "") == 0 ||
           strcmp(value, "--") == 0 ||
           strcmp(value, "ack") == 0 ||
           strcmp(value, "unknown_action") == 0 ||
           strcmp(value, "denied") == 0 ||
           strcmp(value, "error") == 0) return;

        Log->print(F("Lock action received: "));
        Log->println(value);
        LockActionResult lockActionResult = LockActionResult::Failed;
        if(_lockActionReceivedCallback != NULL)
        {
            lockActionResult = _lockActionReceivedCallback(value);
        }

        switch(lockActionResult)
        {
            case LockActionResult::Success:
                publishString(mqtt_topic_lock_action, "ack");
                break;
            case LockActionResult::UnknownAction:
                publishString(mqtt_topic_lock_action, "unknown_action");
                break;
            case LockActionResult::AccessDenied:
                publishString(mqtt_topic_lock_action, "denied");
                break;
            case LockActionResult::Failed:
                publishString(mqtt_topic_lock_action, "error");
                break;
        }
    }

    if(comparePrefixedPath(topic, mqtt_topic_keypad_command_action))
    {
        if(_keypadCommandReceivedReceivedCallback != nullptr)
        {
            if(strcmp(value, "--") == 0) return;

            _keypadCommandReceivedReceivedCallback(value, _keypadCommandId, _keypadCommandName, _keypadCommandCode, _keypadCommandEnabled);

            _keypadCommandId = 0;
            _keypadCommandName = "--";
            _keypadCommandCode = "000000";
            _keypadCommandEnabled = 1;

            if(strcmp(value, "--") != 0)
            {
                publishString(mqtt_topic_keypad_command_action, "--");
            }
            publishInt(mqtt_topic_keypad_command_id, _keypadCommandId);
            publishString(mqtt_topic_keypad_command_name, _keypadCommandName);
            publishString(mqtt_topic_keypad_command_code, _keypadCommandCode);
            publishInt(mqtt_topic_keypad_command_enabled, _keypadCommandEnabled);
        }
    }
    else if(comparePrefixedPath(topic, mqtt_topic_keypad_command_id))
    {
        _keypadCommandId = atoi(value);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_keypad_command_name))
    {
        _keypadCommandName = value;
    }
    else if(comparePrefixedPath(topic, mqtt_topic_keypad_command_code))
    {
        _keypadCommandCode = value;
    }
    else if(comparePrefixedPath(topic, mqtt_topic_keypad_command_enabled))
    {
        _keypadCommandEnabled = atoi(value);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_query_config) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_CONFIG;
        publishString(mqtt_topic_query_config, "0");
    }
    else if(comparePrefixedPath(topic, mqtt_topic_query_lockstate) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_LOCKSTATE;
        publishString(mqtt_topic_query_lockstate, "0");
    }
    else if(comparePrefixedPath(topic, mqtt_topic_query_keypad) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_KEYPAD;
        publishString(mqtt_topic_query_keypad, "0");
    }
    else if(comparePrefixedPath(topic, mqtt_topic_query_battery) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_BATTERY;
        publishString(mqtt_topic_query_battery, "0");
    }

    if(comparePrefixedPath(topic, mqtt_topic_config_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "--") == 0) return;

        if(_configUpdateReceivedCallback != NULL)
        {
            _configUpdateReceivedCallback(value);
        }

        publishString(mqtt_topic_config_action, "--");
    }

    if(comparePrefixedPath(topic, mqtt_topic_keypad_json_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "--") == 0) return;

        if(_keypadJsonCommandReceivedReceivedCallback != NULL)
        {
            _keypadJsonCommandReceivedReceivedCallback(value);
        }

        publishString(mqtt_topic_keypad_json_action, "--");
    }

    if(comparePrefixedPath(topic, mqtt_topic_timecontrol_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "--") == 0) return;

        if(_timeControlCommandReceivedReceivedCallback != NULL)
        {
            _timeControlCommandReceivedReceivedCallback(value);
        }

        publishString(mqtt_topic_timecontrol_action, "--");
    }
}

void NetworkOpener::publishKeyTurnerState(const NukiOpener::OpenerState& keyTurnerState, const NukiOpener::OpenerState& lastKeyTurnerState)
{
    _currentLockState = keyTurnerState.lockState;

    char str[50];
    memset(&str, 0, sizeof(str));

    JsonDocument json;

    lockstateToString(keyTurnerState.lockState, str);

    if((_firstTunerStatePublish || keyTurnerState.lockState != lastKeyTurnerState.lockState || keyTurnerState.nukiState != lastKeyTurnerState.nukiState) && keyTurnerState.lockState != NukiOpener::LockState::Undefined)
    {
        publishString(mqtt_topic_lock_state, str);

        if(_haEnabled)
        {
            publishState(keyTurnerState);
        }
    }

    json["lock_state"] = str;

    if(keyTurnerState.nukiState == NukiOpener::State::ContinuousMode)
    {
        publishString(mqtt_topic_lock_continuous_mode, "on");
        json["continuous_mode"] = 1;
    } else {
        publishString(mqtt_topic_lock_continuous_mode, "off");
        json["continuous_mode"] = 0;
    }

    memset(&str, 0, sizeof(str));
    triggerToString(keyTurnerState.trigger, str);

    if(_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
    {
        publishString(mqtt_topic_lock_trigger, str);
    }

    json["trigger"] = str;

    json["ringToOpenTimer"] = keyTurnerState.ringToOpenTimer;
    char curTime[20];
    sprintf(curTime, "%04d-%02d-%02d %02d:%02d:%02d", keyTurnerState.currentTimeYear, keyTurnerState.currentTimeMonth, keyTurnerState.currentTimeDay, keyTurnerState.currentTimeHour, keyTurnerState.currentTimeMinute, keyTurnerState.currentTimeSecond);
    json["currentTime"] = curTime;
    json["timeZoneOffset"] = keyTurnerState.timeZoneOffset;

    lockactionToString(keyTurnerState.lastLockAction, str);

    if(_firstTunerStatePublish || keyTurnerState.lastLockAction != lastKeyTurnerState.lastLockAction)
    {
        publishString(mqtt_topic_lock_last_lock_action, str);
    }

    json["last_lock_action"] = str;

    memset(&str, 0, sizeof(str));
    triggerToString(keyTurnerState.lastLockActionTrigger, str);
    json["last_lock_action_trigger"] = str;

    memset(&str, 0, sizeof(str));
    completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);

    if(_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        publishString(mqtt_topic_lock_completionStatus, str);
    }

    json["lock_completion_status"] = str;

    memset(&str, 0, sizeof(str));
    NukiOpener::doorSensorStateToString(keyTurnerState.doorSensorState, str);

    if(_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
    {
        publishString(mqtt_topic_lock_door_sensor_state, str);
    }

    json["door_sensor_state"] = str;

    if(_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState)
    {
        bool critical = (keyTurnerState.criticalBatteryState & 0b00000001) > 0;
        publishBool(mqtt_topic_battery_critical, critical);
    }

    json["auth_id"] = _authId;
    json["auth_name"] = _authName;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_lock_json, _buffer);

    _firstTunerStatePublish = false;
}

void NetworkOpener::publishRing(const bool locked)
{
    if(locked)
    {
        publishString(mqtt_topic_lock_ring, "ringlocked");
    }
    else
    {
        publishString(mqtt_topic_lock_ring, "ring");
    }

    publishString(mqtt_topic_lock_binary_ring, "ring");
    _resetRingStateTs = millis() + 2000;
}

void NetworkOpener::publishState(NukiOpener::OpenerState lockState)
{
    if(lockState.nukiState == NukiOpener::State::ContinuousMode)
    {
        publishString(mqtt_topic_lock_ha_state, "unlocked");
        publishString(mqtt_topic_lock_binary_state, "unlocked");
    }
    else
    {
        switch (lockState.lockState)
        {
            case NukiOpener::LockState::Locked:
                publishString(mqtt_topic_lock_ha_state, "locked");
                publishString(mqtt_topic_lock_binary_state, "locked");
                break;
            case NukiOpener::LockState::RTOactive:
            case NukiOpener::LockState::Open:
                publishString(mqtt_topic_lock_ha_state, "unlocked");
                publishString(mqtt_topic_lock_binary_state, "unlocked");
                break;
            case NukiOpener::LockState::Opening:
                publishString(mqtt_topic_lock_ha_state, "unlocking");
                publishString(mqtt_topic_lock_binary_state, "unlocked");
                break;
            case NukiOpener::LockState::Undefined:
            case NukiOpener::LockState::Uncalibrated:
                publishString(mqtt_topic_lock_ha_state, "jammed");
                break;
            default:
                break;
        }
    }
}

void NetworkOpener::publishAuthorizationInfo(const std::list<NukiOpener::LogEntry>& logEntries)
{
    char str[50];
    char authName[33];
    bool authFound = false;

    JsonDocument json;

    int i = 5;
    for(const auto& log : logEntries)
    {
        if(i <= 0)
        {
            break;
        }
        --i;

        memset(authName, 0, sizeof(authName));
        authName[0] = '\0';

        if((log.loggingType == NukiOpener::LoggingType::LockAction || log.loggingType == NukiOpener::LoggingType::KeypadAction))
        {
            int sizeName = sizeof(log.name);
            memcpy(authName, log.name, sizeName);
            if(authName[sizeName - 1] != '\0') authName[sizeName] = '\0';

            if(!authFound)
            {
                authFound = true;
                _authFound = true;
                _authId = log.authId;
                memset(_authName, 0, sizeof(_authName));
                memcpy(_authName, authName, sizeof(authName));
            }
        }

        auto entry = json.add<JsonVariant>();

        entry["index"] = log.index;
        entry["authorizationId"] = log.authId;
        entry["authorizationName"] = _authName;
        entry["timeYear"] = log.timeStampYear;
        entry["timeMonth"] = log.timeStampMonth;
        entry["timeDay"] = log.timeStampDay;
        entry["timeHour"] = log.timeStampHour;
        entry["timeMinute"] = log.timeStampMinute;
        entry["timeSecond"] = log.timeStampSecond;

        memset(str, 0, sizeof(str));
        loggingTypeToString(log.loggingType, str);
        entry["type"] = str;

        switch(log.loggingType)
        {
            case NukiOpener::LoggingType::LockAction:
                memset(str, 0, sizeof(str));
                NukiOpener::lockactionToString((NukiOpener::LockAction)log.data[0], str);
                entry["action"] = str;

                memset(str, 0, sizeof(str));
                NukiOpener::triggerToString((NukiOpener::Trigger)log.data[1], str);
                entry["trigger"] = str;

                memset(str, 0, sizeof(str));
                NukiOpener::completionStatusToString((NukiOpener::CompletionStatus)log.data[3], str);
                entry["completionStatus"] = str;
                break;
            case NukiOpener::LoggingType::KeypadAction:
                memset(str, 0, sizeof(str));
                NukiOpener::lockactionToString((NukiOpener::LockAction)log.data[0], str);
                entry["action"] = str;

                memset(str, 0, sizeof(str));
                NukiOpener::completionStatusToString((NukiOpener::CompletionStatus)log.data[2], str);
                entry["completionStatus"] = str;
                break;
            case NukiOpener::LoggingType::DoorbellRecognition:
                switch(log.data[0] & 3)
                {
                    case 0:
                        entry["mode"] = "None";
                        break;
                    case 1:
                        entry["mode"] = "RTO";
                        break;
                    case 2:
                        entry["mode"] = "CM";
                        break;
                    default:
                        entry["mode"] = "Unknown";
                        break;
                }

                switch(log.data[1])
                {
                    case 0:
                        entry["source"] = "Doorbell";
                        break;
                    case 1:
                        entry["source"] = "Timecontrol";
                        break;
                    case 2:
                        entry["source"] = "App";
                        break;
                    case 3:
                        entry["source"] = "Button";
                        break;
                    case 4:
                        entry["source"] = "Fob";
                        break;
                    case 5:
                        entry["source"] = "Bridge";
                        break;
                    case 6:
                        entry["source"] = "Keypad";
                        break;
                    default:
                        entry["source"] = "Unknown";
                        break;                }

                entry["geofence"] = log.data[2] == 1 ? "active" : "inactive";
                entry["doorbellSuppression"] = log.data[3] == 1 ? "active" : "inactive";
                entry["completionStatus"] = str;

                break;
        }
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_lock_log, _buffer);

    if(authFound)
    {
        publishUInt(mqtt_topic_lock_auth_id, _authId);
        publishString(mqtt_topic_lock_auth_name, _authName);
    }
}

void NetworkOpener::logactionCompletionStatusToString(uint8_t value, char* out)
{
    switch (value)
    {
        case 0x00:
            strcpy(out, "success");
            break;
        case 0x02:
            strcpy(out, "cancelled");
            break;
        case 0x03:
            strcpy(out, "tooRecent");
            break;
        case 0x04:
            strcpy(out, "busy");
            break;
        case 0x08:
            strcpy(out, "incomplete");
            break;
        case 0xfe:
            strcpy(out, "otherError");
            break;
        case 0xff:
            strcpy(out, "unknown");
            break;
        default:
            strcpy(out, "undefined");
            break;
    }
}

void NetworkOpener::clearAuthorizationInfo()
{
    publishString(mqtt_topic_lock_log, "--");
    publishUInt(mqtt_topic_lock_auth_id, 0);
    publishString(mqtt_topic_lock_auth_name, "--");
}

void NetworkOpener::publishCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_lock_action_command_result, resultStr);
}

void NetworkOpener::publishLockstateCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_query_lockstate_command_result, resultStr);
}

void NetworkOpener::publishBatteryReport(const NukiOpener::BatteryReport& batteryReport)
{
    publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0);
}

void NetworkOpener::publishConfig(const NukiOpener::Config &config)
{
    char str[50];
    char curTime[20];
    sprintf(curTime, "%04d-%02d-%02d %02d:%02d:%02d", config.currentTimeYear, config.currentTimeMonth, config.currentTimeDay, config.currentTimeHour, config.currentTimeMinute, config.currentTimeSecond);
    char uidString[20];
    itoa(config.nukiId, uidString, 16);

    JsonDocument json;

    json["nukiID"] = uidString;
    json["name"] = config.name;
    //json["latitude"] = config.latitude;
    //json["longitude"] = config.longitude;
    memset(str, 0, sizeof(str));
    capabilitiesToString(config.capabilities, str);
    json["capabilities"] = str;
    json["pairingEnabled"] = config.pairingEnabled;
    json["buttonEnabled"] = config.buttonEnabled;
    json["ledFlashEnabled"] = config.ledFlashEnabled;
    json["currentTime"] = curTime;
    json["timeZoneOffset"] = config.timeZoneOffset;
    json["dstMode"] = config.dstMode;
    json["hasFob"] = config.hasFob;
    memset(str, 0, sizeof(str));
    fobActionToString(config.fobAction1, str);
    json["fobAction1"] = str;
    memset(str, 0, sizeof(str));
    fobActionToString(config.fobAction2, str);
    json["fobAction2"] = str;
    memset(str, 0, sizeof(str));
    fobActionToString(config.fobAction3, str);
    json["fobAction3"] = str;
    memset(str, 0, sizeof(str));
    operatingModeToString(config.operatingMode, str);
    json["operatingMode"] = str;
    memset(str, 0, sizeof(str));
    _network->advertisingModeToString(config.advertisingMode, str);
    json["advertisingMode"] = str;
    json["hasKeypad"] = config.hasKeypad;
    json["hasKeypadV2"] = config.hasKeypadV2;
    json["firmwareVersion"] = std::to_string(config.firmwareVersion[0]) + "." + std::to_string(config.firmwareVersion[1]) + "." + std::to_string(config.firmwareVersion[2]);
    json["hardwareRevision"] = std::to_string(config.hardwareRevision[0]) + "." + std::to_string(config.hardwareRevision[1]);
    memset(str, 0, sizeof(str));
    _network->timeZoneIdToString(config.timeZoneId, str);
    json["timeZone"] = str;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_config_basic_json, _buffer);
    publishBool(mqtt_topic_config_button_enabled, config.buttonEnabled == 1);
    publishBool(mqtt_topic_config_led_enabled, config.ledFlashEnabled == 1);
    publishString(mqtt_topic_info_firmware_version, std::to_string(config.firmwareVersion[0]) + "." + std::to_string(config.firmwareVersion[1]) + "." + std::to_string(config.firmwareVersion[2]));
    publishString(mqtt_topic_info_hardware_version, std::to_string(config.hardwareRevision[0]) + "." + std::to_string(config.hardwareRevision[1]));
}

void NetworkOpener::publishAdvancedConfig(const NukiOpener::AdvancedConfig &config)
{
    char str[50];

    JsonDocument json;

    json["intercomID"] = config.intercomID;
    json["busModeSwitch"] = config.busModeSwitch;
    json["shortCircuitDuration"] = config.shortCircuitDuration;
    json["electricStrikeDelay"] = config.electricStrikeDelay;
    json["randomElectricStrikeDelay"] = config.randomElectricStrikeDelay;
    json["electricStrikeDuration"] = config.electricStrikeDuration;
    json["disableRtoAfterRing"] = config.disableRtoAfterRing;
    json["rtoTimeout"] = config.rtoTimeout;
    memset(str, 0, sizeof(str));
    doorbellSuppressionToString(config.doorbellSuppression, str);
    json["doorbellSuppression"] = str;
    json["doorbellSuppressionDuration"] = config.doorbellSuppressionDuration;
    memset(str, 0, sizeof(str));
    soundToString(config.soundRing, str);
    json["soundRing"] = str;
    memset(str, 0, sizeof(str));
    soundToString(config.soundOpen, str);
    json["soundOpen"] = str;
    memset(str, 0, sizeof(str));
    soundToString(config.soundRto, str);
    json["soundRto"] = str;
    memset(str, 0, sizeof(str));
    soundToString(config.soundCm, str);
    json["soundCm"] = str;
    json["soundConfirmation"] = config.soundConfirmation;
    json["soundLevel"] = config.soundLevel;
    memset(str, 0, sizeof(str));
    buttonPressActionToString(config.singleButtonPressAction, str);
    json["singleButtonPressAction"] = str;
    memset(str, 0, sizeof(str));
    buttonPressActionToString(config.doubleButtonPressAction, str);
    json["doubleButtonPressAction"] = str;
    memset(str, 0, sizeof(str));
    _network->batteryTypeToString(config.batteryType, str);
    json["batteryType"] = str;
    json["automaticBatteryTypeDetection"] = config.automaticBatteryTypeDetection;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_config_advanced_json, _buffer);
    publishUInt(mqtt_topic_config_sound_level, config.soundLevel);
}

void NetworkOpener::publishRssi(const int &rssi)
{
    publishInt(mqtt_topic_lock_rssi, rssi);
}

void NetworkOpener::publishRetry(const std::string& message)
{
    publishString(mqtt_topic_lock_retry, message);
}

void NetworkOpener::publishBleAddress(const std::string &address)
{
    publishString(mqtt_topic_lock_address, address);
}

void NetworkOpener::publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction)
{
    String availabilityTopic = _preferences->getString("mqttpath");
    availabilityTopic.concat("/maintenance/mqttConnectionState");

    _network->publishHASSConfig(deviceType, baseTopic, name, uidString, availabilityTopic.c_str(), false, lockAction, unlockAction, openAction);
    _network->publishHASSConfigAdditionalOpenerEntities(deviceType, baseTopic, name, uidString);
}

void NetworkOpener::removeHASSConfig(char* uidString)
{
    _network->removeHASSConfig(uidString);
}

void NetworkOpener::publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount)
{
    uint index = 0;

    JsonDocument json;

    for(const auto& entry : entries)
    {
        String basePath = mqtt_topic_keypad;
        basePath.concat("/code_");
        basePath.concat(std::to_string(index).c_str());
        publishKeypadEntry(basePath, entry);
      
        auto jsonEntry = json.add<JsonVariant>();

        jsonEntry["codeId"] = entry.codeId;
        jsonEntry["enabled"] = entry.enabled;
        jsonEntry["name"] = entry.name;
        char createdDT[20];
        sprintf(createdDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.dateCreatedYear, entry.dateCreatedMonth, entry.dateCreatedDay, entry.dateCreatedHour, entry.dateCreatedMin, entry.dateCreatedSec);
        jsonEntry["dateCreated"] = createdDT;
        jsonEntry["lockCount"] = entry.lockCount;
        char lastActiveDT[20];
        sprintf(lastActiveDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.dateLastActiveYear, entry.dateLastActiveMonth, entry.dateLastActiveDay, entry.dateLastActiveHour, entry.dateLastActiveMin, entry.dateLastActiveSec);
        jsonEntry["dateLastActive"] = lastActiveDT;
        jsonEntry["timeLimited"] = entry.timeLimited;
        char allowedFromDT[20];
        sprintf(allowedFromDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.allowedFromYear, entry.allowedFromMonth, entry.allowedFromDay, entry.allowedFromHour, entry.allowedFromMin, entry.allowedFromSec);
        jsonEntry["allowedFrom"] = allowedFromDT;
        char allowedUntilDT[20];
        sprintf(allowedUntilDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.allowedUntilYear, entry.allowedUntilMonth, entry.allowedUntilDay, entry.allowedUntilHour, entry.allowedUntilMin, entry.allowedUntilSec);
        jsonEntry["allowedUntil"] = allowedUntilDT;

        uint8_t allowedWeekdaysInt = entry.allowedWeekdays;
        JsonArray weekdays = jsonEntry["allowedWeekdays"].to<JsonArray>();

        while(allowedWeekdaysInt > 0) {
            if(allowedWeekdaysInt >= 64)
            {
                weekdays.add("mon");
                allowedWeekdaysInt -= 64;
                continue;
            }
            if(allowedWeekdaysInt >= 32)
            {
                weekdays.add("tue");
                allowedWeekdaysInt -= 32;
                continue;
            }
            if(allowedWeekdaysInt >= 16)
            {
                weekdays.add("wed");
                allowedWeekdaysInt -= 16;
                continue;
            }
            if(allowedWeekdaysInt >= 8)
            {
                weekdays.add("thu");
                allowedWeekdaysInt -= 8;
                continue;
            }
            if(allowedWeekdaysInt >= 4)
            {
                weekdays.add("fri");
                allowedWeekdaysInt -= 4;
                continue;
            }
            if(allowedWeekdaysInt >= 2)
            {
                weekdays.add("sat");
                allowedWeekdaysInt -= 2;
                continue;
            }
            if(allowedWeekdaysInt >= 1)
            {
                weekdays.add("sun");
                allowedWeekdaysInt -= 1;
                continue;
            }
        }

        char allowedFromTimeT[5];
        sprintf(allowedFromTimeT, "%02d:%02d", entry.allowedFromTimeHour, entry.allowedFromTimeMin);
        jsonEntry["allowedFromTime"] = allowedFromTimeT;
        char allowedUntilTimeT[5];
        sprintf(allowedUntilTimeT, "%02d:%02d", entry.allowedUntilTimeHour, entry.allowedUntilTimeMin);
        jsonEntry["allowedUntilTime"] = allowedUntilTimeT;

        ++index;
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_keypad_json, _buffer);

    while(index < maxKeypadCodeCount)
    {
        NukiLock::KeypadEntry entry;
        memset(&entry, 0, sizeof(entry));
        String basePath = mqtt_topic_keypad;
        basePath.concat("/code_");
        basePath.concat(std::to_string(index).c_str());
        publishKeypadEntry(basePath, entry);

        ++index;
    }
}

void NetworkOpener::publishTimeControl(const std::list<NukiOpener::TimeControlEntry>& timeControlEntries)
{
    char str[50];
    JsonDocument json;

    for(const auto& entry : timeControlEntries)
    {
        auto jsonEntry = json.add<JsonVariant>();

        jsonEntry["entryId"] = entry.entryId;
        jsonEntry["enabled"] = entry.enabled;
        uint8_t weekdaysInt = entry.weekdays;
        JsonArray weekdays = jsonEntry["weekdays"].to<JsonArray>();

        while(weekdaysInt > 0) {
            if(weekdaysInt >= 64)
            {
                weekdays.add("mon");
                weekdaysInt -= 64;
                continue;
            }
            if(weekdaysInt >= 32)
            {
                weekdays.add("tue");
                weekdaysInt -= 32;
                continue;
            }
            if(weekdaysInt >= 16)
            {
                weekdays.add("wed");
                weekdaysInt -= 16;
                continue;
            }
            if(weekdaysInt >= 8)
            {
                weekdays.add("thu");
                weekdaysInt -= 8;
                continue;
            }
            if(weekdaysInt >= 4)
            {
                weekdays.add("fri");
                weekdaysInt -= 4;
                continue;
            }
            if(weekdaysInt >= 2)
            {
                weekdays.add("sat");
                weekdaysInt -= 2;
                continue;
            }
            if(weekdaysInt >= 1)
            {
                weekdays.add("sun");
                weekdaysInt -= 1;
                continue;
            }
        }

        char timeT[5];
        sprintf(timeT, "%02d:%02d", entry.timeHour, entry.timeMin);
        jsonEntry["time"] = timeT;

        memset(str, 0, sizeof(str));
        NukiOpener::lockactionToString(entry.lockAction, str);
        jsonEntry["lockAction"] = str;
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_timecontrol_json, _buffer);
}

void NetworkOpener::publishConfigCommandResult(const char* result)
{
    publishString(mqtt_topic_config_action_command_result, result);
}

void NetworkOpener::publishKeypadCommandResult(const char* result)
{
    publishString(mqtt_topic_keypad_command_result, result);
}

void NetworkOpener::publishKeypadJsonCommandResult(const char* result)
{
    publishString(mqtt_topic_keypad_json_command_result, result);
}

void NetworkOpener::publishTimeControlCommandResult(const char* result)
{
    publishString(mqtt_topic_timecontrol_command_result, result);
}

void NetworkOpener::publishStatusUpdated(const bool statusUpdated)
{
    publishBool(mqtt_topic_lock_status_updated, statusUpdated);
}

void NetworkOpener::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NetworkOpener::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void NetworkOpener::setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled))
{
    _keypadCommandReceivedReceivedCallback = keypadCommandReceivedReceivedCallback;
}

void NetworkOpener::setKeypadJsonCommandReceivedCallback(void (*keypadJsonCommandReceivedReceivedCallback)(const char *))
{
    _keypadJsonCommandReceivedReceivedCallback = keypadJsonCommandReceivedReceivedCallback;
}

void NetworkOpener::setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char *))
{
    _timeControlCommandReceivedReceivedCallback = timeControlCommandReceivedReceivedCallback;
}

void NetworkOpener::publishFloat(const char *topic, const float value, const uint8_t precision)
{
    _network->publishFloat(_mqttPath, topic, value, precision);
}

void NetworkOpener::publishInt(const char *topic, const int value)
{
    _network->publishInt(_mqttPath, topic, value);
}

void NetworkOpener::publishUInt(const char *topic, const unsigned int value)
{
    _network->publishUInt(_mqttPath, topic, value);
}

void NetworkOpener::publishBool(const char *topic, const bool value)
{
    _network->publishBool(_mqttPath, topic, value);
}

void NetworkOpener::publishString(const char *topic, const String &value)
{
    char str[value.length() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.begin(), value.length());
    publishString(topic, str);
}

void NetworkOpener::publishString(const char *topic, const std::string &value)
{
    char str[value.size() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.data(), value.length());
    publishString(topic, str);
}

void NetworkOpener::publishString(const char* topic, const char* value)
{
    _network->publishString(_mqttPath, topic, value);
}

void NetworkOpener::publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry)
{
    char codeName[sizeof(entry.name) + 1];
    memset(codeName, 0, sizeof(codeName));
    memcpy(codeName, entry.name, sizeof(entry.name));

    publishInt(concat(topic, "/id").c_str(), entry.codeId);
    publishBool(concat(topic, "/enabled").c_str(), entry.enabled);
    publishString(concat(topic, "/name").c_str(), codeName);
    publishInt(concat(topic, "/createdYear").c_str(), entry.dateCreatedYear);
    publishInt(concat(topic, "/createdMonth").c_str(), entry.dateCreatedMonth);
    publishInt(concat(topic, "/createdDay").c_str(), entry.dateCreatedDay);
    publishInt(concat(topic, "/createdHour").c_str(), entry.dateCreatedHour);
    publishInt(concat(topic, "/createdMin").c_str(), entry.dateCreatedMin);
    publishInt(concat(topic, "/createdSec").c_str(), entry.dateCreatedSec);
    publishInt(concat(topic, "/lockCount").c_str(), entry.lockCount);
}

void NetworkOpener::buildMqttPath(const char* path, char* outPath)
{
    int offset = 0;
    for(const char& c : _mqttPath)
    {
        if(c == 0x00)
        {
            break;
        }
        outPath[offset] = c;
        ++offset;
    }
    int i=0;
    while(path[i] != 0x00)
    {
        outPath[offset] = path[i];
        ++i;
        ++offset;
    }
    outPath[offset] = 0x00;
}

void NetworkOpener::subscribe(const char *path)
{
    char prefixedPath[500];
    buildMqttPath(path, prefixedPath);
    _network->subscribe(prefixedPath, MQTT_QOS_LEVEL);
}

bool NetworkOpener::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);

    return strcmp(fullPath, prefixedPath) == 0;
}

String NetworkOpener::concat(String a, String b)
{
    String c = a;
    c.concat(b);
    return c;
}

bool NetworkOpener::reconnected()
{
    bool r = _reconnected;
    _reconnected = false;
    return r;
}

uint8_t NetworkOpener::queryCommands()
{
    uint8_t qc = _queryCommands;
    _queryCommands = 0;
    return qc;
}

void NetworkOpener::buttonPressActionToString(const NukiOpener::ButtonPressAction btnPressAction, char* str) {
  switch (btnPressAction) {
    case NukiOpener::ButtonPressAction::NoAction:
      strcpy(str, "No Action");
      break;
    case NukiOpener::ButtonPressAction::ToggleRTO:
      strcpy(str, "Toggle RTO");
      break;
    case NukiOpener::ButtonPressAction::ActivateRTO:
      strcpy(str, "Activate RTO");
      break;
    case NukiOpener::ButtonPressAction::DeactivateRTO:
      strcpy(str, "Deactivate RTO");
      break;
    case NukiOpener::ButtonPressAction::ToggleCM:
      strcpy(str, "Toggle CM");
      break;
    case NukiOpener::ButtonPressAction::ActivateCM:
      strcpy(str, "Activate CM");
      break;
    case NukiOpener::ButtonPressAction::DectivateCM:
      strcpy(str, "Deactivate CM");
      break;
    case NukiOpener::ButtonPressAction::Open:
      strcpy(str, "Open");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}

void NetworkOpener::fobActionToString(const int fobact, char* str) {
  switch (fobact) {
    case 0:
      strcpy(str, "No Action");
      break;
    case 1:
      strcpy(str, "Toggle RTO");
      break;
    case 2:
      strcpy(str, "Activate RTO");
      break;
    case 3:
      strcpy(str, "Deactivate RTO");
      break;
    case 7:
      strcpy(str, "Open");
      break;
    case 8:
      strcpy(str, "Ring");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}

void NetworkOpener::capabilitiesToString(const int capabilities, char* str) {
  switch (capabilities) {
    case 0:
      strcpy(str, "Door opener");
      break;
    case 1:
      strcpy(str, "Both");
      break;
    case 2:
      strcpy(str, "RTO");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}

void NetworkOpener::operatingModeToString(const int opmode, char* str) {
  switch (opmode) {
    case 0:
      strcpy(str, "Generic door opener");
      break;
    case 1:
      strcpy(str, "Analogue intercom");
      break;
    case 2:
      strcpy(str, "Digital intercom");
      break;
    case 3:
      strcpy(str, "Siedle");
      break;
    case 4:
      strcpy(str, "TCS");
      break;
    case 5:
      strcpy(str, "Bticino");
      break;
    case 6:
      strcpy(str, "Siedle HTS");
      break;
    case 7:
      strcpy(str, "STR");
      break;
    case 8:
      strcpy(str, "Ritto");
      break;
    case 9:
      strcpy(str, "Fermax");
      break;
    case 10:
      strcpy(str, "Comelit");
      break;
    case 11:
      strcpy(str, "Urmet BiBus");
      break;
    case 12:
      strcpy(str, "Urmet 2Voice");
      break;
    case 13:
      strcpy(str, "Golmar");
      break;
    case 14:
      strcpy(str, "SKS");
      break;
    case 15:
      strcpy(str, "Spare");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}

void NetworkOpener::doorbellSuppressionToString(const int dbsupr, char* str) {
  switch (dbsupr) {
    case 0:
      strcpy(str, "Off");
      break;
    case 1:
      strcpy(str, "CM");
      break;
    case 2:
      strcpy(str, "RTO");
      break;
    case 3:
      strcpy(str, "CM & RTO");
      break;
    case 4:
      strcpy(str, "Ring");
      break;
    case 5:
      strcpy(str, "CM & Ring");
      break;
    case 6:
      strcpy(str, "RTO & Ring");
      break;
    case 7:
      strcpy(str, "CM & RTO & Ring");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}

void NetworkOpener::soundToString(const int sound, char* str) {
  switch (sound) {
    case 0:
      strcpy(str, "No Sound");
      break;
    case 1:
      strcpy(str, "Sound 1");
      break;
    case 2:
      strcpy(str, "Sound 2");
      break;
    case 3:
      strcpy(str, "Sound 3");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}