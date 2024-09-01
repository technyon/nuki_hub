#include "NukiNetworkLock.h"
#include "Arduino.h"
#include "Config.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <ArduinoJson.h>
#include <ctype.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

extern bool forceEnableWebServer;
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

NukiNetworkLock::NukiNetworkLock(NukiNetwork* network, NukiOfficial* nukiOfficial, Preferences* preferences, char* buffer, size_t bufferSize)
: _network(network),
  _nukiOfficial(nukiOfficial),
  _preferences(preferences),
  _buffer(buffer),
  _bufferSize(bufferSize)
{
    _nukiPublisher = new NukiPublisher(network, _mqttPath);

    memset(_authName, 0, sizeof(_authName));
    _authName[0] = '\0';

    _nukiOfficial->offTopics.reserve(10);
    //_nukiOfficial->_offTopics.push_back(mqtt_topic_nukiOfficial->_official_mode);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_state);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_batteryCritical);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_batteryChargeState);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_batteryCharging);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_keypadBatteryCritical);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_doorsensorState);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_doorsensorBatteryCritical);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_connected);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_commandResponse);
    _nukiOfficial->offTopics.push_back((char*)mqtt_topic_official_lockActionEvent);

    _network->registerMqttReceiver(this);
}

NukiNetworkLock::~NukiNetworkLock()
{
}

void NukiNetworkLock::initialize()
{
    String mqttPath = _preferences->getString(preference_mqtt_lock_path, "");
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

    _haEnabled = _preferences->getString(preference_mqtt_hass_discovery, "") != "";
    _disableNonJSON = _preferences->getBool(preference_disable_non_json, false);
    _nukiOfficial->offEnabled = _preferences->getBool(preference_official_hybrid, false);

    _network->initTopic(_mqttPath, mqtt_topic_lock_action, "--");
    _network->subscribe(_mqttPath, mqtt_topic_lock_action);
    _network->initTopic(_mqttPath, mqtt_topic_config_action, "--");
    _network->subscribe(_mqttPath, mqtt_topic_config_action);
    _network->subscribe(_mqttPath, mqtt_topic_reset);
    _network->initTopic(_mqttPath, mqtt_topic_reset, "0");

    if(_preferences->getBool(preference_update_from_mqtt, false))
    {
        _network->subscribe(_mqttPath, mqtt_topic_update);
        _network->initTopic(_mqttPath, mqtt_topic_update, "0");
    }

    _network->subscribe(_mqttPath, mqtt_topic_webserver_action);
    _network->initTopic(_mqttPath, mqtt_topic_webserver_action, "--");
    _network->initTopic(_mqttPath, mqtt_topic_webserver_state, (_preferences->getBool(preference_webserver_enabled, true) || forceEnableWebServer ? "1" : "0"));

    _network->initTopic(_mqttPath, mqtt_topic_query_config, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_lockstate, "0");
    _network->initTopic(_mqttPath, mqtt_topic_query_battery, "0");
    _network->subscribe(_mqttPath, mqtt_topic_query_config);
    _network->subscribe(_mqttPath, mqtt_topic_query_lockstate);
    _network->subscribe(_mqttPath, mqtt_topic_query_battery);

    if(_disableNonJSON)
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
        if(!_disableNonJSON)
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

    if(_nukiOfficial->offEnabled)
    {
        _nukiOfficial->setUid(_preferences->getUInt(preference_nuki_id_lock, 0));

        for(const auto& offTopic : _nukiOfficial->offTopics)
        {
            _network->subscribe(_nukiOfficial->getMqttPath(), offTopic);
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

void NukiNetworkLock::onMqttDataReceived(const char* topic, byte* payload, const unsigned int length)
{
    char* value = (char*)payload;

    if(_network->mqttRecentlyConnected() && _network->pathEquals(_mqttPath, mqtt_topic_lock_action, topic))
    {
        Log->println("MQTT recently connected, ignoring lock action.");
        return;
    }

    if(comparePrefixedPath(topic, mqtt_topic_reset) && strcmp(value, "1") == 0)
    {
        Log->println(F("Restart requested via MQTT."));
        _network->clearWifiFallback();
        delay(200);
        restartEsp(RestartReason::RequestedViaMqtt);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_update) && strcmp(value, "1") == 0 && _preferences->getBool(preference_update_from_mqtt, false))
    {
        Log->println(F("Update requested via MQTT."));

        bool otaManifestSuccess = false;
        JsonDocument doc;

        NetworkClientSecure *client = new NetworkClientSecure;
        if (client) {
            client->setCACertBundle(x509_crt_imported_bundle_bin_start, x509_crt_imported_bundle_bin_end - x509_crt_imported_bundle_bin_start);
            {
                HTTPClient https;
                https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                https.useHTTP10(true);

                if (https.begin(*client, GITHUB_OTA_MANIFEST_URL)) {
                    int httpResponseCode = https.GET();

                    if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY)
                    {
                        DeserializationError jsonError = deserializeJson(doc, https.getStream());

                        if (!jsonError) { otaManifestSuccess = true; }
                    }
                }
                https.end();
            }
            delete client;
        }

        if (otaManifestSuccess)
        {
            String currentVersion = NUKI_HUB_VERSION;

            if(atof(doc["release"]["version"]) >= atof(currentVersion.c_str()))
            {
                if(strcmp(NUKI_HUB_VERSION, doc["release"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["release"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["release"]["time"].as<const char*>()) == 0)
                {
                    Log->println(F("Nuki Hub is already on the latest release version, OTA update aborted."));
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL);
                    Log->println(F("Updating to latest release version."));
                    delay(200);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
            else if(currentVersion.indexOf("beta") > 0)
            {
                if(strcmp(NUKI_HUB_VERSION, doc["beta"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["beta"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["beta"]["time"].as<const char*>()) == 0)
                {
                    Log->println(F("Nuki Hub is already on the latest beta version, OTA update aborted."));
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_BETA_RELEASE_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_BETA_UPDATER_BINARY_URL);
                    Log->println(F("Updating to latest beta version."));
                    delay(200);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
            else if(currentVersion.indexOf("master") > 0)
            {
                if(strcmp(NUKI_HUB_VERSION, doc["master"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["master"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["master"]["time"].as<const char*>()) == 0)
                {
                    Log->println(F("Nuki Hub is already on the latest development version, OTA update aborted."));
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_RELEASE_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_MASTER_UPDATER_BINARY_URL);
                    Log->println(F("Updating to latest developmemt version."));
                    delay(200);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
            else
            {
                if(strcmp(NUKI_HUB_VERSION, doc["release"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["release"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["release"]["time"].as<const char*>()) == 0)
                {
                    Log->println(F("Nuki Hub is already on the latest release version, OTA update aborted."));
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL);
                    Log->println(F("Updating to latest release version."));
                    delay(200);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
        }
        else
        {
            Log->println(F("Failed to retrieve OTA manifest, OTA update aborted."));
        }
    }
    else if(comparePrefixedPath(topic, mqtt_topic_webserver_action))
    {
        if(strcmp(value, "") == 0 ||
           strcmp(value, "--") == 0) return;

        if(strcmp(value, "1") == 0)
        {
            if(_preferences->getBool(preference_webserver_enabled, true) || forceEnableWebServer) return;
            Log->println(F("Webserver enabled, restarting."));
            _preferences->putBool(preference_webserver_enabled, true);

        }
        else if (strcmp(value, "0") == 0)
        {
            if(!_preferences->getBool(preference_webserver_enabled, true) && !forceEnableWebServer) return;
            Log->println(F("Webserver disabled, restarting."));
            _preferences->putBool(preference_webserver_enabled, false);
        }

        publishString(mqtt_topic_webserver_action, "--", true);
        _network->clearWifiFallback();
        delay(200);
        restartEsp(RestartReason::ReconfigureWebServer);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_lock_log_rolling_last))
    {
        if(strcmp(value, "") == 0 ||
           strcmp(value, "--") == 0) return;

        if(atoi(value) > 0 && atoi(value) > _lastRollingLog) _lastRollingLog = atoi(value);
    }

    if(_nukiOfficial->offEnabled)
    {
        for(auto offTopic : _nukiOfficial->offTopics)
        {
            if(_nukiOfficial->comparePrefixedPath(topic, offTopic))
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

    if(!_disableNonJSON)
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

void NukiNetworkLock::publishKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState, const NukiLock::KeyTurnerState& lastKeyTurnerState)
{
    char str[50];
    memset(&str, 0, sizeof(str));

    JsonDocument json;
    JsonDocument jsonBattery;

    if(!_nukiOfficial->offConnected)
    {
        lockstateToString(keyTurnerState.lockState, str);

        if(keyTurnerState.lockState != NukiLock::LockState::Undefined)
        {

            publishString(mqtt_topic_lock_state, str, true);

            if(_haEnabled)
            {
                publishState(keyTurnerState.lockState);
            }
        }

        json["lock_state"] = str;
    }
    else
    {
        lockstateToString((NukiLock::LockState)_nukiOfficial->offState, str);
        json["lock_state"] = str;
    }

    json["lockngo_state"] = (keyTurnerState.lockNgoTimer == 0 ? 0 : 1);

    memset(&str, 0, sizeof(str));

    if(!_nukiOfficial->offConnected)
    {
        triggerToString(keyTurnerState.trigger, str);

        if(_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
        {
            publishString(mqtt_topic_lock_trigger, str, true);
        }

        json["trigger"] = str;
    }
    else
    {
        triggerToString((NukiLock::Trigger)_nukiOfficial->offTrigger, str);
        json["trigger"] = str;
    }

    char curTime[20];
    sprintf(curTime, "%04d-%02d-%02d %02d:%02d:%02d", keyTurnerState.currentTimeYear, keyTurnerState.currentTimeMonth, keyTurnerState.currentTimeDay, keyTurnerState.currentTimeHour, keyTurnerState.currentTimeMinute, keyTurnerState.currentTimeSecond);
    json["currentTime"] = curTime;
    json["timeZoneOffset"] = keyTurnerState.timeZoneOffset;
    json["nightModeActive"] = keyTurnerState.nightModeActive;

    memset(&str, 0, sizeof(str));

    if(!_nukiOfficial->offConnected)
    {
        lockactionToString(keyTurnerState.lastLockAction, str);

        if(_firstTunerStatePublish || keyTurnerState.lastLockAction != lastKeyTurnerState.lastLockAction)
        {
            publishString(mqtt_topic_lock_last_lock_action, str, true);
        }

        json["last_lock_action"] = str;
    }
    else
    {
        lockactionToString((NukiLock::LockAction)_nukiOfficial->offLockAction, str);
        json["last_lock_action"] = str;
    }

    memset(&str, 0, sizeof(str));
    triggerToString(keyTurnerState.lastLockActionTrigger, str);
    json["last_lock_action_trigger"] = str;

    memset(&str, 0, sizeof(str));
    NukiLock::completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);

    if(_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        publishString(mqtt_topic_lock_completionStatus, str, true);
    }

    json["lock_completion_status"] = str;
    memset(&str, 0, sizeof(str));

    if(!_nukiOfficial->offConnected)
    {
        NukiLock::doorSensorStateToString(keyTurnerState.doorSensorState, str);

        if(_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
        {
            publishString(mqtt_topic_lock_door_sensor_state, str, true);
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

        if((_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState) && !_disableNonJSON)
        {
            publishBool(mqtt_topic_battery_critical, critical, true);
            publishBool(mqtt_topic_battery_charging, charging, true);
            publishInt(mqtt_topic_battery_level, level, true);
        }

        if((_firstTunerStatePublish || keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState) && !_disableNonJSON)
        {
            publishBool(mqtt_topic_battery_keypad_critical, keypadCritical, true);
        }

        serializeJson(jsonBattery, _buffer, _bufferSize);
        publishString(mqtt_topic_battery_basic_json, _buffer, true);
    }
    else
    {
        NukiLock::doorSensorStateToString((NukiLock::DoorSensorState)_nukiOfficial->offDoorsensorState, str);
        json["door_sensor_state"] = str;
    }

    json["auth_id"] = getAuthId();
    json["auth_name"] = _authName;

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_lock_json, _buffer, true);

    _firstTunerStatePublish = false;
}

void NukiNetworkLock::publishState(NukiLock::LockState lockState)
{
    switch(lockState)
    {
        case NukiLock::LockState::Locked:
            publishString(mqtt_topic_lock_ha_state, "locked", true);
            publishString(mqtt_topic_lock_binary_state, "locked", true);
            break;
        case NukiLock::LockState::Locking:
            publishString(mqtt_topic_lock_ha_state, "locking", true);
            publishString(mqtt_topic_lock_binary_state, "locked", true);
            break;
        case NukiLock::LockState::Unlocking:
            publishString(mqtt_topic_lock_ha_state, "unlocking", true);
            publishString(mqtt_topic_lock_binary_state, "unlocked", true);
            break;
        case NukiLock::LockState::Unlocked:
        case NukiLock::LockState::UnlockedLnga:
            publishString(mqtt_topic_lock_ha_state, "unlocked", true);
            publishString(mqtt_topic_lock_binary_state, "unlocked", true);
            break;
        case NukiLock::LockState::Unlatched:
            publishString(mqtt_topic_lock_ha_state, "open", true);
            publishString(mqtt_topic_lock_binary_state, "unlocked", true);
            break;
        case NukiLock::LockState::Unlatching:
            publishString(mqtt_topic_lock_ha_state, "opening", true);
            publishString(mqtt_topic_lock_binary_state, "unlocked", true);
            break;
        case NukiLock::LockState::Uncalibrated:
        case NukiLock::LockState::Calibration:
        case NukiLock::LockState::BootRun:
        case NukiLock::LockState::MotorBlocked:
            publishString(mqtt_topic_lock_ha_state, "jammed", true);
            break;
        default:
            break;
    }
}

void NukiNetworkLock::publishAuthorizationInfo(const std::list<NukiLock::LogEntry>& logEntries, bool latest)
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
                _authId = log.authId;
                _nukiOfficial->clearAuthId();
                memset(_authName, 0, sizeof(_authName));
                memcpy(_authName, authName, sizeof(authName));

                if(authName[sizeName - 1] != '\0' && _authEntries.count(getAuthId()) > 0)
                {
                    memset(_authName, 0, sizeof(_authName));
                    memcpy(_authName, _authEntries[getAuthId()].c_str(), sizeof(_authEntries[getAuthId()].c_str()));
                }
            }
        }

        auto entry = json.add<JsonVariant>();

        entry["index"] = log.index;
        entry["authorizationId"] = log.authId;
        entry["authorizationName"] = authName;

        if(entry["authorizationName"].as<String>().length() == 0 && _authEntries.count(log.authId) > 0)
        {
           entry["authorizationName"] = _authEntries[log.authId];
        }

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
            publishString(mqtt_topic_lock_log_rolling, _buffer, true);
            publishInt(mqtt_topic_lock_log_rolling_last, log.index, true);
        }
    }

    serializeJson(json, _buffer, _bufferSize);

    if(latest) publishString(mqtt_topic_lock_log_latest, _buffer, true);
    else publishString(mqtt_topic_lock_log, _buffer, true);

    if(authIndex > 0)
    {
        publishUInt(mqtt_topic_lock_auth_id, getAuthId(), true);
        publishString(mqtt_topic_lock_auth_name, _authName, true);
    }
}

void NukiNetworkLock::clearAuthorizationInfo()
{
    publishString(mqtt_topic_lock_log, "--", true);
    publishUInt(mqtt_topic_lock_auth_id, 0, true);
    publishString(mqtt_topic_lock_auth_name, "--", true);
}

void NukiNetworkLock::publishCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_lock_action_command_result, resultStr, true);
}

void NukiNetworkLock::publishLockstateCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_query_lockstate_command_result, resultStr, true);
}

void NukiNetworkLock::publishBatteryReport(const NukiLock::BatteryReport& batteryReport)
{
    if(!_disableNonJSON)
    {
        publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0, true);
        publishInt(mqtt_topic_battery_drain, batteryReport.batteryDrain, true); // milliwatt seconds
        publishFloat(mqtt_topic_battery_max_turn_current, (float)batteryReport.maxTurnCurrent / 1000.0, true);
        publishInt(mqtt_topic_battery_lock_distance, batteryReport.lockDistance, true); // degrees
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
    publishString(mqtt_topic_battery_advanced_json, _buffer, true);
}

void NukiNetworkLock::publishConfig(const NukiLock::Config &config)
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
    publishString(mqtt_topic_config_basic_json, _buffer, true);

    if(!_disableNonJSON)
    {
        publishBool(mqtt_topic_config_button_enabled, config.buttonEnabled == 1, true);
        publishBool(mqtt_topic_config_led_enabled, config.ledEnabled == 1, true);
        publishInt(mqtt_topic_config_led_brightness, config.ledBrightness, true);
        publishBool(mqtt_topic_config_single_lock, config.singleLock == 1, true);
    }

    publishString(mqtt_topic_info_firmware_version, std::to_string(config.firmwareVersion[0]) + "." + std::to_string(config.firmwareVersion[1]) + "." + std::to_string(config.firmwareVersion[2]), true);
    publishString(mqtt_topic_info_hardware_version, std::to_string(config.hardwareRevision[0]) + "." + std::to_string(config.hardwareRevision[1]), true);
}

void NukiNetworkLock::publishAdvancedConfig(const NukiLock::AdvancedConfig &config)
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
    publishString(mqtt_topic_config_advanced_json, _buffer, true);

    if(!_disableNonJSON)
    {
        publishBool(mqtt_topic_config_auto_unlock, config.autoUnLockDisabled == 0, true);
        publishBool(mqtt_topic_config_auto_lock, config.autoLockEnabled == 1, true);
    }
}

void NukiNetworkLock::publishRssi(const int& rssi)
{
    publishInt(mqtt_topic_lock_rssi, rssi, true);
}

void NukiNetworkLock::publishRetry(const std::string& message)
{
    publishString(mqtt_topic_lock_retry, message, true);
}

void NukiNetworkLock::publishBleAddress(const std::string &address)
{
    publishString(mqtt_topic_lock_address, address, true);
}

void NukiNetworkLock::publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount)
{
    bool publishCode = _preferences->getBool(preference_keypad_publish_code, false);
    bool topicPerEntry = _preferences->getBool(preference_keypad_topic_per_entry, false);
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

        if(publishCode) jsonEntry["code"] = entry.code;
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

        if(topicPerEntry)
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
                               { (char*)"pl_nukiOfficial->_off", (char*)disCommand.c_str() },
                               { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                               { (char*)"stat_on", (char*)"1" },
                               { (char*)"stat_nukiOfficial->_off", (char*)"0" }});
        }

        ++index;
    }

    serializeJson(json, _buffer, _bufferSize);
    publishString(mqtt_topic_keypad_json, _buffer, true);

    if(!_disableNonJSON)
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

        if(!publishCode)
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

void NukiNetworkLock::publishKeypadEntry(const String topic, NukiLock::KeypadEntry entry)
{
    if(_disableNonJSON) return;

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

void NukiNetworkLock::publishTimeControl(const std::list<NukiLock::TimeControlEntry>& timeControlEntries, uint maxTimeControlEntryCount)
{
    bool topicPerEntry = _preferences->getBool(preference_timecontrol_topic_per_entry, false);
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

        if(topicPerEntry)
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
                             (char*)"SmartLock",
                             "",
                             "",
                             "diagnostic",
                             String("~") + mqtt_topic_timecontrol_action,
                             { { (char*)"json_attr_t", (char*)basePathPrefixChr },
                               { (char*)"pl_on", (char*)enaCommand.c_str() },
                               { (char*)"pl_nukiOfficial->_off", (char*)disCommand.c_str() },
                               { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                               { (char*)"stat_on", (char*)"1" },
                               { (char*)"stat_nukiOfficial->_off", (char*)"0" }});
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

void NukiNetworkLock::publishAuth(const std::list<NukiLock::AuthorizationEntry>& authEntries, uint maxAuthEntryCount)
{
    uint index = 0;
    char str[50];
    char uidString[20];
    itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_lock_path);
    JsonDocument json;

    for(const auto& entry : authEntries)
    {
        auto jsonEntry = json.add<JsonVariant>();

        jsonEntry["authId"] = entry.authId;
        jsonEntry["idType"] = entry.idType; //CONSIDER INT TO STRING
        jsonEntry["enabled"] = entry.enabled;
        jsonEntry["name"] = entry.name;
        _authEntries[jsonEntry["authId"]] = jsonEntry["name"].as<String>();
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
                             (char*)"SmartLock",
                             "",
                             "",
                             "diagnostic",
                             String("~") + mqtt_topic_auth_action,
                             { { (char*)"json_attr_t", (char*)basePathPrefixChr },
                               { (char*)"pl_on", (char*)enaCommand.c_str() },
                               { (char*)"pl_nukiOfficial->_off", (char*)disCommand.c_str() },
                               { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                               { (char*)"stat_on", (char*)"1" },
                               { (char*)"stat_nukiOfficial->_off", (char*)"0" }});
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

void NukiNetworkLock::publishConfigCommandResult(const char* result)
{
    publishString(mqtt_topic_config_action_command_result, result, true);
}

void NukiNetworkLock::publishKeypadCommandResult(const char* result)
{
    if(_disableNonJSON) return;
    publishString(mqtt_topic_keypad_command_result, result, true);
}

void NukiNetworkLock::publishKeypadJsonCommandResult(const char* result)
{
    publishString(mqtt_topic_keypad_json_command_result, result, true);
}

void NukiNetworkLock::publishTimeControlCommandResult(const char* result)
{
    publishString(mqtt_topic_timecontrol_command_result, result, true);
}

void NukiNetworkLock::publishAuthCommandResult(const char* result)
{
    publishString(mqtt_topic_auth_command_result, result, true);
}

void NukiNetworkLock::publishStatusUpdated(const bool statusUpdated)
{
    publishBool(mqtt_topic_lock_status_updated, statusUpdated, true);
}

void NukiNetworkLock::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NukiNetworkLock::setOfficialUpdateReceivedCallback(void (*officialUpdateReceivedCallback)(const char *, const char *))
{
    _officialUpdateReceivedCallback = officialUpdateReceivedCallback;
}

void NukiNetworkLock::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void NukiNetworkLock::setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled))
{
    if(_disableNonJSON) return;
    _keypadCommandReceivedReceivedCallback = keypadCommandReceivedReceivedCallback;
}

void NukiNetworkLock::setKeypadJsonCommandReceivedCallback(void (*keypadJsonCommandReceivedReceivedCallback)(const char *))
{
    _keypadJsonCommandReceivedReceivedCallback = keypadJsonCommandReceivedReceivedCallback;
}

void NukiNetworkLock::setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char *))
{
    _timeControlCommandReceivedReceivedCallback = timeControlCommandReceivedReceivedCallback;
}

void NukiNetworkLock::setAuthCommandReceivedCallback(void (*authCommandReceivedReceivedCallback)(const char *))
{
    _authCommandReceivedReceivedCallback = authCommandReceivedReceivedCallback;
}

void NukiNetworkLock::buildMqttPath(const char* path, char* outPath)
{
    int offset = 0;
    char inPath[181] = {0};

    memcpy(inPath, _mqttPath, sizeof(_mqttPath));

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

bool NukiNetworkLock::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

void NukiNetworkLock::publishHASSConfig(char *deviceType, const char *baseTopic, char *name,  char *uidString, const char *softwareVersion, const char *hardwareVersion, const bool& hasDoorSensor, const bool& hasKeypad, const bool& publishAuthData, char *lockAction,
                               char *unlockAction, char *openAction)
{
    _network->publishHASSConfig(deviceType, baseTopic, name, uidString, softwareVersion, hardwareVersion, "~/maintenance/mqttConnectionState", hasKeypad, lockAction, unlockAction, openAction);
    _network->publishHASSConfigAdditionalLockEntities(deviceType, baseTopic, name, uidString);

    if(hasDoorSensor)
    {
        _network->publishHASSConfigDoorSensor(deviceType, baseTopic, name, uidString);
    }
    else
    {
        _network->removeHASSConfigTopic((char*)"binary_sensor", (char*)"door_sensor", uidString);
    }

    #ifndef CONFIG_IDF_TARGET_ESP32H2
    _network->publishHASSWifiRssiConfig(deviceType, baseTopic, name, uidString);
    #endif

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

void NukiNetworkLock::removeHASSConfig(char *uidString)
{
    _network->removeHASSConfig(uidString);
}

void NukiNetworkLock::publishOffAction(const int value)
{
    _network->publishInt(_nukiOfficial->getMqttPath(), mqtt_topic_official_lock_action, value, false);
}

void NukiNetworkLock::publishFloat(const char *topic, const float value, bool retain, const uint8_t precision)
{
    _nukiPublisher->publishFloat(topic, value, retain, precision);
}

void NukiNetworkLock::publishInt(const char *topic, const int value, bool retain)
{
    _nukiPublisher->publishInt(topic, value, retain);
}

void NukiNetworkLock::publishUInt(const char *topic, const unsigned int value, bool retain)
{
    _nukiPublisher->publishUInt(topic, value, retain);
}

void NukiNetworkLock::publishBool(const char *topic, const bool value, bool retain)
{
    _nukiPublisher->publishBool(topic, value, retain);
}

bool NukiNetworkLock::publishString(const char *topic, const String &value, bool retain)
{
    _nukiPublisher->publishString(topic, value, retain);
}

bool NukiNetworkLock::publishString(const char *topic, const std::string &value, bool retain)
{
    _nukiPublisher->publishString(topic, value, retain);
}

bool NukiNetworkLock::publishString(const char *topic, const char *value, bool retain)
{
    return _nukiPublisher->publishString(topic, value, retain);
}

void NukiNetworkLock::publishULong(const char *topic, const unsigned long value, bool retain)
{
    return _nukiPublisher->publishULong(topic, value, retain);
}

void NukiNetworkLock::publishLongLong(const char *topic, int64_t value, bool retain)
{
    return _nukiPublisher->publishLongLong(topic, value, retain);
}

String NukiNetworkLock::concat(String a, String b)
{
    String c = a;
    c.concat(b);
    return c;
}

bool NukiNetworkLock::reconnected()
{
    bool r = _reconnected;
    _reconnected = false;
    return r;
}

uint8_t NukiNetworkLock::queryCommands()
{
    uint8_t qc = _queryCommands;
    _queryCommands = 0;
    return qc;
}

void NukiNetworkLock::buttonPressActionToString(const NukiLock::ButtonPressAction btnPressAction, char* str) {
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

void NukiNetworkLock::homeKitStatusToString(const int hkstatus, char* str) {
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

void NukiNetworkLock::fobActionToString(const int fobact, char* str) {
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

const uint32_t NukiNetworkLock::getAuthId() const
{
    if(_nukiOfficial->hasAuthId())
    {
        return _nukiOfficial->getAuthId();
    }
    return _authId;
}
