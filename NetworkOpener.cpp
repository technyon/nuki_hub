#include "NetworkOpener.h"
#include "Arduino.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"

NetworkOpener::NetworkOpener(Network* network, Preferences* preferences)
        : _preferences(preferences),
          _network(network)
{
    _configTopics.reserve(5);
    _configTopics.push_back(mqtt_topic_config_button_enabled);
    _configTopics.push_back(mqtt_topic_config_led_enabled);
    _configTopics.push_back(mqtt_topic_config_sound_level);

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
    for(const auto& topic : _configTopics)
    {
        _network->subscribe(_mqttPath, topic);
    }

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

void NetworkOpener::update()
{
    if(_resetLockStateTs != 0 && millis() >= _resetLockStateTs)
    {
        char str[50];
        memset(str, 0, sizeof(str));
        _resetLockStateTs = 0;
        lockstateToString(NukiOpener::LockState::Locked, str);
        publishString(mqtt_topic_lock_state, str);
    }
}

void NetworkOpener::onMqttDataReceived(const char* topic, byte* payload, const unsigned int length)
{
    char* value = (char*)payload;

    if(comparePrefixedPath(topic, mqtt_topic_lock_action))
    {
        if(strcmp((char*)payload, "") == 0 || strcmp(value, "--") == 0 || strcmp(value, "ack") == 0 || strcmp(value, "unknown_action") == 0) return;

        Log->print(F("Opener lock action received: "));
        Log->println(value);
        bool success = false;
        if(_lockActionReceivedCallback != NULL)
        {
            success = _lockActionReceivedCallback(value);
        }
        publishString(mqtt_topic_lock_action, success ? "ack" : "unknown_action");
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

void NetworkOpener::publishKeyTurnerState(const NukiOpener::OpenerState& keyTurnerState, const NukiOpener::OpenerState& lastKeyTurnerState)
{
    char str[50];

    if((_firstTunerStatePublish || keyTurnerState.lockState != lastKeyTurnerState.lockState || keyTurnerState.nukiState != lastKeyTurnerState.nukiState) && keyTurnerState.lockState != NukiOpener::LockState::Undefined)
    {
        memset(&str, 0, sizeof(str));

        if(keyTurnerState.nukiState == NukiOpener::State::ContinuousMode)
        {
            publishString(mqtt_topic_lock_state, "ContinuousMode");
        }
        else
        {
            lockstateToString(keyTurnerState.lockState, str);
            publishString(mqtt_topic_lock_state, str);
        }

        if(_haEnabled)
        {
            publishBinaryState(keyTurnerState);
        }
    }

    if(_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
    {
        memset(&str, 0, sizeof(str));
        triggerToString(keyTurnerState.trigger, str);
        publishString(mqtt_topic_lock_trigger, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        memset(&str, 0, sizeof(str));
        NukiOpener::completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);
        publishString(mqtt_topic_lock_completionStatus, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
    {
        memset(&str, 0, sizeof(str));
        NukiOpener::doorSensorStateToString(keyTurnerState.doorSensorState, str);
        publishString(mqtt_topic_lock_door_sensor_state, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState)
    {
        bool critical = (keyTurnerState.criticalBatteryState & 0b00000001) > 0;
        publishBool(mqtt_topic_battery_critical, critical);
    }

    _firstTunerStatePublish = false;
}

void NetworkOpener::publishRing()
{
    publishString(mqtt_topic_lock_state, "ring");
    _resetLockStateTs = millis() + 2000;
}

void NetworkOpener::publishBinaryState(NukiOpener::OpenerState lockState)
{
    if(lockState.nukiState == NukiOpener::State::ContinuousMode)
    {
        publishString(mqtt_topic_lock_binary_state, "unlocked");
    }
    else
    {
        switch (lockState.lockState)
        {
            case NukiOpener::LockState::Locked:
                publishString(mqtt_topic_lock_binary_state, "locked");
                break;
            case NukiOpener::LockState::RTOactive:
            case NukiOpener::LockState::Open:
            case NukiOpener::LockState::Opening:
                publishString(mqtt_topic_lock_binary_state, "unlocked");
                break;
            default:
                break;
        }
    }
}

void NetworkOpener::publishAuthorizationInfo(const std::list<NukiOpener::LogEntry>& logEntries)
{
    char str[50];

    bool authFound = false;
    uint32_t authId = 0;
    char authName[33];
    memset(authName, 0, sizeof(authName));

    String json = "[\n";

    for(const auto& log : logEntries)
    {
        if((log.loggingType == NukiOpener::LoggingType::LockAction || log.loggingType == NukiOpener::LoggingType::KeypadAction || log.loggingType == NukiOpener::LoggingType::DoorbellRecognition) && ! authFound)
        {
            authFound = true;
            authId = log.authId;
            memcpy(authName, log.name, sizeof(log.name));
        }

        json.concat("{\n");

        json.concat("\"index\": "); json.concat(log.index); json.concat(",\n");
        json.concat("\"authorizationId\": "); json.concat(log.authId); json.concat(",\n");

        memset(str, 0, sizeof(str));
        memcpy(str, log.name, sizeof(log.name));
        json.concat("\"authorizationName\": \""); json.concat(str); json.concat("\",\n");

        json.concat("\"timeYear\": "); json.concat(log.timeStampYear); json.concat(",\n");
        json.concat("\"timeMonth\": "); json.concat(log.timeStampMonth); json.concat(",\n");
        json.concat("\"timeDay\": "); json.concat(log.timeStampDay); json.concat(",\n");
        json.concat("\"timeHour\": "); json.concat(log.timeStampHour); json.concat(",\n");
        json.concat("\"timeMinute\": "); json.concat(log.timeStampMinute); json.concat(",\n");
        json.concat("\"timeSecond\": "); json.concat(log.timeStampSecond); json.concat(",\n");

        memset(str, 0, sizeof(str));
        loggingTypeToString(log.loggingType, str);
        json.concat("\"type\": \""); json.concat(str); json.concat("\",\n");

        switch(log.loggingType)
        {
            case NukiOpener::LoggingType::LockAction:
                memset(str, 0, sizeof(str));
                NukiLock::lockactionToString((NukiLock::LockAction)log.data[0], str);
                json.concat("\"action\": \""); json.concat(str); json.concat("\",\n");

                memset(str, 0, sizeof(str));
                NukiLock::triggerToString((NukiLock::Trigger)log.data[1], str);
                json.concat("\"trigger\": \""); json.concat(str); json.concat("\",\n");

                memset(str, 0, sizeof(str));
                logactionCompletionStatusToString(log.data[3], str);
                json.concat("\"completionStatus\": \""); json.concat(str); json.concat("\"\n");
                break;
            case NukiOpener::LoggingType::KeypadAction:
                memset(str, 0, sizeof(str));
                NukiLock::lockactionToString((NukiLock::LockAction)log.data[0], str);
                json.concat("\"action\": \""); json.concat(str); json.concat("\",\n");

                memset(str, 0, sizeof(str));
                NukiLock::completionStatusToString((NukiLock::CompletionStatus)log.data[2], str);
                json.concat("\"completionStatus\": \""); json.concat(str); json.concat("\"\n");
                break;
            case NukiOpener::LoggingType::DoorbellRecognition:
                json.concat("\"mode\": \"");
                switch(log.data[0] & 3)
                {
                    case 0:
                        json.concat("None");
                        break;
                    case 1:
                        json.concat("RTO");
                        break;
                    case 2:
                        json.concat("CM");
                        break;
                    default:
                        json.concat("Unknown");
                        break;
                }
                json.concat("\",\n");

                json.concat("\"source\": \"");
                switch(log.data[1])
                {
                    case 0:
                        json.concat("Doorbell");
                        break;
                    case 1:
                        json.concat("Timecontrol");
                        break;
                    case 2:
                        json.concat("App");
                        break;
                    case 3:
                        json.concat("Button");
                        break;
                    case 4:
                        json.concat("Fob");
                        break;
                    case 5:
                        json.concat("Bridge");
                        break;
                    case 6:
                        json.concat("Keypad");
                        break;
                }
                json.concat("\",\n");

                json.concat("\"geofence\": \""); json.concat(log.data[2] == 1 ? "active" : "inactive"); json.concat("\",\n");
                json.concat("\"doorbellSuppression\": \""); json.concat(log.data[3] == 1 ? "active" : "inactive"); json.concat("\",\n");

                memset(str, 0, sizeof(str));
                logactionCompletionStatusToString(log.data[5], str);
                json.concat("\"completionStatus\": \""); json.concat(str); json.concat("\"\n");

                break;
        }

        json.concat("}");
        if(&log == &logEntries.back())
        {
            json.concat("\n");
        }
        else
        {
            json.concat(",\n");
        }
    }

    json.concat("]");
    publishString(mqtt_topic_lock_log, json);

    if(authFound)
    {
        publishUInt(mqtt_topic_lock_auth_id, authId);
        publishString(mqtt_topic_lock_auth_name, authName);
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

void NetworkOpener::publishBatteryReport(const NukiOpener::BatteryReport& batteryReport)
{
    publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0);
}

void NetworkOpener::publishConfig(const NukiOpener::Config &config)
{
    publishBool(mqtt_topic_config_button_enabled, config.buttonEnabled == 1);
    publishBool(mqtt_topic_config_led_enabled, config.ledFlashEnabled == 1);
}

void NetworkOpener::publishAdvancedConfig(const NukiOpener::AdvancedConfig &config)
{
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

void NetworkOpener::publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState)
{
    _network->publishHASSConfig(deviceType, baseTopic, name, uidString, false, lockAction, unlockAction, openAction, lockedState, unlockedState);
    _network->publishHASSConfigRingDetect(deviceType, baseTopic, name, uidString);
    _network->publishHASSConfigSoundLevel(deviceType, baseTopic, name, uidString);
    _network->publishHASSBleRssiConfig(deviceType, baseTopic, name, uidString);
}

void NetworkOpener::removeHASSConfig(char* uidString)
{
    _network->removeHASSConfig(uidString);
}

void NetworkOpener::publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount)
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

void NetworkOpener::publishKeypadCommandResult(const char* result)
{
    publishString(mqtt_topic_keypad_command_result, result);
}

void NetworkOpener::setLockActionReceivedCallback(bool (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NetworkOpener::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *, const char *))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void NetworkOpener::setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled))
{
    _keypadCommandReceivedReceivedCallback = keypadCommandReceivedReceivedCallback;
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
