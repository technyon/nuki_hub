#include "NetworkLock.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <ArduinoJson.h>
#include <ctype.h>

NetworkLock::NetworkLock(Network* network, Preferences* preferences, char* buffer, size_t bufferSize)
: _network(network),
  _preferences(preferences),
  _buffer(buffer),
  _bufferSize(bufferSize)
{
    memset(_authName, 0, sizeof(_authName));
    _authName[0] = '\0';

    _offTopics.reserve(10);
    //_offTopics.push_back(mqtt_topic_official_mode);
    _offTopics.push_back((char*)mqtt_topic_official_state);
    _offTopics.push_back((char*)mqtt_topic_official_batteryCritical);
    _offTopics.push_back((char*)mqtt_topic_official_batteryChargeState);
    _offTopics.push_back((char*)mqtt_topic_official_batteryCharging);
    _offTopics.push_back((char*)mqtt_topic_official_keypadBatteryCritical);
    _offTopics.push_back((char*)mqtt_topic_official_doorsensorState);
    _offTopics.push_back((char*)mqtt_topic_official_doorsensorBatteryCritical);
    _offTopics.push_back((char*)mqtt_topic_official_connected);
    _offTopics.push_back((char*)mqtt_topic_official_commandResponse);
    _offTopics.push_back((char*)mqtt_topic_official_lockActionEvent);

    _network->registerMqttReceiver(this);
}

NetworkLock::~NetworkLock()
{
}

void NetworkLock::initialize()
{
    String mqttPath = _preferences->getString(preference_mqtt_lock_path);
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
        strcpy(_mqttPath, "nuki");
        _preferences->putString(preference_mqtt_lock_path, _mqttPath);
    }

    _network->setMqttPresencePath(_mqttPath);

    _haEnabled = _preferences->getString(preference_mqtt_hass_discovery) != "";

    _network->initTopic(_mqttPath, mqtt_topic_lock_action, "--");
    _network->subscribe(_mqttPath, mqtt_topic_lock_action);
    _network->initTopic(_mqttPath, mqtt_topic_config_action, "--");
    _network->subscribe(_mqttPath, mqtt_topic_config_action);

    _network->subscribe(_mqttPath, mqtt_topic_reset);
    _network->initTopic(_mqttPath, mqtt_topic_reset, "0");

    _network->subscribe(_mqttPath, mqtt_topic_webserver_action);
    _network->initTopic(_mqttPath, mqtt_topic_webserver_action, "--");
    _network->initTopic(_mqttPath, mqtt_topic_webserver_state, (_preferences->getBool(preference_webserver_enabled, true) ? "1" : "0"));

    _network->initTopic(_mqttPath, mqtt_topic_query_config, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_lockstate, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_battery, "0");
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
        _network->removeTopic(_mqttPath, mqtt_topic_keypad_command_result);
        _network->removeTopic(_mqttPath, mqtt_topic_config_button_enabled);
        _network->removeTopic(_mqttPath, mqtt_topic_config_led_enabled);
        _network->removeTopic(_mqttPath, mqtt_topic_config_led_brightness);
        _network->removeTopic(_mqttPath, mqtt_topic_config_auto_unlock);
        _network->removeTopic(_mqttPath, mqtt_topic_config_auto_lock);
        _network->removeTopic(_mqttPath, mqtt_topic_config_single_lock);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_level);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_critical);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_charging);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_voltage);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_drain);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_max_turn_current);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_lock_distance);
        _network->removeTopic(_mqttPath, mqtt_topic_battery_keypad_critical);
        //_network->removeTopic(_mqttPath, mqtt_topic_presence);
    }

    if(!_preferences->getBool(preference_conf_info_enabled, false))
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

    if(_preferences->getBool(preference_official_hybrid, false))
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        for(char* c=uidString; *c=toupper(*c); ++c);
        strcpy(_offMqttPath, "nuki/");
        strcat(_offMqttPath,uidString);

        for(const auto& offTopic : _offTopics)
        {
            _network->subscribe(_offMqttPath, offTopic);
        }
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

void NetworkLock::onMqttDataReceived(const char* topic, byte* payload, const unsigned int length)
{
    char* value = (char*)payload;

    if(comparePrefixedPath(topic, mqtt_topic_reset) && strcmp(value, "1") == 0)
    {
        Log->println(F("Restart requested via MQTT."));
        _network->clearWifiFallback();
        delay(200);
        restartEsp(RestartReason::RequestedViaMqtt);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_webserver_action))
    {
        if(strcmp(value, "") == 0 ||
           strcmp(value, "--") == 0) return;

        if(strcmp(value, "1") == 0)
        {
            if(_preferences->getBool(preference_webserver_enabled, true)) return;
            Log->println(F("Webserver enabled, restarting."));
            _preferences->putBool(preference_webserver_enabled, true);

        }
        else if (strcmp(value, "0") == 0)
        {
            if(!_preferences->getBool(preference_webserver_enabled, true)) return;
            Log->println(F("Webserver disabled, restarting."));
            _preferences->putBool(preference_webserver_enabled, false);
        }

        publishString(mqtt_topic_webserver_action, "--");
        _network->clearWifiFallback();
        delay(200);
        restartEsp(RestartReason::RequestedViaMqtt);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_lock_log_rolling_last))
    {
        if(strcmp(value, "") == 0 ||
           strcmp(value, "--") == 0) return;

        if(atoi(value) > 0 && atoi(value) > _lastRollingLog) _lastRollingLog = atoi(value);
    }

    if(_preferences->getBool(preference_official_hybrid, false))
    {
        for(auto offTopic : _offTopics)
        {
            if(comparePrefixedPath(topic, offTopic, true))
            {
                if(_officialUpdateReceivedCallback != nullptr)
                {
                    _officialUpdateReceivedCallback(offTopic, value);
                }
            }
        }
    }

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
    }

    if(comparePrefixedPath(topic, mqtt_topic_query_config) && strcmp(value, "1") == 0)
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

void NetworkLock::publishKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState, const NukiLock::KeyTurnerState& lastKeyTurnerState)
{
    char str[50];
    memset(&str, 0, sizeof(str));

    JsonDocument json;
    JsonDocument jsonBattery;

    if(!_offConnected)
    {
        lockstateToString(keyTurnerState.lockState, str);

        if(keyTurnerState.lockState != NukiLock::LockState::Undefined)
        {

            publishString(mqtt_topic_lock_state, str);

            if(_haEnabled)
            {
                publishState(keyTurnerState.lockState);
            }
        }

        json["lock_state"] = str;
    }
    else
    {
        lockstateToString((NukiLock::LockState)_offState, str);
        json["lock_state"] = str;
    }

    json["lockngo_state"] = (keyTurnerState.lockNgoTimer == 0 ? 0 : 1);

    memset(&str, 0, sizeof(str));

    if(!_offConnected)
    {
        triggerToString(keyTurnerState.trigger, str);

        if(_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
        {
            publishString(mqtt_topic_lock_trigger, str);
        }

        json["trigger"] = str;
    }
    else
    {
        triggerToString((NukiLock::Trigger)_offTrigger, str);
        json["trigger"] = str;
    }

    char curTime[20];
    sprintf(curTime, "%04d-%02d-%02d %02d:%02d:%02d", keyTurnerState.currentTimeYear, keyTurnerState.currentTimeMonth, keyTurnerState.currentTimeDay, keyTurnerState.currentTimeHour, keyTurnerState.currentTimeMinute, keyTurnerState.currentTimeSecond);
    json["currentTime"] = curTime;
    json["timeZoneOffset"] = keyTurnerState.timeZoneOffset;
    json["nightModeActive"] = keyTurnerState.nightModeActive;

    memset(&str, 0, sizeof(str));

    if(!_offConnected)
    {
        lockactionToString(keyTurnerState.lastLockAction, str);

        if(_firstTunerStatePublish || keyTurnerState.lastLockAction != lastKeyTurnerState.lastLockAction)
        {
            publishString(mqtt_topic_lock_last_lock_action, str);
        }

        json["last_lock_action"] = str;
    }
    else
    {
        lockactionToString((NukiLock::LockAction)_offLockAction, str);
        json["last_lock_action"] = str;
    }

    memset(&str, 0, sizeof(str));
    triggerToString(keyTurnerState.lastLockActionTrigger, str);
    json["last_lock_action_trigger"] = str;

    memset(&str, 0, sizeof(str));
    NukiLock::completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);

    if(_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        publishString(mqtt_topic_lock_completionStatus, str);
    }

    json["lock_completion_status"] = str;
    memset(&str, 0, sizeof(str));

    if(!_offConnected)
    {
        NukiLock::doorSensorStateToString(keyTurnerState.doorSensorState, str);

        if(_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
        {
            publishString(mqtt_topic_lock_door_sensor_state, str);
        }

        json["door_sensor_state"] = str;

        bool critical = (keyTurnerState.criticalBatteryState & 0b00000001) > 0;
        bool charging = (keyTurnerState.criticalBatteryState & 0b00000010) > 0;
        uint8_t level = (keyTurnerState.criticalBatteryState & 0b11111100) >> 1;
        bool keypadCritical = (keyTurnerState.accessoryBatteryState & (1 << 7)) != 0 ? (keyTurnerState.accessoryBatteryState & (1 << 6)) != 0 : false;

        jsonBattery["critical"] = critical ? "1" : "0";
        jsonBattery["charging"] = charging ? "1" : "0";
        jsonBattery["level"] = level;
        jsonBattery["keypadCritical"] = keypadCritical ? "1" : "0";

        if((_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState) && !_preferences->getBool(preference_disable_non_json, false))
        {
            publishBool(mqtt_topic_battery_critical, critical);
            publishBool(mqtt_topic_battery_charging, charging);
            publishInt(mqtt_topic_battery_level, level);
        }

        if((_firstTunerStatePublish || keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState) && !_preferences->getBool(preference_disable_non_json, false))
        {
            publishBool(mqtt_topic_battery_keypad_critical, keypadCritical);
        }
    }
    else
    {
        NukiLock::doorSensorStateToString((NukiLock::DoorSensorState)_offDoorsensorState, str);
        json["door_sensor_state"] = str;
    }

    json["auth_id"] = _authId;
    json["auth_name"] = _authName;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_lock_json, _buffer);

    serializeJson(jsonBattery, _buffer, _bufferSize);
    publishString(mqtt_topic_battery_basic_json, _buffer);

    _firstTunerStatePublish = false;
}

void NetworkLock::publishState(NukiLock::LockState lockState)
{
    switch(lockState)
    {
        case NukiLock::LockState::Locked:
            publishString(mqtt_topic_lock_ha_state, "locked");
            publishString(mqtt_topic_lock_binary_state, "locked");
            break;
        case NukiLock::LockState::Locking:
            publishString(mqtt_topic_lock_ha_state, "locking");
            publishString(mqtt_topic_lock_binary_state, "locked");
            break;
        case NukiLock::LockState::Unlocking:
            publishString(mqtt_topic_lock_ha_state, "unlocking");
            publishString(mqtt_topic_lock_binary_state, "unlocked");
            break;
        case NukiLock::LockState::Unlocked:
        case NukiLock::LockState::UnlockedLnga:
            publishString(mqtt_topic_lock_ha_state, "unlocked");
            publishString(mqtt_topic_lock_binary_state, "unlocked");
            break;
        case NukiLock::LockState::Unlatched:
            publishString(mqtt_topic_lock_ha_state, "open");
            publishString(mqtt_topic_lock_binary_state, "unlocked");
            break;
        case NukiLock::LockState::Unlatching:
            publishString(mqtt_topic_lock_ha_state, "opening");
            publishString(mqtt_topic_lock_binary_state, "unlocked");
            break;
        case NukiLock::LockState::Uncalibrated:
        case NukiLock::LockState::Calibration:
        case NukiLock::LockState::BootRun:
        case NukiLock::LockState::MotorBlocked:
            publishString(mqtt_topic_lock_ha_state, "jammed");
            break;
        default:
            break;
    }
}

void NetworkLock::publishAuthorizationInfo(const std::list<NukiLock::LogEntry>& logEntries, bool latest)
{
    char str[50];
    char authName[33];
    uint32_t authIndex = 0;

    JsonDocument json;

    for(const auto& log : logEntries)
    {
        memset(authName, 0, sizeof(authName));
        authName[0] = '\0';

        if((log.loggingType == NukiLock::LoggingType::LockAction || log.loggingType == NukiLock::LoggingType::KeypadAction))
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
        entry["authorizationName"] = authName;
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
            case NukiLock::LoggingType::LockAction:
                memset(str, 0, sizeof(str));
                NukiLock::lockactionToString((NukiLock::LockAction)log.data[0], str);
                entry["action"] = str;

                memset(str, 0, sizeof(str));
                NukiLock::triggerToString((NukiLock::Trigger)log.data[1], str);
                entry["trigger"] = str;

                memset(str, 0, sizeof(str));
                NukiLock::completionStatusToString((NukiLock::CompletionStatus)log.data[3], str);
                entry["completionStatus"] = str;
                break;
            case NukiLock::LoggingType::KeypadAction:
                memset(str, 0, sizeof(str));
                NukiLock::lockactionToString((NukiLock::LockAction)log.data[0], str);
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
                    NukiLock::completionStatusToString((NukiLock::CompletionStatus)log.data[2], str);
                    entry["completionStatus"] = str;
                }

                entry["codeId"] = 256U*log.data[4]+log.data[3];
                break;
            case NukiLock::LoggingType::DoorSensor:
                switch(log.data[0])
                {
                    case 0:
                        entry["action"] = "DoorOpened";
                        break;
                    case 1:
                        entry["action"] = "DoorClosed";
                        break;
                    case 2:
                        entry["action"] = "SensorJammed";
                        break;
                    default:
                        entry["action"] = "Unknown";
                        break;
                }
                break;
        }

        if(log.index > _lastRollingLog)
        {
            _lastRollingLog = log.index;
            serializeJson(entry, _buffer, _bufferSize);
            publishString(mqtt_topic_lock_log_rolling, _buffer);
            publishInt(mqtt_topic_lock_log_rolling_last, log.index);
        }
    }

    serializeJson(json, _buffer, _bufferSize);

    if(latest) publishString(mqtt_topic_lock_log_latest, _buffer);
    else publishString(mqtt_topic_lock_log, _buffer);

    if(authIndex > 0)
    {
        publishUInt(mqtt_topic_lock_auth_id, _authId);
        publishString(mqtt_topic_lock_auth_name, _authName);
    }
}

void NetworkLock::clearAuthorizationInfo()
{
    publishString(mqtt_topic_lock_log, "--");
    publishUInt(mqtt_topic_lock_auth_id, 0);
    publishString(mqtt_topic_lock_auth_name, "--");}

void NetworkLock::publishCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_lock_action_command_result, resultStr);
}

void NetworkLock::publishLockstateCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_query_lockstate_command_result, resultStr);
}

void NetworkLock::publishBatteryReport(const NukiLock::BatteryReport& batteryReport)
{
    if(!_preferences->getBool(preference_disable_non_json, false))
    {
        publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0);
        publishInt(mqtt_topic_battery_drain, batteryReport.batteryDrain); // milliwatt seconds
        publishFloat(mqtt_topic_battery_max_turn_current, (float)batteryReport.maxTurnCurrent / 1000.0);
        publishInt(mqtt_topic_battery_lock_distance, batteryReport.lockDistance); // degrees
    }

    char str[50];
    memset(&str, 0, sizeof(str));

    JsonDocument json;

    json["batteryDrain"] = batteryReport.batteryDrain;
    json["batteryVoltage"] = (float)batteryReport.batteryVoltage / 1000.0;
    json["critical"] = batteryReport.criticalBatteryState;
    lockactionToString(batteryReport.lockAction, str);
    json["lockAction"] = str;
    json["startVoltage"] = (float)batteryReport.startVoltage / 1000.0;
    json["lowestVoltage"] = (float)batteryReport.lowestVoltage / 1000.0;
    json["lockDistance"] = (float)batteryReport.lockDistance / 1000.0;
    json["startTemperature"] = batteryReport.startTemperature;
    json["maxTurnCurrent"] = (float)batteryReport.maxTurnCurrent / 1000.0;
    json["batteryResistance"] = (float)batteryReport.batteryResistance / 1000.0;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_battery_advanced_json, _buffer);
}

void NetworkLock::publishConfig(const NukiLock::Config &config)
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
    json["autoUnlatch"] = config.autoUnlatch;
    json["pairingEnabled"] = config.pairingEnabled;
    json["buttonEnabled"] = config.buttonEnabled;
    json["ledEnabled"] = config.ledEnabled;
    json["ledBrightness"] = config.ledBrightness;
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
    json["singleLock"] = config.singleLock;
    memset(str, 0, sizeof(str));
    _network->advertisingModeToString(config.advertisingMode, str);
    json["advertisingMode"] = str;
    json["hasKeypad"] = config.hasKeypad;
    json["hasKeypadV2"] = config.hasKeypadV2;
    json["firmwareVersion"] = std::to_string(config.firmwareVersion[0]) + "." + std::to_string(config.firmwareVersion[1]) + "." + std::to_string(config.firmwareVersion[2]);
    json["hardwareRevision"] = std::to_string(config.hardwareRevision[0]) + "." + std::to_string(config.hardwareRevision[1]);
    memset(str, 0, sizeof(str));
    homeKitStatusToString(config.homeKitStatus, str);
    json["homeKitStatus"] = str;
    memset(str, 0, sizeof(str));
    _network->timeZoneIdToString(config.timeZoneId, str);
    json["timeZone"] = str;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_config_basic_json, _buffer);

    if(!_preferences->getBool(preference_disable_non_json, false))
    {
        publishBool(mqtt_topic_config_button_enabled, config.buttonEnabled == 1);
        publishBool(mqtt_topic_config_led_enabled, config.ledEnabled == 1);
        publishInt(mqtt_topic_config_led_brightness, config.ledBrightness);
        publishBool(mqtt_topic_config_single_lock, config.singleLock == 1);
    }

    publishString(mqtt_topic_info_firmware_version, std::to_string(config.firmwareVersion[0]) + "." + std::to_string(config.firmwareVersion[1]) + "." + std::to_string(config.firmwareVersion[2]));
    publishString(mqtt_topic_info_hardware_version, std::to_string(config.hardwareRevision[0]) + "." + std::to_string(config.hardwareRevision[1]));
}

void NetworkLock::publishAdvancedConfig(const NukiLock::AdvancedConfig &config)
{
    char str[50];
    char nmst[6];
    sprintf(nmst, "%02d:%02d", config.nightModeStartTime[0], config.nightModeStartTime[1]);
    char nmet[6];
    sprintf(nmet, "%02d:%02d", config.nightModeEndTime[0], config.nightModeEndTime[1]);

    JsonDocument json;

    json["totalDegrees"] = config.totalDegrees;
    json["unlockedPositionOffsetDegrees"] = config.unlockedPositionOffsetDegrees;
    json["lockedPositionOffsetDegrees"] = config.lockedPositionOffsetDegrees;
    json["singleLockedPositionOffsetDegrees"] = config.singleLockedPositionOffsetDegrees;
    json["unlockedToLockedTransitionOffsetDegrees"] = config.unlockedToLockedTransitionOffsetDegrees;
    json["lockNgoTimeout"] = config.lockNgoTimeout;
    memset(str, 0, sizeof(str));
    buttonPressActionToString(config.singleButtonPressAction, str);
    json["singleButtonPressAction"] = str;
    memset(str, 0, sizeof(str));
    buttonPressActionToString(config.doubleButtonPressAction, str);
    json["doubleButtonPressAction"] = str;
    json["detachedCylinder"] = config.detachedCylinder;
    memset(str, 0, sizeof(str));
    _network->batteryTypeToString(config.batteryType, str);
    json["batteryType"] = str;
    json["automaticBatteryTypeDetection"] = config.automaticBatteryTypeDetection;
    json["unlatchDuration"] = config.unlatchDuration;
    json["autoLockTimeOut"] = config.autoLockTimeOut;
    json["autoUnLockDisabled"] = config.autoUnLockDisabled;
    json["nightModeEnabled"] = config.nightModeEnabled;
    json["nightModeStartTime"] = nmst;
    json["nightModeEndTime"] = nmet;
    json["nightModeAutoLockEnabled"] = config.nightModeAutoLockEnabled;
    json["nightModeAutoUnlockDisabled"] = config.nightModeAutoUnlockDisabled;
    json["nightModeImmediateLockOnStart"] = config.nightModeImmediateLockOnStart;
    json["autoLockEnabled"] = config.autoLockEnabled;
    json["immediateAutoLockEnabled"] = config.immediateAutoLockEnabled;
    json["autoUpdateEnabled"] = config.autoUpdateEnabled;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_config_advanced_json, _buffer);

    if(!_preferences->getBool(preference_disable_non_json, false))
    {
        publishBool(mqtt_topic_config_auto_unlock, config.autoUnLockDisabled == 0);
        publishBool(mqtt_topic_config_auto_lock, config.autoLockEnabled == 1);
    }
}

void NetworkLock::publishRssi(const int& rssi)
{
    publishInt(mqtt_topic_lock_rssi, rssi);
}

void NetworkLock::publishRetry(const std::string& message)
{
    publishString(mqtt_topic_lock_retry, message);
}

void NetworkLock::publishBleAddress(const std::string &address)
{
    publishString(mqtt_topic_lock_address, address);
}

void NetworkLock::publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount)
{
    uint index = 0;
    char uidString[20];
    itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_lock_path);
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
            publishString(basePath.c_str(), _buffer);

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
    publishString(mqtt_topic_keypad_json, _buffer);

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
            String codeTopic = "code_";
            codeTopic.concat(std::to_string(j).c_str());
            _network->removeTopic(codesTopic, codeTopic);
            std::string mqttDeviceName = std::string("keypad_") + std::to_string(j);
            _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
        }
    }
}

void NetworkLock::publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry)
{
    if(_preferences->getBool(preference_disable_non_json, false)) return;

    char codeName[sizeof(entry.name) + 1];
    memset(codeName, 0, sizeof(codeName));
    memcpy(codeName, entry.name, sizeof(entry.name));

    publishInt(concat(topic, "/id").c_str(), entry.codeId);
    publishBool(concat(topic, "/enabled").c_str(), entry.enabled);
    publishString(concat(topic, "/name").c_str(), codeName);

    if(_preferences->getBool(preference_keypad_publish_code, false))
    {
        publishInt(concat(topic, "/code").c_str(), entry.code);
    }

    publishInt(concat(topic, "/createdYear").c_str(), entry.dateCreatedYear);
    publishInt(concat(topic, "/createdMonth").c_str(), entry.dateCreatedMonth);
    publishInt(concat(topic, "/createdDay").c_str(), entry.dateCreatedDay);
    publishInt(concat(topic, "/createdHour").c_str(), entry.dateCreatedHour);
    publishInt(concat(topic, "/createdMin").c_str(), entry.dateCreatedMin);
    publishInt(concat(topic, "/createdSec").c_str(), entry.dateCreatedSec);
    publishInt(concat(topic, "/lockCount").c_str(), entry.lockCount);
}

void NetworkLock::publishTimeControl(const std::list<NukiLock::TimeControlEntry>& timeControlEntries, uint maxTimeControlEntryCount)
{
    uint index = 0;
    char str[50];
    char uidString[20];
    itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_lock_path);
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
        NukiLock::lockactionToString(entry.lockAction, str);
        jsonEntry["lockAction"] = str;

        if(_preferences->getBool(preference_timecontrol_topic_per_entry, false))
        {
            String basePath = mqtt_topic_timecontrol;
            basePath.concat("/entries/");
            basePath.concat(std::to_string(index).c_str());
            jsonEntry["index"] = index;
            serializeJson(jsonEntry, _buffer, _bufferSize);
            publishString(basePath.c_str(), _buffer);

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
                             (char*)"SmartLock",
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
    publishString(mqtt_topic_timecontrol_json, _buffer);
    
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

void NetworkLock::publishConfigCommandResult(const char* result)
{
    publishString(mqtt_topic_config_action_command_result, result);
}

void NetworkLock::publishKeypadCommandResult(const char* result)
{
    if(_preferences->getBool(preference_disable_non_json, false)) return;
    publishString(mqtt_topic_keypad_command_result, result);
}

void NetworkLock::publishKeypadJsonCommandResult(const char* result)
{
    publishString(mqtt_topic_keypad_json_command_result, result);
}

void NetworkLock::publishTimeControlCommandResult(const char* result)
{
    publishString(mqtt_topic_timecontrol_command_result, result);
}

void NetworkLock::publishStatusUpdated(const bool statusUpdated)
{
    publishBool(mqtt_topic_lock_status_updated, statusUpdated);
}

void NetworkLock::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NetworkLock::setOfficialUpdateReceivedCallback(void (*officialUpdateReceivedCallback)(const char *, const char *))
{
    _officialUpdateReceivedCallback = officialUpdateReceivedCallback;
}

void NetworkLock::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void NetworkLock::setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled))
{
    if(_preferences->getBool(preference_disable_non_json, false)) return;
    _keypadCommandReceivedReceivedCallback = keypadCommandReceivedReceivedCallback;
}

void NetworkLock::setKeypadJsonCommandReceivedCallback(void (*keypadJsonCommandReceivedReceivedCallback)(const char *))
{
    _keypadJsonCommandReceivedReceivedCallback = keypadJsonCommandReceivedReceivedCallback;
}

void NetworkLock::setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char *))
{
    _timeControlCommandReceivedReceivedCallback = timeControlCommandReceivedReceivedCallback;
}

void NetworkLock::buildMqttPath(const char* path, char* outPath, bool offPath)
{
    int offset = 0;
    char inPath[181] = {0};

    if(offPath) memcpy(inPath, _offMqttPath, sizeof(_offMqttPath));
    else memcpy(inPath, _mqttPath, sizeof(_mqttPath));

    for(const char& c : inPath)
    {
        if(c == 0x00)
        {
            break;
        }
        outPath[offset] = c;
        ++offset;
    }
    int i=0;
    while(outPath[i] != 0x00)
    {
        outPath[offset] = path[i];
        ++i;
        ++offset;
    }
    outPath[i+1] = 0x00;
}

bool NetworkLock::comparePrefixedPath(const char *fullPath, const char *subPath, bool offPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath, offPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

void NetworkLock::publishHASSConfig(char *deviceType, const char *baseTopic, char *name, char *uidString, const bool& hasDoorSensor, const bool& hasKeypad, const bool& publishAuthData, char *lockAction,
                               char *unlockAction, char *openAction)
{
    _network->publishHASSConfig(deviceType, baseTopic, name, uidString, "~/maintenance/mqttConnectionState", hasKeypad, lockAction, unlockAction, openAction);
    _network->publishHASSConfigAdditionalLockEntities(deviceType, baseTopic, name, uidString);

    if(hasDoorSensor)
    {
        _network->publishHASSConfigDoorSensor(deviceType, baseTopic, name, uidString);
    }
    else
    {
        _network->removeHASSConfigTopic((char*)"binary_sensor", (char*)"door_sensor", uidString);
    }
    _network->publishHASSWifiRssiConfig(deviceType, baseTopic, name, uidString);

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

void NetworkLock::removeHASSConfig(char *uidString)
{
    _network->removeHASSConfig(uidString);
}

void NetworkLock::publishOffAction(const int value)
{
    _network->publishInt(_offMqttPath, mqtt_topic_official_lock_action, value, false);
}

void NetworkLock::publishFloat(const char *topic, const float value, const uint8_t precision, bool retain)
{
    _network->publishFloat(_mqttPath, topic, value, precision, retain);
}

void NetworkLock::publishInt(const char *topic, const int value, bool retain)
{
    _network->publishInt(_mqttPath, topic, value, retain);
}

void NetworkLock::publishUInt(const char *topic, const unsigned int value, bool retain)
{
    _network->publishUInt(_mqttPath, topic, value, retain);
}

void NetworkLock::publishBool(const char *topic, const bool value, bool retain)
{
    _network->publishBool(_mqttPath, topic, value, retain);
}

bool NetworkLock::publishString(const char *topic, const String &value, bool retain)
{
    char str[value.length() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.begin(), value.length());
    return publishString(topic, str, retain);
}

bool NetworkLock::publishString(const char *topic, const std::string &value, bool retain)
{
    char str[value.size() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.data(), value.length());
    return publishString(topic, str, retain);
}

bool NetworkLock::publishString(const char *topic, const char *value, bool retain)
{
    return _network->publishString(_mqttPath, topic, value, retain);
}

void NetworkLock::publishULong(const char *topic, const unsigned long value, bool retain)
{
    return _network->publishULong(_mqttPath, topic, value, retain);
}

String NetworkLock::concat(String a, String b)
{
    String c = a;
    c.concat(b);
    return c;
}

bool NetworkLock::reconnected()
{
    bool r = _reconnected;
    _reconnected = false;
    return r;
}

uint8_t NetworkLock::queryCommands()
{
    uint8_t qc = _queryCommands;
    _queryCommands = 0;
    return qc;
}

void NetworkLock::buttonPressActionToString(const NukiLock::ButtonPressAction btnPressAction, char* str) {
  switch (btnPressAction) {
    case NukiLock::ButtonPressAction::NoAction:
      strcpy(str, "No Action");
      break;
    case NukiLock::ButtonPressAction::Intelligent:
      strcpy(str, "Intelligent");
      break;
    case NukiLock::ButtonPressAction::Unlock:
      strcpy(str, "Unlock");
      break;
    case NukiLock::ButtonPressAction::Lock:
      strcpy(str, "Lock");
      break;
    case NukiLock::ButtonPressAction::Unlatch:
      strcpy(str, "Unlatch");
      break;
    case NukiLock::ButtonPressAction::LockNgo:
      strcpy(str, "Lock n Go");
      break;
    case NukiLock::ButtonPressAction::ShowStatus:
      strcpy(str, "Show Status");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}

void NetworkLock::homeKitStatusToString(const int hkstatus, char* str) {
  switch (hkstatus) {
    case 0:
      strcpy(str, "Not Available");
      break;
    case 1:
      strcpy(str, "Disabled");
      break;
    case 2:
      strcpy(str, "Enabled");
      break;
    case 3:
      strcpy(str, "Enabled & Paired");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}

void NetworkLock::fobActionToString(const int fobact, char* str) {
  switch (fobact) {
    case 0:
      strcpy(str, "No Action");
      break;
    case 1:
      strcpy(str, "Unlock");
      break;
    case 2:
      strcpy(str, "Lock");
      break;
    case 3:
      strcpy(str, "Lock n Go");
      break;
    case 4:
      strcpy(str, "Intelligent");
      break;
    default:
      strcpy(str, "undefined");
      break;
  }
}