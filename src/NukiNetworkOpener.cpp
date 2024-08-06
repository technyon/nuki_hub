#include "NukiNetworkOpener.h"
#include "Arduino.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include <ArduinoJson.h>

NukiNetworkOpener::NukiNetworkOpener(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize)
        : _preferences(preferences),
          _network(network),
          _buffer(buffer),
          _bufferSize(bufferSize)
{
    memset(_authName, 0, sizeof(_authName));
    _authName[0] = '\0';

    _network->registerMqttReceiver(this);
}

void NukiNetworkOpener::initialize()
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

    if(_preferences->getBool(preference_disable_non_json, false))
    {
        _network->removeTopic(_mqttPath, mqtt_topic_keypad_command_action);
        _network->removeTopic(_mqttPath, mqtt_topic_keypad_command_id);
        _network->removeTopic(_mqttPath, mqtt_topic_keypad_command_name);
        _network->removeTopic(_mqttPath, mqtt_topic_keypad_command_code);
        _network->removeTopic(_mqttPath, mqtt_topic_keypad_command_enabled);
        _network->removeTopic(_mqttPath, mqtt_topic_config_button_enabled);
        _network->removeTopic(_mqttPath, mqtt_topic_config_led_enabled);
        _network->removeTopic(_mqttPath, mqtt_topic_config_sound_level);
        _network->removeTopic(_mqttPath, mqtt_topic_keypad_command_result);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_level);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_critical);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_charging);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_voltage);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_keypad_critical);
        //_network->removeTopic(_mqttPath, mqtt_topic_presence);
    }

    if(!_preferences->getBool(preference_conf_info_enabled, true))
    {
        _network->removeTopic(_mqttPath, mqtt_topic_config_basic_json);
        _network->removeTopic(_mqttPath, mqtt_topic_config_advanced_json);
        _network->removeTopic(_mqttPath, mqtt_topic_config_button_enabled);
        _network->removeTopic(_mqttPath, mqtt_topic_config_led_enabled);
        _network->removeTopic(_mqttPath, mqtt_topic_config_led_brightness);
        _network->removeTopic(_mqttPath, mqtt_topic_config_auto_unlock);
        _network->removeTopic(_mqttPath, mqtt_topic_config_auto_lock);
        _network->removeTopic(_mqttPath, mqtt_topic_config_single_lock);
    }

    if(_preferences->getBool(preference_keypad_control_enabled))
    {
        if(!_preferences->getBool(preference_disable_non_json, false))
        {
            _network->subscribe(_mqttPath, mqtt_topic_keypad_command_action);
            _network->subscribe(_mqttPath, mqtt_topic_keypad_command_id);
            _network->subscribe(_mqttPath, mqtt_topic_keypad_command_name);
            _network->subscribe(_mqttPath, mqtt_topic_keypad_command_code);
            _network->subscribe(_mqttPath, mqtt_topic_keypad_command_enabled);
            _network->initTopic(_mqttPath, mqtt_topic_keypad_command_action, "--");
            _network->initTopic(_mqttPath, mqtt_topic_keypad_command_id, "0");
            _network->initTopic(_mqttPath, mqtt_topic_keypad_command_name, "--");
            _network->initTopic(_mqttPath, mqtt_topic_keypad_command_code, "000000");
            _network->initTopic(_mqttPath, mqtt_topic_keypad_command_enabled, "1");
        }

        _network->subscribe(_mqttPath, mqtt_topic_query_keypad);
        _network->subscribe(_mqttPath, mqtt_topic_keypad_json_action);
        _network->initTopic(_mqttPath, mqtt_topic_query_keypad, "0");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_json_action, "--");
    }

    if(_preferences->getBool(preference_timecontrol_control_enabled))
    {
        _network->subscribe(_mqttPath, mqtt_topic_timecontrol_action);
        _network->initTopic(_mqttPath, mqtt_topic_timecontrol_action, "--");
    }
    
    if(_preferences->getBool(preference_auth_control_enabled))
    {
        _network->subscribe(_mqttPath, mqtt_topic_auth_action);
        _network->initTopic(_mqttPath, mqtt_topic_auth_action, "--");
    }

    if(_preferences->getBool(preference_publish_authdata, false))
    {
        _network->subscribe(_mqttPath, mqtt_topic_lock_log_rolling_last);
    }

    _network->addReconnectedCallback([&]()
     {
         _reconnected = true;
     });
}

void NukiNetworkOpener::update()
{
    if(_resetRingStateTs != 0 && (esp_timer_get_time() / 1000) >= _resetRingStateTs)
    {
        _resetRingStateTs = 0;
        publishString(mqtt_topic_lock_binary_ring, "standby", true);
    }
}

void NukiNetworkOpener::onMqttDataReceived(const char* topic, byte* payload, const unsigned int length)
{
    char* value = (char*)payload;

    if(_network->mqttRecentlyConnected() && _network->pathEquals(_mqttPath, mqtt_topic_lock_action, topic))
    {
        Log->println("MQTT recently connected, ignoring opener action.");
        return;
    }

    if(comparePrefixedPath(topic, mqtt_topic_lock_log_rolling_last))
    {
        if(strcmp(value, "") == 0 ||
           strcmp(value, "--") == 0) return;

        if(atoi(value) > 0 && atoi(value) > _lastRollingLog) _lastRollingLog = atoi(value);
    }

    if(comparePrefixedPath(topic, mqtt_topic_lock_action))
    {
        if(strcmp(value, "") == 0 ||
           strcmp(value, "--") == 0 ||
           strcmp(value, "ack") == 0 ||
           strcmp(value, "unknown_action") == 0 ||
           strcmp(value, "denied") == 0 ||
           strcmp(value, "error") == 0) return;

        Log->print(F("Opener action received: "));
        Log->println(value);
        LockActionResult lockActionResult = LockActionResult::Failed;
        if(_lockActionReceivedCallback != NULL)
        {
            lockActionResult = _lockActionReceivedCallback(value);
        }

        switch(lockActionResult)
        {
            case LockActionResult::Success:
                publishString(mqtt_topic_lock_action, "ack", false);
                break;
            case LockActionResult::UnknownAction:
                publishString(mqtt_topic_lock_action, "unknown_action", false);
                break;
            case LockActionResult::AccessDenied:
                publishString(mqtt_topic_lock_action, "denied", false);
                break;
            case LockActionResult::Failed:
                publishString(mqtt_topic_lock_action, "error", false);
                break;
        }
    }

    if(!_preferences->getBool(preference_disable_non_json, false))
    {
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
                    publishString(mqtt_topic_keypad_command_action, "--", true);
                }
                publishInt(mqtt_topic_keypad_command_id, _keypadCommandId, true);
                publishString(mqtt_topic_keypad_command_name, _keypadCommandName, true);
                publishString(mqtt_topic_keypad_command_code, _keypadCommandCode, true);
                publishInt(mqtt_topic_keypad_command_enabled, _keypadCommandEnabled, true);
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
    }

    if(comparePrefixedPath(topic, mqtt_topic_query_config) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_CONFIG;
        publishString(mqtt_topic_query_config, "0", true);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_query_lockstate) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_LOCKSTATE;
        publishString(mqtt_topic_query_lockstate, "0", true);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_query_keypad) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_KEYPAD;
        publishString(mqtt_topic_query_keypad, "0", true);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_query_battery) && strcmp(value, "1") == 0)
    {
        _queryCommands = _queryCommands | QUERY_COMMAND_BATTERY;
        publishString(mqtt_topic_query_battery, "0", true);
    }

    if(comparePrefixedPath(topic, mqtt_topic_config_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "--") == 0) return;

        if(_configUpdateReceivedCallback != NULL)
        {
            _configUpdateReceivedCallback(value);
        }

        publishString(mqtt_topic_config_action, "--", true);
    }

    if(comparePrefixedPath(topic, mqtt_topic_keypad_json_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "--") == 0) return;

        if(_keypadJsonCommandReceivedReceivedCallback != NULL)
        {
            _keypadJsonCommandReceivedReceivedCallback(value);
        }

        publishString(mqtt_topic_keypad_json_action, "--", true);
    }

    if(comparePrefixedPath(topic, mqtt_topic_timecontrol_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "--") == 0) return;

        if(_timeControlCommandReceivedReceivedCallback != NULL)
        {
            _timeControlCommandReceivedReceivedCallback(value);
        }

        publishString(mqtt_topic_timecontrol_action, "--", true);
    }
    
    if(comparePrefixedPath(topic, mqtt_topic_auth_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "--") == 0) return;

        if(_authCommandReceivedReceivedCallback != NULL)
        {
            _authCommandReceivedReceivedCallback(value);
        }

        publishString(mqtt_topic_auth_action, "--", true);
    }
}

void NukiNetworkOpener::publishKeyTurnerState(const NukiOpener::OpenerState& keyTurnerState, const NukiOpener::OpenerState& lastKeyTurnerState)
{
    _currentLockState = keyTurnerState.lockState;

    char str[50];
    memset(&str, 0, sizeof(str));

    JsonDocument json;
    JsonDocument jsonBattery;

    lockstateToString(keyTurnerState.lockState, str);

    if((_firstTunerStatePublish || keyTurnerState.lockState != lastKeyTurnerState.lockState || keyTurnerState.nukiState != lastKeyTurnerState.nukiState) && keyTurnerState.lockState != NukiOpener::LockState::Undefined)
    {
        publishString(mqtt_topic_lock_state, str, true);

        if(_haEnabled)
        {
            publishState(keyTurnerState);
        }
    }

    json["lock_state"] = str;

    if(keyTurnerState.nukiState == NukiOpener::State::ContinuousMode)
    {
        publishString(mqtt_topic_lock_continuous_mode, "on", true);
        json["continuous_mode"] = 1;
    } else {
        publishString(mqtt_topic_lock_continuous_mode, "off", true);
        json["continuous_mode"] = 0;
    }

    memset(&str, 0, sizeof(str));
    triggerToString(keyTurnerState.trigger, str);

    if(_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
    {
        publishString(mqtt_topic_lock_trigger, str, true);
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
        publishString(mqtt_topic_lock_last_lock_action, str, true);
    }

    json["last_lock_action"] = str;

    memset(&str, 0, sizeof(str));
    triggerToString(keyTurnerState.lastLockActionTrigger, str);
    json["last_lock_action_trigger"] = str;

    memset(&str, 0, sizeof(str));
    completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);

    if(_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        publishString(mqtt_topic_lock_completionStatus, str, true);
    }

    json["lock_completion_status"] = str;

    memset(&str, 0, sizeof(str));
    NukiOpener::doorSensorStateToString(keyTurnerState.doorSensorState, str);

    if(_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
    {
        publishString(mqtt_topic_lock_door_sensor_state, str, true);
    }

    json["door_sensor_state"] = str;

    bool critical = (keyTurnerState.criticalBatteryState & 0b00000001) > 0;
    jsonBattery["critical"] = critical ? "1" : "0";

    if((_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState) && !_preferences->getBool(preference_disable_non_json, false))
    {
        publishBool(mqtt_topic_battery_critical, critical, true);
    }

    json["auth_id"] = _authId;
    json["auth_name"] = _authName;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_lock_json, _buffer, true);

    serializeJson(jsonBattery, _buffer, _bufferSize);
    publishString(mqtt_topic_battery_basic_json, _buffer, true);

    _firstTunerStatePublish = false;
}

void NukiNetworkOpener::publishRing(const bool locked)
{
    if(locked)
    {
        publishString(mqtt_topic_lock_ring, "ringlocked", true);
    }
    else
    {
        publishString(mqtt_topic_lock_ring, "ring", true);
    }

    publishString(mqtt_topic_lock_binary_ring, "ring", true);
    _resetRingStateTs = (esp_timer_get_time() / 1000) + 2000;
}

void NukiNetworkOpener::publishState(NukiOpener::OpenerState lockState)
{
    if(lockState.nukiState == NukiOpener::State::ContinuousMode)
    {
        publishString(mqtt_topic_lock_ha_state, "unlocked", true);
        publishString(mqtt_topic_lock_binary_state, "unlocked", true);
    }
    else
    {
        switch (lockState.lockState)
        {
            case NukiOpener::LockState::Locked:
                publishString(mqtt_topic_lock_ha_state, "locked", true);
                publishString(mqtt_topic_lock_binary_state, "locked", true);
                break;
            case NukiOpener::LockState::RTOactive:
                publishString(mqtt_topic_lock_ha_state, "unlocked", true);
                publishString(mqtt_topic_lock_binary_state, "unlocked", true);
                break;
            case NukiOpener::LockState::Open:
                publishString(mqtt_topic_lock_ha_state, "open", true);
                publishString(mqtt_topic_lock_binary_state, "unlocked", true);
                break;
            case NukiOpener::LockState::Opening:
                publishString(mqtt_topic_lock_ha_state, "opening", true);
                publishString(mqtt_topic_lock_binary_state, "unlocked", true);
                break;
            case NukiOpener::LockState::Undefined:
            case NukiOpener::LockState::Uncalibrated:
                publishString(mqtt_topic_lock_ha_state, "jammed", true);
                break;
            default:
                break;
        }
    }
}

void NukiNetworkOpener::publishAuthorizationInfo(const std::list<NukiOpener::LogEntry>& logEntries, bool latest)
{
    char str[50];
    char authName[33];
    uint32_t authIndex = 0;

    JsonDocument json;

    for(const auto& log : logEntries)
    {
        memset(authName, 0, sizeof(authName));
        authName[0] = '\0';

        if((log.loggingType == NukiOpener::LoggingType::LockAction || log.loggingType == NukiOpener::LoggingType::KeypadAction))
        {
            int sizeName = sizeof(log.name);
            memcpy(authName, log.name, sizeName);
            if(authName[sizeName - 1] != '\0') authName[sizeName] = '\0';

            if(log.index > authIndex)
            {
                authIndex = log.index;
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

                switch(log.data[1])
                {
                    case 0:
                        entry["trigger"] = "arrowkey";
                        break;
                    case 1:
                        entry["trigger"] = "code";
                        break;
                    case 2:
                        entry["trigger"] = "fingerprint";
                        break;
                    default:
                        entry["trigger"] = "Unknown";
                        break;
                }

                memset(str, 0, sizeof(str));

                if(log.data[2] == 9) entry["completionStatus"] = "notAuthorized";
                else if (log.data[2] == 224) entry["completionStatus"] = "invalidCode";
                else
                {
                    NukiOpener::completionStatusToString((NukiOpener::CompletionStatus)log.data[2], str);
                    entry["completionStatus"] = str;
                }

                entry["codeId"] = 256U*log.data[4]+log.data[3];
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
                entry["soundId"] = log.data[4];
                memset(str, 0, sizeof(str));
                NukiOpener::completionStatusToString((NukiOpener::CompletionStatus)log.data[5], str);
                entry["completionStatus"] = str;
                entry["codeId"] = 256U*log.data[7]+log.data[6];
                break;
        }

        if(log.index > _lastRollingLog)
        {
            _lastRollingLog = log.index;
            serializeJson(entry, _buffer, _bufferSize);
            publishString(mqtt_topic_lock_log_rolling, _buffer, true);
            publishInt(mqtt_topic_lock_log_rolling_last, log.index, true);
        }
    }

    serializeJson(json, _buffer, _bufferSize);

    if(latest) publishString(mqtt_topic_lock_log_latest, _buffer, true);
    else publishString(mqtt_topic_lock_log, _buffer, true);

    if(authIndex > 0)
    {
        publishUInt(mqtt_topic_lock_auth_id, _authId, true);
        publishString(mqtt_topic_lock_auth_name, _authName, true);
    }
}

void NukiNetworkOpener::clearAuthorizationInfo()
{
    publishString(mqtt_topic_lock_log, "--", true);
    publishUInt(mqtt_topic_lock_auth_id, 0, true);
    publishString(mqtt_topic_lock_auth_name, "--", true);
}

void NukiNetworkOpener::publishCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_lock_action_command_result, resultStr, true);
}

void NukiNetworkOpener::publishLockstateCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_query_lockstate_command_result, resultStr, true);
}

void NukiNetworkOpener::publishBatteryReport(const NukiOpener::BatteryReport& batteryReport)
{
    if(!_preferences->getBool(preference_disable_non_json, false))
    {
        publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0, true);
    }

    char str[50];
    memset(&str, 0, sizeof(str));

    JsonDocument json;

    json["batteryVoltage"] = (float)batteryReport.batteryVoltage / 1000.0;
    json["critical"] = batteryReport.criticalBatteryState;
    lockactionToString(batteryReport.lockAction, str);
    json["lockAction"] = str;
    json["startVoltage"] = (float)batteryReport.startVoltage / 1000.0;
    json["lowestVoltage"] = (float)batteryReport.lowestVoltage / 1000.0;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_battery_advanced_json, _buffer, true);
}

void NukiNetworkOpener::publishConfig(const NukiOpener::Config &config)
{
    char str[50];
    char curTime[20];
    sprintf(curTime, "%04d-%02d-%02d %02d:%02d:%02d", config.currentTimeYear, config.currentTimeMonth, config.currentTimeDay, config.currentTimeHour, config.currentTimeMinute, config.currentTimeSecond);
    char uidString[20];
    itoa(config.nukiId, uidString, 16);

    JsonDocument json;

    memset(_nukiName, 0, sizeof(_nukiName));
    memcpy(_nukiName, config.name, sizeof(config.name));

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
    publishString(mqtt_topic_config_basic_json, _buffer, true);

    if(!_preferences->getBool(preference_disable_non_json, false))
    {
        publishBool(mqtt_topic_config_button_enabled, config.buttonEnabled == 1, true);
        publishBool(mqtt_topic_config_led_enabled, config.ledFlashEnabled == 1, true);
    }

    publishString(mqtt_topic_info_firmware_version, std::to_string(config.firmwareVersion[0]) + "." + std::to_string(config.firmwareVersion[1]) + "." + std::to_string(config.firmwareVersion[2]), true);
    publishString(mqtt_topic_info_hardware_version, std::to_string(config.hardwareRevision[0]) + "." + std::to_string(config.hardwareRevision[1]), true);
}

void NukiNetworkOpener::publishAdvancedConfig(const NukiOpener::AdvancedConfig &config)
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
    publishString(mqtt_topic_config_advanced_json, _buffer, true);

    if(!_preferences->getBool(preference_disable_non_json, false))
    {
        publishUInt(mqtt_topic_config_sound_level, config.soundLevel, true);
    }
}

void NukiNetworkOpener::publishRssi(const int &rssi)
{
    publishInt(mqtt_topic_lock_rssi, rssi, true);
}

void NukiNetworkOpener::publishRetry(const std::string& message)
{
    publishString(mqtt_topic_lock_retry, message, true);
}

void NukiNetworkOpener::publishBleAddress(const std::string &address)
{
    publishString(mqtt_topic_lock_address, address, true);
}

void NukiNetworkOpener::publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const char *softwareVersion, const char *hardwareVersion, const bool& publishAuthData, const bool& hasKeypad, char* lockAction, char* unlockAction, char* openAction)
{
    String availabilityTopic = _preferences->getString("mqttpath");
    availabilityTopic.concat("/maintenance/mqttConnectionState");

    _network->publishHASSConfig(deviceType, baseTopic, name, uidString, softwareVersion, hardwareVersion, availabilityTopic.c_str(), hasKeypad, lockAction, unlockAction, openAction);
    _network->publishHASSConfigAdditionalOpenerEntities(deviceType, baseTopic, name, uidString);
    if(publishAuthData)
    {
        _network->publishHASSConfigAccessLog(deviceType, baseTopic, name, uidString);
    }
    else
    {
        _network->removeHASSConfigTopic((char*)"sensor", (char*)"last_action_authorization", uidString);
        _network->removeHASSConfigTopic((char*)"sensor", (char*)"rolling_log", uidString);
    }
    if(hasKeypad)
    {
        _network->publishHASSConfigKeypad(deviceType, baseTopic, name, uidString);
    }
    else
    {
        _network->removeHASSConfigTopic((char*)"sensor", (char*)"keypad_status", uidString);
        _network->removeHASSConfigTopic((char*)"binary_sensor", (char*)"keypad_battery_low", uidString);
    }
}

void NukiNetworkOpener::removeHASSConfig(char* uidString)
{
    _network->removeHASSConfig(uidString);
}

void NukiNetworkOpener::publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount)
{
    uint index = 0;
    char uidString[20];
    itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_opener_path);
    JsonDocument json;

    for(const auto& entry : entries)
    {
        String basePath = mqtt_topic_keypad;
        basePath.concat("/code_");
        basePath.concat(std::to_string(index).c_str());
        publishKeypadEntry(basePath, entry);

        auto jsonEntry = json.add<JsonVariant>();

        jsonEntry["codeId"] = entry.codeId;

        if(_preferences->getBool(preference_keypad_publish_code, false))
        {
            jsonEntry["code"] = entry.code;
        }

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

        if(_preferences->getBool(preference_keypad_topic_per_entry, false))
        {
            basePath = mqtt_topic_keypad;
            basePath.concat("/codes/");
            basePath.concat(std::to_string(index).c_str());
            jsonEntry["name_ha"] = entry.name;
            jsonEntry["index"] = index;
            serializeJson(jsonEntry, _buffer, _bufferSize);
            publishString(basePath.c_str(), _buffer, true);

            String basePathPrefix = "~";
            basePathPrefix.concat(basePath);
            const char *basePathPrefixChr = basePathPrefix.c_str();

            std::string baseCommand = std::string("{ \"action\": \"update\", \"codeId\": \"") + std::to_string(entry.codeId);
            std::string enaCommand = baseCommand + (char*)"\", \"enabled\": \"1\" }";
            std::string disCommand = baseCommand + (char*)"\", \"enabled\": \"0\" }";
            std::string mqttDeviceName = std::string("keypad_") + std::to_string(index);
            std::string uidStringPostfix = std::string("_") + mqttDeviceName;
            char codeName[33];
            memcpy(codeName, entry.name, sizeof(entry.name));
            codeName[sizeof(entry.name)] = '\0';
            std::string displayName = std::string("Keypad - ") + std::string((char*)codeName) + " - " + std::to_string(entry.codeId);

            _network->publishHassTopic("switch",
                             mqttDeviceName.c_str(),
                             uidString,
                             uidStringPostfix.c_str(),
                             displayName.c_str(),
                             _nukiName,
                             baseTopic.c_str(),
                             String("~") + basePath.c_str(),
                             (char*)"SmartLock",
                             "",
                             "",
                             "diagnostic",
                             String("~") + mqtt_topic_keypad_json_action,
                             { { (char*)"json_attr_t", (char*)basePathPrefixChr },
                               { (char*)"pl_on", (char*)enaCommand.c_str() },
                               { (char*)"pl_off", (char*)disCommand.c_str() },
                               { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                               { (char*)"stat_on", (char*)"1" },
                               { (char*)"stat_off", (char*)"0" }});
        }

        ++index;
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_keypad_json, _buffer, true);

    if(!_preferences->getBool(preference_disable_non_json, false))
    {
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

        if(!_preferences->getBool(preference_keypad_publish_code, false))
        {
            for(int i=0; i<maxKeypadCodeCount; i++)
            {
                String codeTopic = _mqttPath;
                codeTopic.concat(mqtt_topic_keypad);
                codeTopic.concat("/code_");
                codeTopic.concat(std::to_string(i).c_str());
                codeTopic.concat("/");
                _network->removeTopic(codeTopic, "code");
            }
        }
    }
    else
    {
        for(int i=0; i<maxKeypadCodeCount; i++)
        {
            String codeTopic = _mqttPath;
            codeTopic.concat(mqtt_topic_keypad);
            codeTopic.concat("/code_");
            codeTopic.concat(std::to_string(i).c_str());
            codeTopic.concat("/");
            _network->removeTopic(codeTopic, "id");
            _network->removeTopic(codeTopic, "enabled");
            _network->removeTopic(codeTopic, "code");
            _network->removeTopic(codeTopic, "name");
            _network->removeTopic(codeTopic, "createdYear");
            _network->removeTopic(codeTopic, "createdMonth");
            _network->removeTopic(codeTopic, "createdDay");
            _network->removeTopic(codeTopic, "createdHour");
            _network->removeTopic(codeTopic, "createdMin");
            _network->removeTopic(codeTopic, "createdSec");
            _network->removeTopic(codeTopic, "lockCount");
        }

        for(int j=entries.size(); j<maxKeypadCodeCount; j++)
        {
            String codesTopic = _mqttPath;
            codesTopic.concat(mqtt_topic_keypad_codes);
            codesTopic.concat("/");
            _network->removeTopic(codesTopic, (char*)std::to_string(j).c_str());
            std::string mqttDeviceName = std::string("keypad_") + std::to_string(j);
            _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
        }
    }
}

void NukiNetworkOpener::publishTimeControl(const std::list<NukiOpener::TimeControlEntry>& timeControlEntries, uint maxTimeControlEntryCount)
{
    uint index = 0;
    char str[50];
    char uidString[20];
    itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_opener_path);
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

        if(_preferences->getBool(preference_timecontrol_topic_per_entry, false))
        {
            String basePath = mqtt_topic_timecontrol;
            basePath.concat("/entries/");
            basePath.concat(std::to_string(index).c_str());
            jsonEntry["index"] = index;
            serializeJson(jsonEntry, _buffer, _bufferSize);
            publishString(basePath.c_str(), _buffer, true);
            String basePathPrefix = "~";
            basePathPrefix.concat(basePath);
            const char *basePathPrefixChr = basePathPrefix.c_str();
            std::string baseCommand = std::string("{ \"action\": \"update\", \"entryId\": \"") + std::to_string(entry.entryId);
            std::string enaCommand = baseCommand + (char*)"\", \"enabled\": \"1\" }";
            std::string disCommand = baseCommand + (char*)"\", \"enabled\": \"0\" }";
            std::string mqttDeviceName = std::string("timecontrol_") + std::to_string(index);
            std::string uidStringPostfix = std::string("_") + mqttDeviceName;
            std::string displayName = std::string("Timecontrol - ") + std::to_string(entry.entryId);

            _network->publishHassTopic("switch",
                             mqttDeviceName.c_str(),
                             uidString,
                             uidStringPostfix.c_str(),
                             displayName.c_str(),
                             _nukiName,
                             baseTopic.c_str(),
                             String("~") + basePath.c_str(),
                             (char*)"Opener",
                             "",
                             "",
                             "diagnostic",
                             String("~") + mqtt_topic_timecontrol_action,
                             { { (char*)"json_attr_t", (char*)basePathPrefixChr },
                               { (char*)"pl_on", (char*)enaCommand.c_str() },
                               { (char*)"pl_off", (char*)disCommand.c_str() },
                               { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                               { (char*)"stat_on", (char*)"1" },
                               { (char*)"stat_off", (char*)"0" }});
        }

        ++index;
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_timecontrol_json, _buffer, true);

    for(int j=timeControlEntries.size(); j<maxTimeControlEntryCount; j++)
    {
        String entriesTopic = _mqttPath;
        entriesTopic.concat(mqtt_topic_timecontrol_entries);
        entriesTopic.concat("/");
        _network->removeTopic(entriesTopic, (char*)std::to_string(j).c_str());
        std::string mqttDeviceName = std::string("timecontrol_") + std::to_string(j);
        _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
    }
}

void NukiNetworkOpener::publishAuth(const std::list<NukiOpener::AuthorizationEntry>& authEntries, uint maxAuthEntryCount)
{
    uint index = 0;
    char str[50];
    char uidString[20];
    itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_opener_path);
    JsonDocument json;

    for(const auto& entry : authEntries)
    {
        auto jsonEntry = json.add<JsonVariant>();

        jsonEntry["authId"] = entry.authId;
        jsonEntry["idType"] = entry.idType; //CONSIDER INT TO STRING
        jsonEntry["enabled"] = entry.enabled;
        jsonEntry["name"] = entry.name;
        jsonEntry["remoteAllowed"] = entry.remoteAllowed;
        char createdDT[20];
        sprintf(createdDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.createdYear, entry.createdMonth, entry.createdDay, entry.createdHour, entry.createdMinute, entry.createdSecond);
        jsonEntry["dateCreated"] = createdDT;
        jsonEntry["lockCount"] = entry.lockCount;
        char lastActiveDT[20];
        sprintf(lastActiveDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.lastActYear, entry.lastActMonth, entry.lastActDay, entry.lastActHour, entry.lastActMinute, entry.lastActSecond);
        jsonEntry["dateLastActive"] = lastActiveDT;
        jsonEntry["timeLimited"] = entry.timeLimited;
        char allowedFromDT[20];
        sprintf(allowedFromDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.allowedFromYear, entry.allowedFromMonth, entry.allowedFromDay, entry.allowedFromHour, entry.allowedFromMinute, entry.allowedFromSecond);
        jsonEntry["allowedFrom"] = allowedFromDT;
        char allowedUntilDT[20];
        sprintf(allowedUntilDT, "%04d-%02d-%02d %02d:%02d:%02d", entry.allowedUntilYear, entry.allowedUntilMonth, entry.allowedUntilDay, entry.allowedUntilHour, entry.allowedUntilMinute, entry.allowedUntilSecond);
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

        if(_preferences->getBool(preference_auth_topic_per_entry, false))
        {
            String basePath = mqtt_topic_auth;
            basePath.concat("/entries/");
            basePath.concat(std::to_string(index).c_str());
            jsonEntry["index"] = index;
            serializeJson(jsonEntry, _buffer, _bufferSize);
            publishString(basePath.c_str(), _buffer, true);

            String basePathPrefix = "~";
            basePathPrefix.concat(basePath);
            const char *basePathPrefixChr = basePathPrefix.c_str();

            std::string baseCommand = std::string("{ \"action\": \"update\", \"authId\": \"") + std::to_string(entry.authId);
            std::string enaCommand = baseCommand + (char*)"\", \"enabled\": \"1\" }";
            std::string disCommand = baseCommand + (char*)"\", \"enabled\": \"0\" }";
            std::string mqttDeviceName = std::string("auth_") + std::to_string(index);
            std::string uidStringPostfix = std::string("_") + mqttDeviceName;
            std::string displayName = std::string("Authorization - ") + std::to_string(entry.authId);

            _network->publishHassTopic("switch",
                             mqttDeviceName.c_str(),
                             uidString,
                             uidStringPostfix.c_str(),
                             displayName.c_str(),
                             _nukiName,
                             baseTopic.c_str(),
                             String("~") + basePath.c_str(),
                             (char*)"Opener",
                             "",
                             "",
                             "diagnostic",
                             String("~") + mqtt_topic_auth_action,
                             { { (char*)"json_attr_t", (char*)basePathPrefixChr },
                               { (char*)"pl_on", (char*)enaCommand.c_str() },
                               { (char*)"pl_off", (char*)disCommand.c_str() },
                               { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                               { (char*)"stat_on", (char*)"1" },
                               { (char*)"stat_off", (char*)"0" }});
        }

        ++index;
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_auth_json, _buffer, true);

    for(int j=authEntries.size(); j<maxAuthEntryCount; j++)
    {
        String entriesTopic = _mqttPath;
        entriesTopic.concat(mqtt_topic_auth_entries);
        entriesTopic.concat("/");
        _network->removeTopic(entriesTopic, (char*)std::to_string(j).c_str());
        std::string mqttDeviceName = std::string("auth_") + std::to_string(j);
        _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
    }
}

void NukiNetworkOpener::publishConfigCommandResult(const char* result)
{
    publishString(mqtt_topic_config_action_command_result, result, true);
}

void NukiNetworkOpener::publishKeypadCommandResult(const char* result)
{
    if(_preferences->getBool(preference_disable_non_json, false)) return;
    publishString(mqtt_topic_keypad_command_result, result, true);
}

void NukiNetworkOpener::publishKeypadJsonCommandResult(const char* result)
{
    publishString(mqtt_topic_keypad_json_command_result, result, true);
}

void NukiNetworkOpener::publishTimeControlCommandResult(const char* result)
{
    publishString(mqtt_topic_timecontrol_command_result, result, true);
}

void NukiNetworkOpener::publishAuthCommandResult(const char* result)
{
    publishString(mqtt_topic_auth_command_result, result, true);
}

void NukiNetworkOpener::publishStatusUpdated(const bool statusUpdated)
{
    publishBool(mqtt_topic_lock_status_updated, statusUpdated, true);
}

void NukiNetworkOpener::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NukiNetworkOpener::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void NukiNetworkOpener::setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled))
{
    if(_preferences->getBool(preference_disable_non_json, false)) return;
    _keypadCommandReceivedReceivedCallback = keypadCommandReceivedReceivedCallback;
}

void NukiNetworkOpener::setKeypadJsonCommandReceivedCallback(void (*keypadJsonCommandReceivedReceivedCallback)(const char *))
{
    _keypadJsonCommandReceivedReceivedCallback = keypadJsonCommandReceivedReceivedCallback;
}

void NukiNetworkOpener::setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char *))
{
    _timeControlCommandReceivedReceivedCallback = timeControlCommandReceivedReceivedCallback;
}

void NukiNetworkOpener::setAuthCommandReceivedCallback(void (*authCommandReceivedReceivedCallback)(const char *))
{
    _authCommandReceivedReceivedCallback = authCommandReceivedReceivedCallback;
}

void NukiNetworkOpener::publishFloat(const char *topic, const float value, bool retain, const uint8_t precision)
{
    _network->publishFloat(_mqttPath, topic, value, retain, precision);
}

void NukiNetworkOpener::publishInt(const char *topic, const int value, bool retain)
{
    _network->publishInt(_mqttPath, topic, value, retain);
}

void NukiNetworkOpener::publishUInt(const char *topic, const unsigned int value, bool retain)
{
    _network->publishUInt(_mqttPath, topic, value, retain);
}

void NukiNetworkOpener::publishBool(const char *topic, const bool value, bool retain)
{
    _network->publishBool(_mqttPath, topic, value, retain);
}

void NukiNetworkOpener::publishString(const char *topic, const String &value, bool retain)
{
    char str[value.length() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.begin(), value.length());
    publishString(topic, str, retain);
}

void NukiNetworkOpener::publishString(const char *topic, const std::string &value, bool retain)
{
    char str[value.size() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.data(), value.length());
    publishString(topic, str, retain);
}

void NukiNetworkOpener::publishString(const char* topic, const char* value, bool retain)
{
    _network->publishString(_mqttPath, topic, value, retain);
}

void NukiNetworkOpener::publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry)
{
    if(_preferences->getBool(preference_disable_non_json, false)) return;

    char codeName[sizeof(entry.name) + 1];
    memset(codeName, 0, sizeof(codeName));
    memcpy(codeName, entry.name, sizeof(entry.name));

    publishInt(concat(topic, "/id").c_str(), entry.codeId, true);
    publishBool(concat(topic, "/enabled").c_str(), entry.enabled, true);
    publishString(concat(topic, "/name").c_str(), codeName, true);

    if(_preferences->getBool(preference_keypad_publish_code, false))
    {
        publishInt(concat(topic, "/code").c_str(), entry.code, true);
    }

    publishInt(concat(topic, "/createdYear").c_str(), entry.dateCreatedYear, true);
    publishInt(concat(topic, "/createdMonth").c_str(), entry.dateCreatedMonth, true);
    publishInt(concat(topic, "/createdDay").c_str(), entry.dateCreatedDay, true);
    publishInt(concat(topic, "/createdHour").c_str(), entry.dateCreatedHour, true);
    publishInt(concat(topic, "/createdMin").c_str(), entry.dateCreatedMin, true);
    publishInt(concat(topic, "/createdSec").c_str(), entry.dateCreatedSec, true);
    publishInt(concat(topic, "/lockCount").c_str(), entry.lockCount, true);
}

void NukiNetworkOpener::buildMqttPath(const char* path, char* outPath)
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

void NukiNetworkOpener::subscribe(const char *path)
{
    char prefixedPath[500];
    buildMqttPath(path, prefixedPath);
    _network->subscribe(prefixedPath, MQTT_QOS_LEVEL);
}

bool NukiNetworkOpener::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);

    return strcmp(fullPath, prefixedPath) == 0;
}

String NukiNetworkOpener::concat(String a, String b)
{
    String c = a;
    c.concat(b);
    return c;
}

bool NukiNetworkOpener::reconnected()
{
    bool r = _reconnected;
    _reconnected = false;
    return r;
}

uint8_t NukiNetworkOpener::queryCommands()
{
    uint8_t qc = _queryCommands;
    _queryCommands = 0;
    return qc;
}

void NukiNetworkOpener::buttonPressActionToString(const NukiOpener::ButtonPressAction btnPressAction, char* str) {
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

void NukiNetworkOpener::fobActionToString(const int fobact, char* str) {
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

void NukiNetworkOpener::capabilitiesToString(const int capabilities, char* str) {
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

void NukiNetworkOpener::operatingModeToString(const int opmode, char* str) {
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

void NukiNetworkOpener::doorbellSuppressionToString(const int dbsupr, char* str) {
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

void NukiNetworkOpener::soundToString(const int sound, char* str) {
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
