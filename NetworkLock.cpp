#include "NetworkLock.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <ArduinoJson.h>

NetworkLock::NetworkLock(Network* network, Preferences* preferences, char* buffer, size_t bufferSize)
: _network(network),
  _preferences(preferences),
  _buffer(buffer),
  _bufferSize(bufferSize)
{
    _configTopics.reserve(5);
    _configTopics.push_back(mqtt_topic_config_button_enabled);
    _configTopics.push_back(mqtt_topic_config_led_enabled);
    _configTopics.push_back(mqtt_topic_config_led_brightness);
    _configTopics.push_back(mqtt_topic_config_auto_unlock);
    _configTopics.push_back(mqtt_topic_config_auto_lock);
    _configTopics.push_back(mqtt_topic_config_single_lock);

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
    for(const auto& topic : _configTopics)
    {
        _network->subscribe(_mqttPath, topic);
    }

    _network->subscribe(_mqttPath, mqtt_topic_reset);
    _network->initTopic(_mqttPath, mqtt_topic_reset, "0");

    _network->initTopic(_mqttPath, mqtt_topic_query_config, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_lockstate, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_battery, "0");
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
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_action, "--");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_id, "0");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_name, "--");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_code, "000000");
        _network->initTopic(_mqttPath, mqtt_topic_keypad_command_enabled, "1");
        _network->initTopic(_mqttPath, mqtt_topic_query_keypad, "0");
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

    for(auto configTopic : _configTopics)
    {
        if(comparePrefixedPath(topic, configTopic))
        {
            if(_configUpdateReceivedCallback != nullptr)
            {
                _configUpdateReceivedCallback(configTopic, value);
            }
        }
    }
}

void NetworkLock::publishKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState, const NukiLock::KeyTurnerState& lastKeyTurnerState)
{
    char str[50];

    if((_firstTunerStatePublish || keyTurnerState.lockState != lastKeyTurnerState.lockState) && keyTurnerState.lockState != NukiLock::LockState::Undefined)
    {
        memset(&str, 0, sizeof(str));
        lockstateToString(keyTurnerState.lockState, str);
        publishString(mqtt_topic_lock_state, str);

        if(_haEnabled)
        {
            publishBinaryState(keyTurnerState.lockState);
        }
    }

    if(_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
    {
        memset(&str, 0, sizeof(str));
        triggerToString(keyTurnerState.trigger, str);
        publishString(mqtt_topic_lock_trigger, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.lastLockAction != lastKeyTurnerState.lastLockAction)
    {
        memset(&str, 0, sizeof(str));
        lockactionToString(keyTurnerState.lastLockAction, str);
        publishString(mqtt_topic_lock_last_lock_action, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        memset(&str, 0, sizeof(str));
        NukiLock::completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);
        publishString(mqtt_topic_lock_completionStatus, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
    {
        memset(&str, 0, sizeof(str));
        NukiLock::doorSensorStateToString(keyTurnerState.doorSensorState, str);
        publishString(mqtt_topic_lock_door_sensor_state, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState)
    {
        bool critical = (keyTurnerState.criticalBatteryState & 0b00000001) > 0;
        publishBool(mqtt_topic_battery_critical, critical);

        bool charging = (keyTurnerState.criticalBatteryState & 0b00000010) > 0;
        publishBool(mqtt_topic_battery_charging, charging);

        uint8_t level = (keyTurnerState.criticalBatteryState & 0b11111100) >> 1;
        publishInt(mqtt_topic_battery_level, level);
    }

    if(_firstTunerStatePublish || keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState)
    {
        if ((keyTurnerState.accessoryBatteryState & (1 << 7)) != 0) {
            publishBool(mqtt_topic_battery_keypad_critical, (keyTurnerState.accessoryBatteryState & (1 << 6)) != 0);
        }
        else
        {
            publishBool(mqtt_topic_battery_keypad_critical, false);
        }
    }

    _firstTunerStatePublish = false;
}

void NetworkLock::publishBinaryState(NukiLock::LockState lockState)
{
    switch(lockState)
    {
        case NukiLock::LockState::Locked:
        case NukiLock::LockState::Locking:
            publishString(mqtt_topic_lock_binary_state, "locked");
            break;
        case NukiLock::LockState::Unlocked:
        case NukiLock::LockState::Unlocking:
        case NukiLock::LockState::Unlatched:
        case NukiLock::LockState::Unlatching:
        case NukiLock::LockState::UnlockedLnga:
            publishString(mqtt_topic_lock_binary_state, "unlocked");
            break;
        default:
            break;
    }
}

void NetworkLock::publishAuthorizationInfo(const std::list<NukiLock::LogEntry>& logEntries)
{
    char str[50];

    bool authFound = false;
    uint32_t authId = 0;
    char authName[33];
    memset(authName, 0, sizeof(authName));

    DynamicJsonDocument json(_bufferSize);

    for(const auto& log : logEntries)
    {
        if((log.loggingType == NukiLock::LoggingType::LockAction || log.loggingType == NukiLock::LoggingType::KeypadAction) && ! authFound)
        {
            authFound = true;
            authId = log.authId;
            memcpy(authName, log.name, sizeof(log.name));
        }

        auto entry = json.add();

        entry["index"] = log.index;
        entry["authorizationId"] = log.authId;
        entry["authorizationName"] = log.name;
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

                memset(str, 0, sizeof(str));
                NukiLock::completionStatusToString((NukiLock::CompletionStatus)log.data[2], str);
                entry["completionStatus"] = str;
                break;
            case NukiLock::LoggingType::DoorSensor:
                memset(str, 0, sizeof(str));
                NukiLock::lockactionToString((NukiLock::LockAction)log.data[0], str);

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

                memset(str, 0, sizeof(str));
                NukiLock::completionStatusToString((NukiLock::CompletionStatus)log.data[2], str);
                entry["completionStatus"] = str;
                break;
        }
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_lock_log, _buffer);

    if(authFound)
    {
        publishUInt(mqtt_topic_lock_auth_id, authId);
        publishString(mqtt_topic_lock_auth_name, authName);
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
    publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0);
    publishInt(mqtt_topic_battery_drain, batteryReport.batteryDrain); // milliwatt seconds
    publishFloat(mqtt_topic_battery_max_turn_current, (float)batteryReport.maxTurnCurrent / 1000.0);
    publishInt(mqtt_topic_battery_lock_distance, batteryReport.lockDistance); // degrees
}

void NetworkLock::publishConfig(const NukiLock::Config &config)
{
    publishBool(mqtt_topic_config_button_enabled, config.buttonEnabled == 1);
    publishBool(mqtt_topic_config_led_enabled, config.ledEnabled == 1);
    publishInt(mqtt_topic_config_led_brightness, config.ledBrightness);
    publishBool(mqtt_topic_config_single_lock, config.singleLock == 1);
    publishString(mqtt_topic_info_firmware_version, std::to_string(config.firmwareVersion[0]) + "." + std::to_string(config.firmwareVersion[1]) + "." + std::to_string(config.firmwareVersion[2]));
    publishString(mqtt_topic_info_hardware_version, std::to_string(config.hardwareRevision[0]) + "." + std::to_string(config.hardwareRevision[1]));
}

void NetworkLock::publishAdvancedConfig(const NukiLock::AdvancedConfig &config)
{
    publishBool(mqtt_topic_config_auto_unlock, config.autoUnLockDisabled == 0);
    publishBool(mqtt_topic_config_auto_lock, config.autoLockEnabled == 1);
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
    for(const auto& entry : entries)
    {
        String basePath = mqtt_topic_keypad;
        basePath.concat("/code_");
        basePath.concat(std::to_string(index).c_str());
        publishKeypadEntry(basePath, entry);

        ++index;
    }
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

void NetworkLock::publishKeypadCommandResult(const char* result)
{
    publishString(mqtt_topic_keypad_command_result, result);
}

void NetworkLock::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NetworkLock::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *, const char *))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void NetworkLock::setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled))
{
    _keypadCommandReceivedReceivedCallback = keypadCommandReceivedReceivedCallback;
}

void NetworkLock::buildMqttPath(const char* path, char* outPath)
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
    while(outPath[i] != 0x00)
    {
        outPath[offset] = path[i];
        ++i;
        ++offset;
    }
    outPath[i+1] = 0x00;
}

bool NetworkLock::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

void NetworkLock::publishHASSConfig(char *deviceType, const char *baseTopic, char *name, char *uidString, const bool& hasDoorSensor, const bool& hasKeypad, const bool& publishAuthData, char *lockAction,
                               char *unlockAction, char *openAction, char *lockedState, char *unlockedState)
{
    _network->publishHASSConfig(deviceType, baseTopic, name, uidString, "~/maintenance/mqttConnectionState", hasKeypad, lockAction, unlockAction, openAction, lockedState, unlockedState);
    _network->publishHASSConfigBatLevel(deviceType, baseTopic, name, uidString);
    _network->publishHASSConfigLedBrightness(deviceType, baseTopic, name, uidString);
    if(hasDoorSensor)
    {
        _network->publishHASSConfigDoorSensor(deviceType, baseTopic, name, uidString, lockAction, unlockAction, openAction, lockedState, unlockedState);
    }
    else
    {
        _network->removeHASSConfigTopic("binary_sensor", "door_sensor", uidString);
    }
    _network->publishHASSWifiRssiConfig(deviceType, baseTopic, name, uidString);
    _network->publishHASSBleRssiConfig(deviceType, baseTopic, name, uidString);

    if(publishAuthData)
    {
        _network->publishHASSConfigAccessLog(deviceType, baseTopic, name, uidString);
    }
    else
    {
        _network->removeHASSConfigTopic("sensor", "last_action_authorization", uidString);
    }

    if(hasKeypad)
    {
        _network->publishHASSConfigKeypadAttemptInfo(deviceType, baseTopic, name, uidString);
    }
    else
    {
        _network->removeHASSConfigTopic("sensor", "keypad_status", uidString);
    }
}

void NetworkLock::removeHASSConfig(char *uidString)
{
    _network->removeHASSConfig(uidString);
}

void NetworkLock::publishFloat(const char *topic, const float value, const uint8_t precision)
{
    _network->publishFloat(_mqttPath, topic, value, precision);
}

void NetworkLock::publishInt(const char *topic, const int value)
{
    _network->publishInt(_mqttPath, topic, value);
}

void NetworkLock::publishUInt(const char *topic, const unsigned int value)
{
    _network->publishUInt(_mqttPath, topic, value);
}

void NetworkLock::publishBool(const char *topic, const bool value)
{
    _network->publishBool(_mqttPath, topic, value);
}

bool NetworkLock::publishString(const char *topic, const String &value)
{
    char str[value.length() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.begin(), value.length());
    return publishString(topic, str);
}

bool NetworkLock::publishString(const char *topic, const std::string &value)
{
    char str[value.size() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.data(), value.length());
    return publishString(topic, str);
}

bool NetworkLock::publishString(const char *topic, const char *value)
{
    return _network->publishString(_mqttPath, topic, value);
}

void NetworkLock::publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry)
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

void NetworkLock::publishULong(const char *topic, const unsigned long value)
{
    return _network->publishULong(_mqttPath, topic, value);
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
