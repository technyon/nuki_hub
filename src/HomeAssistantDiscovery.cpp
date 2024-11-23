#include "HomeAssistantDiscovery.h"
#include "Config.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"

HomeAssistantDiscovery::HomeAssistantDiscovery(NetworkDevice* device, Preferences *preferences, char* buffer, size_t bufferSize)
    : _device(device),
      _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize)
{
    _discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery, "");
    _baseTopic = _preferences->getString(preference_mqtt_lock_path);
    _offEnabled = _preferences->getBool(preference_official_hybrid_enabled, false);
    _checkUpdates = _preferences->getBool(preference_check_updates, false);
    _updateFromMQTT = _preferences->getBool(preference_update_from_mqtt, false);
    _hostname = _preferences->getString(preference_hostname, "");
    sprintf(_nukiHubUidString, "%u", _preferences->getUInt(preference_device_id_lock, 0));
}

void HomeAssistantDiscovery::setupHASS(int type, uint32_t nukiId, char* nukiName, const char* firmwareVersion, const char* hardwareVersion, bool hasDoorSensor, bool hasKeypad)
{
    char uidString[20];
    itoa(nukiId, uidString, 16);
    bool publishAuthData = _preferences->getBool(preference_publish_authdata, false);

    if(type == 0)
    {
        publishHASSNukiHubConfig();
        Log->println("HASS setup for NukiHub completed.");
    }
    else if(type == 1)
    {
        if(_preferences->getUInt(preference_nuki_id_lock, 0) != nukiId)
        {
            return;
        }
        String lockTopic = _baseTopic;
        lockTopic.concat("/lock");
        publishHASSConfig((char*)"SmartLock", lockTopic.c_str(), nukiName, uidString, firmwareVersion, hardwareVersion, hasDoorSensor, hasKeypad, publishAuthData, (char*)"lock", (char*)"unlock", (char*)"unlatch");
        Log->println("HASS setup for lock completed.");
    }
    else if(type == 2)
    {
        if(_preferences->getUInt(preference_nuki_id_opener, 0) != nukiId)
        {
            return;
        }
        String openerTopic = _baseTopic;
        openerTopic.concat("/opener");
        if(_preferences->getBool(preference_opener_continuous_mode, false))
        {
            publishHASSConfig((char*)"Opener", openerTopic.c_str(), nukiName, uidString, firmwareVersion, hardwareVersion, hasDoorSensor, hasKeypad, publishAuthData, (char*)"deactivateCM", (char*)"activateCM", (char*)"electricStrikeActuation");
        }
        else
        {
            publishHASSConfig((char*)"Opener", openerTopic.c_str(), nukiName, uidString, firmwareVersion, hardwareVersion, hasDoorSensor, hasKeypad, publishAuthData, (char*)"deactivateRTO", (char*)"activateRTO", (char*)"electricStrikeActuation");
        }

        Log->println("HASS setup for opener completed.");
    }
}

void HomeAssistantDiscovery::disableHASS()
{
    removeHASSConfig(_nukiHubUidString);

    char uidString[20];

    if(_preferences->getUInt(preference_nuki_id_lock, 0) != 0)
    {
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        removeHASSConfig(uidString);
    }
    if(_preferences->getUInt(preference_nuki_id_opener, 0) != 0)
    {
        itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
        removeHASSConfig(uidString);
    }
}

void HomeAssistantDiscovery::publishHASSNukiHubConfig()
{
    JsonDocument json;
    json.clear();
    JsonObject dev = json["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(String("nuki_") + _nukiHubUidString);
    json["dev"]["mf"] = "Technyon";
    json["dev"]["mdl"] = "NukiHub";
    json["dev"]["name"] = _hostname.c_str();
    json["dev"]["sw"] = NUKI_HUB_VERSION;
    json["dev"]["hw"] = NUKI_HUB_HW;

    String cuUrl = _preferences->getString(preference_mqtt_hass_cu_url, "");

    if (cuUrl != "")
    {
        json["dev"]["cu"] = cuUrl;
    }
    else
    {
        json["dev"]["cu"] = "http://" + _device->localIP();
    }

    json["~"] = _baseTopic;
    json["name"] = "Restart Nuki Hub";
    json["unique_id"] = String(_nukiHubUidString) + "_reset";
    json["avty"][0]["t"] = String("~") + mqtt_topic_mqtt_connection_state;
    json["opt"] = "false";
    json["stat_t"] = String("~") + mqtt_topic_reset;
    json["ent_cat"] = "diagnostic";
    json["cmd_t"] = String("~") + mqtt_topic_reset;
    json["ic"] = "mdi:restart";
    json["pl_on"] = "1";
    json["pl_off"] = "0";
    json["stat_on"] = "1";
    json["stat_off"] = "0";

    serializeJson(json, _buffer, _bufferSize);

    String path = _preferences->getString(preference_mqtt_hass_discovery, "homeassistant");
    path.concat("/switch/");
    path.concat(_nukiHubUidString);
    path.concat("/reset/config");

    _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);

#ifndef CONFIG_IDF_TARGET_ESP32H2
    publishHassTopic("sensor",
                     "wifi_signal_strength",
                     _nukiHubUidString,
                     "_wifi_signal_strength",
                     "WIFI signal strength",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_wifi_rssi,
                     "NukiHub",
                     "signal_strength",
                     "measurement",
                     "diagnostic",
                     "",
    { {(char*)"unit_of_meas", (char*)"dBm"} });
#endif

    // MQTT Connected
    publishHassTopic("binary_sensor",
                     "mqtt_connected",
                     _nukiHubUidString,
                     "_mqtt_connected",
                     "MQTT connected",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_mqtt_connection_state,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        {(char*)"pl_on", (char*)"online"},
        {(char*)"pl_off", (char*)"offline"},
        {(char*)"ic", (char*)"mdi:lan-connect"}
    });

    // Network device
    publishHassTopic("sensor",
                     "network_device",
                     _nukiHubUidString,
                     "_network_device",
                     "Network device",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_network_device,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    { { (char*)"en", (char*)"true" }});

    // Nuki Hub Webserver enabled
    publishHassTopic("switch",
                     "webserver",
                     _nukiHubUidString,
                     "_webserver",
                     "Nuki Hub webserver enabled",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_webserver_state,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     String("~") + mqtt_topic_webserver_action,
    {
        { (char*)"pl_on", (char*)"1" },
        { (char*)"pl_off", (char*)"0" },
        { (char*)"stat_on", (char*)"1" },
        { (char*)"stat_off", (char*)"0" }
    });

    // Uptime
    publishHassTopic("sensor",
                     "uptime",
                     _nukiHubUidString,
                     "_uptime",
                     "Uptime",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_uptime,
                     "NukiHub",
                     "duration",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"en", (char*)"true" },
        { (char*)"unit_of_meas", (char*)"min"}
    });

    if(_preferences->getBool(preference_mqtt_log_enabled, false))
    {
        // MQTT Log
        publishHassTopic("sensor",
                         "mqtt_log",
                         _nukiHubUidString,
                         "_mqtt_log",
                         "MQTT Log",
                         _hostname.c_str(),
                         _baseTopic.c_str(),
                         String("~") + mqtt_topic_log,
                         "NukiHub",
                         "",
                         "",
                         "diagnostic",
                         "",
        { { (char*)"en", (char*)"true" }});
    }
    else
    {
        removeHassTopic((char*)"sensor", (char*)"mqtt_log", _nukiHubUidString);
    }

    // Nuki Hub version
    publishHassTopic("sensor",
                     "nuki_hub_version",
                     _nukiHubUidString,
                     "_nuki_hub_version",
                     "Nuki Hub version",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_info_nuki_hub_version,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"en", (char*)"true" },
        {(char*)"ic", (char*)"mdi:counter"}
    });

    // Nuki Hub build
    publishHassTopic("sensor",
                     "nuki_hub_build",
                     _nukiHubUidString,
                     "_nuki_hub_build",
                     "Nuki Hub build",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_info_nuki_hub_build,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"en", (char*)"true" },
        {(char*)"ic", (char*)"mdi:counter"}
    });

    // Nuki Hub restart reason
    publishHassTopic("sensor",
                     "nuki_hub_restart_reason",
                     _nukiHubUidString,
                     "_nuki_hub_restart_reason",
                     "Nuki Hub restart reason",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_restart_reason_fw,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    { { (char*)"en", (char*)"true" }});

    // Nuki Hub restart reason ESP
    publishHassTopic("sensor",
                     "nuki_hub_restart_reason_esp",
                     _nukiHubUidString,
                     "_nuki_hub_restart_reason_esp",
                     "Nuki Hub restart reason ESP",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_restart_reason_esp,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    { { (char*)"en", (char*)"true" }});

    if(_checkUpdates)
    {
        // NUKI Hub latest
        publishHassTopic("sensor",
                         "nuki_hub_latest",
                         _nukiHubUidString,
                         "_nuki_hub_latest",
                         "NUKI Hub latest",
                         _hostname.c_str(),
                         _baseTopic.c_str(),
                         String("~") + mqtt_topic_info_nuki_hub_latest,
                         "NukiHub",
                         "",
                         "",
                         "diagnostic",
                         "",
        {
            { (char*)"en", (char*)"true" },
            {(char*)"ic", (char*)"mdi:counter"}
        });

        // NUKI Hub update
        char latest_version_topic[250];
        _baseTopic.toCharArray(latest_version_topic,_baseTopic.length() + 1);
        strcat(latest_version_topic, mqtt_topic_info_nuki_hub_latest);

        if(!_updateFromMQTT)
        {
            publishHassTopic("update",
                             "nuki_hub_update",
                             _nukiHubUidString,
                             "_nuki_hub_update",
                             "NUKI Hub firmware update",
                             _hostname.c_str(),
                             _baseTopic.c_str(),
                             String("~") + mqtt_topic_info_nuki_hub_version,
                             "NukiHub",
                             "firmware",
                             "",
                             "diagnostic",
                             "",
            {
                { (char*)"en", (char*)"true" },
                { (char*)"ent_pic", (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/master/icon/favicon-32x32.png" },
                { (char*)"rel_u", (char*)GITHUB_LATEST_RELEASE_URL },
                { (char*)"l_ver_t", (char*)latest_version_topic }
            });
        }
        else
        {
            publishHassTopic("update",
                             "nuki_hub_update",
                             _nukiHubUidString,
                             "_nuki_hub_update",
                             "NUKI Hub firmware update",
                             _hostname.c_str(),
                             _baseTopic.c_str(),
                             String("~") + mqtt_topic_info_nuki_hub_version,
                             "NukiHub",
                             "firmware",
                             "",
                             "diagnostic",
                             String("~") + mqtt_topic_update,
            {
                { (char*)"en", (char*)"true" },
                { (char*)"pl_inst", (char*)"1" },
                { (char*)"ent_pic", (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/master/icon/favicon-32x32.png" },
                { (char*)"rel_u", (char*)GITHUB_LATEST_RELEASE_URL },
                { (char*)"l_ver_t", (char*)latest_version_topic }
            });
        }
    }
    else
    {
        removeHassTopic((char*)"sensor", (char*)"nuki_hub_latest", _nukiHubUidString);
        removeHassTopic((char*)"update", (char*)"nuki_hub_update", _nukiHubUidString);
    }

    // Nuki Hub IP Address
    publishHassTopic("sensor",
                     "nuki_hub_ip",
                     _nukiHubUidString,
                     "_nuki_hub_ip",
                     "Nuki Hub IP",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_info_nuki_hub_ip,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"en", (char*)"true" },
        {(char*)"ic", (char*)"mdi:ip"}
    });
}

void HomeAssistantDiscovery::publishHASSConfig(char *deviceType, const char *baseTopic, char *name, char *uidString, const char *softwareVersion, const char *hardwareVersion, const bool& hasDoorSensor, const bool& hasKeypad, const bool& publishAuthData, char *lockAction, char *unlockAction, char *openAction)
{
    String availabilityTopic = _baseTopic;
    availabilityTopic.concat(mqtt_topic_mqtt_connection_state);

    publishHASSDeviceConfig(deviceType, baseTopic, name, uidString, softwareVersion, hardwareVersion, availabilityTopic.c_str(), hasKeypad, lockAction, unlockAction, openAction);

    if(strcmp(deviceType, "SmartLock") == 0)
    {
        publishHASSConfigAdditionalLockEntities(deviceType, baseTopic, name, uidString);
    }
    else
    {
        publishHASSConfigAdditionalOpenerEntities(deviceType, baseTopic, name, uidString);
    }
    if(hasDoorSensor)
    {
        publishHASSConfigDoorSensor(deviceType, baseTopic, name, uidString);
    }
    else
    {
        removeHASSConfigTopic((char*)"binary_sensor", (char*)"door_sensor", uidString);
    }
    if(publishAuthData)
    {
        publishHASSConfigAccessLog(deviceType, baseTopic, name, uidString);
    }
    else
    {
        removeHASSConfigTopic((char*)"sensor", (char*)"last_action_authorization", uidString);
        removeHASSConfigTopic((char*)"sensor", (char*)"rolling_log", uidString);
    }
    if(hasKeypad)
    {
        publishHASSConfigKeypad(deviceType, baseTopic, name, uidString);
    }
    else
    {
        removeHASSConfigTopic((char*)"sensor", (char*)"keypad_status", uidString);
        removeHASSConfigTopic((char*)"binary_sensor", (char*)"keypad_battery_low", uidString);
    }
}

void HomeAssistantDiscovery::publishHASSDeviceConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const char *softwareVersion, const char *hardwareVersion, const char* availabilityTopic, const bool& hasKeypad, char* lockAction, char* unlockAction, char* openAction)
{
    JsonDocument json;
    json.clear();
    JsonObject dev = json["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(String("nuki_") + uidString);
    json["dev"]["mf"] = "Nuki";
    json["dev"]["mdl"] = deviceType;
    json["dev"]["name"] = name;
    json["dev"]["sw"] = softwareVersion;
    json["dev"]["hw"] = hardwareVersion;
    json["dev"]["via_device"] = String("nuki_") + _nukiHubUidString;

    String cuUrl = _preferences->getString(preference_mqtt_hass_cu_url, "");

    if (cuUrl != "")
    {
        json["dev"]["cu"] = cuUrl;
    }
    else
    {
        json["dev"]["cu"] = "http://" + _device->localIP();
    }

    json["~"] = baseTopic;
    json["name"] = nullptr;
    json["unique_id"] = String(uidString) + "_lock";
    json["cmd_t"] = String("~") + String(mqtt_topic_lock_action);
    json["avty"][0]["t"] = availabilityTopic;
    json["pl_lock"] = lockAction;
    json["pl_unlk"] = unlockAction;

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    if((strcmp(deviceType, "SmartLock") == 0 && (int)aclPrefs[2]) || (strcmp(deviceType, "SmartLock") != 0 && (int)aclPrefs[11]))
    {
        json["pl_open"] = openAction;
    }

    json["stat_t"] = String("~") + mqtt_topic_lock_ha_state;
    json["stat_jam"] = "jammed";
    json["stat_locked"] = "locked";
    json["stat_locking"] = "locking";
    json["stat_unlocked"] = "unlocked";
    json["stat_unlocking"] = "unlocking";
    json["stat_open"] = "open";
    json["stat_opening"] = "opening";
    json["opt"] = "false";

    serializeJson(json, _buffer, _bufferSize);

    String path = _preferences->getString(preference_mqtt_hass_discovery, "homeassistant");
    path.concat("/lock/");
    path.concat(uidString);
    path.concat("/smartlock/config");

    _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);


    // Firmware version
    publishHassTopic("sensor",
                     "firmware_version",
                     uidString,
                     "_firmware_version",
                     "Firmware version",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_info_firmware_version,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"en", (char*)"true" },
        {(char*)"ic", (char*)"mdi:counter"}
    });

    // Hardware version
    publishHassTopic("sensor",
                     "hardware_version",
                     uidString,
                     "_hardware_version",
                     "Hardware version",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_info_hardware_version,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"en", (char*)"true" },
        {(char*)"ic", (char*)"mdi:counter"}
    });

    // Battery critical
    publishHassTopic("binary_sensor",
                     "battery_low",
                     uidString,
                     "_battery_low",
                     "Battery low",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_battery_basic_json,
                     deviceType,
                     "battery",
                     "",
                     "diagnostic",
                     "",
    {
        {(char*)"pl_on", (char*)"1"},
        {(char*)"pl_off", (char*)"0"},
        {(char*)"val_tpl", (char*)"{{value_json.critical}}" }
    });

    // Battery voltage
    publishHassTopic("sensor",
                     "battery_voltage",
                     uidString,
                     "_battery_voltage",
                     "Battery voltage",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_battery_advanced_json,
                     deviceType,
                     "voltage",
                     "measurement",
                     "diagnostic",
                     "",
    {
        {(char*)"unit_of_meas", (char*)"V"},
        {(char*)"val_tpl", (char*)"{{value_json.batteryVoltage}}" }
    });

    // Trigger
    publishHassTopic("sensor",
                     "trigger",
                     uidString,
                     "_trigger",
                     "Trigger",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_trigger,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
    { { (char*)"en", (char*)"true" } });

    if(_offEnabled)
    {
        // Hybrid connected
        String hybridPath = _baseTopic;
        hybridPath.concat("/lock");
        hybridPath.concat(mqtt_topic_hybrid_state);
        publishHassTopic("binary_sensor",
                         "hybrid_connected",
                         uidString,
                         "_hybrid_connected",
                         "Hybrid connected",
                         name,
                         baseTopic,
                         hybridPath,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
        {
            {(char*)"pl_on", (char*)"1"},
            {(char*)"pl_off", (char*)"0"},
            { (char*)"en", (char*)"true" }
        });
    }
    else
    {
        removeHassTopic((char*)"binary_sensor", (char*)"hybrid_connected", uidString);
    }

    // Query Lock State
    publishHassTopic("button",
                     "query_lockstate",
                     uidString,
                     "_query_lockstate",
                     "Query lock state",
                     name,
                     baseTopic,
                     "",
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     String("~") + mqtt_topic_query_lockstate,
    {
        { (char*)"en", (char*)"false" },
        { (char*)"pl_prs", (char*)"1" }
    });

    // Query Config
    publishHassTopic("button",
                     "query_config",
                     uidString,
                     "_query_config",
                     "Query config",
                     name,
                     baseTopic,
                     "",
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     String("~") + mqtt_topic_query_config,
    {
        { (char*)"en", (char*)"false" },
        { (char*)"pl_prs", (char*)"1" }
    });

    // Query Lock State Command result
    publishHassTopic("button",
                     "query_commandresult",
                     uidString,
                     "_query_commandresult",
                     "Query lock state command result",
                     name,
                     baseTopic,
                     "",
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     String("~") + mqtt_topic_query_lockstate_command_result,
    {
        { (char*)"en", (char*)"false" },
        { (char*)"pl_prs", (char*)"1" }
    });

    publishHassTopic("sensor",
                     "bluetooth_signal_strength",
                     uidString,
                     "_bluetooth_signal_strength",
                     "Bluetooth signal strength",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_rssi,
                     deviceType,
                     "signal_strength",
                     "measurement",
                     "diagnostic",
                     "",
    { {(char*)"unit_of_meas", (char*)"dBm"} });
}

void HomeAssistantDiscovery::publishHASSConfigAdditionalLockEntities(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedLockConfigAclPrefs[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if(_preferences->getBool(preference_conf_info_enabled, true))
    {
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
    }

    if((int)aclPrefs[2])
    {
        // Unlatch
        publishHassTopic("button",
                         "unlatch",
                         uidString,
                         "_unlatch",
                         "Open",
                         name,
                         baseTopic,
                         "",
                         deviceType,
                         "",
                         "",
                         "",
                         String("~") + mqtt_topic_lock_action,
        {
            { (char*)"en", (char*)"false" },
            { (char*)"pl_prs", (char*)"unlatch" }
        });
    }
    else
    {
        removeHassTopic((char*)"button", (char*)"unlatch", uidString);
    }

    if((int)aclPrefs[3])
    {
        // Lock 'n' Go
        publishHassTopic("button",
                         "lockngo",
                         uidString,
                         "_lockngo",
                         "Lock 'n' Go",
                         name,
                         baseTopic,
                         "",
                         deviceType,
                         "",
                         "",
                         "",
                         String("~") + mqtt_topic_lock_action,
        {
            { (char*)"en", (char*)"false" },
            { (char*)"pl_prs", (char*)"lockNgo" }
        });
    }
    else
    {
        removeHassTopic((char*)"button", (char*)"lockngo", uidString);
    }

    if((int)aclPrefs[4])
    {
        // Lock 'n' Go with unlatch
        publishHassTopic("button",
                         "lockngounlatch",
                         uidString,
                         "_lockngounlatch",
                         "Lock 'n' Go with unlatch",
                         name,
                         baseTopic,
                         "",
                         deviceType,
                         "",
                         "",
                         "",
                         String("~") + mqtt_topic_lock_action,
        {
            { (char*)"en", (char*)"false" },
            { (char*)"pl_prs", (char*)"lockNgoUnlatch" }
        });
    }
    else
    {
        removeHassTopic((char*)"button", (char*)"lockngounlatch", uidString);
    }

    // Query Battery
    publishHassTopic("button",
                     "query_battery",
                     uidString,
                     "_query_battery",
                     "Query battery",
                     name,
                     baseTopic,
                     "",
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     String("~") + mqtt_topic_query_battery,
    {
        { (char*)"en", (char*)"false" },
        { (char*)"pl_prs", (char*)"1" }
    });

    if((int)basicLockConfigAclPrefs[6] == 1)
    {
        // LED enabled
        publishHassTopic("switch",
                         "led_enabled",
                         uidString,
                         "_led_enabled",
                         "LED enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:led-variant-on" },
            { (char*)"pl_on", (char*)"{ \"ledEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"ledEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.ledEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"led_enabled", uidString);
    }

    if((int)basicLockConfigAclPrefs[5] == 1)
    {
        // Button enabled
        publishHassTopic("switch",
                         "button_enabled",
                         uidString,
                         "_button_enabled",
                         "Button enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:radiobox-marked" },
            { (char*)"pl_on", (char*)"{ \"buttonEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"buttonEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.buttonEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"button_enabled", uidString);
    }

    if((int)advancedLockConfigAclPrefs[19] == 1)
    {
        // Auto Lock
        publishHassTopic("switch",
                         "auto_lock",
                         uidString,
                         "_auto_lock",
                         "Auto lock",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"autoLockEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"autoLockEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.autoLockEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"auto_lock", uidString);
    }

    if((int)advancedLockConfigAclPrefs[12] == 1)
    {
        // Auto Unlock
        publishHassTopic("switch",
                         "auto_unlock",
                         uidString,
                         "_auto_unlock",
                         "Auto unlock",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"autoUnLockDisabled\": \"0\"}" },
            { (char*)"pl_off", (char*)"{ \"autoUnLockDisabled\": \"1\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.autoUnLockDisabled}}" },
            { (char*)"stat_on", (char*)"0" },
            { (char*)"stat_off", (char*)"1" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"auto_unlock", uidString);
    }

    if((int)basicLockConfigAclPrefs[13] == 1)
    {
        // Double lock
        publishHassTopic("switch",
                         "double_lock",
                         uidString,
                         "_double_lock",
                         "Double lock",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"singleLock\": \"0\"}" },
            { (char*)"pl_off", (char*)"{ \"singleLock\": \"1\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.singleLock}}" },
            { (char*)"stat_on", (char*)"0" },
            { (char*)"stat_off", (char*)"1" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"double_lock", uidString);
    }

    publishHassTopic("sensor",
                     "battery_level",
                     uidString,
                     "_battery_level",
                     "Battery level",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_battery_basic_json,
                     deviceType,
                     "battery",
                     "measurement",
                     "diagnostic",
                     "",
    {
        {(char*)"unit_of_meas", (char*)"%"},
        {(char*)"val_tpl", (char*)"{{value_json.level}}" }
    });

    if((int)basicLockConfigAclPrefs[7] == 1)
    {
        publishHassTopic("number",
                         "led_brightness",
                         uidString,
                         "_led_brightness",
                         "LED brightness",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:brightness-6" },
            { (char*)"cmd_tpl", (char*)"{ \"ledBrightness\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.ledBrightness}}" },
            { (char*)"min", (char*)"0" },
            { (char*)"max", (char*)"5" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"led_brightness", uidString);
    }

    if((int)basicLockConfigAclPrefs[3] == 1)
    {
        // Auto Unlatch
        publishHassTopic("switch",
                         "auto_unlatch",
                         uidString,
                         "_auto_unlatch",
                         "Auto unlatch",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"autoUnlatch\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"autoUnlatch\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.autoUnlatch}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"auto_unlatch", uidString);
    }

    if((int)basicLockConfigAclPrefs[4] == 1)
    {
        // Pairing enabled
        publishHassTopic("switch",
                         "pairing_enabled",
                         uidString,
                         "_pairing_enabled",
                         "Pairing enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"pairingEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"pairingEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.pairingEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"pairing_enabled", uidString);
    }

    if((int)basicLockConfigAclPrefs[8] == 1)
    {
        publishHassTopic("number",
                         "timezone_offset",
                         uidString,
                         "_timezone_offset",
                         "Timezone offset",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:timer-cog-outline" },
            { (char*)"cmd_tpl", (char*)"{ \"timeZoneOffset\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.timeZoneOffset}}" },
            { (char*)"min", (char*)"0" },
            { (char*)"max", (char*)"60" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"timezone_offset", uidString);
    }

    if((int)basicLockConfigAclPrefs[9] == 1)
    {
        // DST Mode
        publishHassTopic("switch",
                         "dst_mode",
                         uidString,
                         "_dst_mode",
                         "DST mode European",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"dstMode\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"dstMode\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.dstMode}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"dst_mode", uidString);
    }

    if((int)basicLockConfigAclPrefs[10] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_fob_action_1", "Fob action 1", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.fobAction1}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"fobAction1\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Unlock";
        json["options"][2] = "Lock";
        json["options"][3] = "Lock n Go";
        json["options"][4] = "Intelligent";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "fob_action_1", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"fob_action_1", uidString);
    }

    if((int)basicLockConfigAclPrefs[11] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_fob_action_2", "Fob action 2", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.fobAction2}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"fobAction2\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Unlock";
        json["options"][2] = "Lock";
        json["options"][3] = "Lock n Go";
        json["options"][4] = "Intelligent";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "fob_action_2", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"fob_action_2", uidString);
    }

    if((int)basicLockConfigAclPrefs[12] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_fob_action_3", "Fob action 3", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.fobAction3}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"fobAction3\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Unlock";
        json["options"][2] = "Lock";
        json["options"][3] = "Lock n Go";
        json["options"][4] = "Intelligent";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "fob_action_3", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"fob_action_3", uidString);
    }

    if((int)basicLockConfigAclPrefs[14] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_advertising_mode", "Advertising mode", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.advertisingMode}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"advertisingMode\": \"{{ value }}\" }" }});
        json["options"][0] = "Automatic";
        json["options"][1] = "Normal";
        json["options"][2] = "Slow";
        json["options"][3] = "Slowest";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "advertising_mode", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"advertising_mode", uidString);
    }

    if((int)basicLockConfigAclPrefs[15] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_timezone", "Timezone", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.timeZone}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"timeZone\": \"{{ value }}\" }" }});
        json["options"][0] = "Africa/Cairo";
        json["options"][1] = "Africa/Lagos";
        json["options"][2] = "Africa/Maputo";
        json["options"][3] = "Africa/Nairobi";
        json["options"][4] = "America/Anchorage";
        json["options"][5] = "America/Argentina/Buenos_Aires";
        json["options"][6] = "America/Chicago";
        json["options"][7] = "America/Denver";
        json["options"][8] = "America/Halifax";
        json["options"][9] = "America/Los_Angeles";
        json["options"][10] = "America/Manaus";
        json["options"][11] = "America/Mexico_City";
        json["options"][12] = "America/New_York";
        json["options"][13] = "America/Phoenix";
        json["options"][14] = "America/Regina";
        json["options"][15] = "America/Santiago";
        json["options"][16] = "America/Sao_Paulo";
        json["options"][17] = "America/St_Johns";
        json["options"][18] = "Asia/Bangkok";
        json["options"][19] = "Asia/Dubai";
        json["options"][20] = "Asia/Hong_Kong";
        json["options"][21] = "Asia/Jerusalem";
        json["options"][22] = "Asia/Karachi";
        json["options"][23] = "Asia/Kathmandu";
        json["options"][24] = "Asia/Kolkata";
        json["options"][25] = "Asia/Riyadh";
        json["options"][26] = "Asia/Seoul";
        json["options"][27] = "Asia/Shanghai";
        json["options"][28] = "Asia/Tehran";
        json["options"][29] = "Asia/Tokyo";
        json["options"][30] = "Asia/Yangon";
        json["options"][31] = "Australia/Adelaide";
        json["options"][32] = "Australia/Brisbane";
        json["options"][33] = "Australia/Darwin";
        json["options"][34] = "Australia/Hobart";
        json["options"][35] = "Australia/Perth";
        json["options"][36] = "Australia/Sydney";
        json["options"][37] = "Europe/Berlin";
        json["options"][38] = "Europe/Helsinki";
        json["options"][39] = "Europe/Istanbul";
        json["options"][40] = "Europe/London";
        json["options"][41] = "Europe/Moscow";
        json["options"][42] = "Pacific/Auckland";
        json["options"][43] = "Pacific/Guam";
        json["options"][44] = "Pacific/Honolulu";
        json["options"][45] = "Pacific/Pago_Pago";
        json["options"][46] = "None";

        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "timezone", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"timezone", uidString);
    }

    if((int)advancedLockConfigAclPrefs[0] == 1)
    {
        publishHassTopic("number",
                         "unlocked_position_offset_degrees",
                         uidString,
                         "_unlocked_position_offset_degrees",
                         "Unlocked position offset degrees",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"unlockedPositionOffsetDegrees\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.unlockedPositionOffsetDegrees}}" },
            { (char*)"min", (char*)"-90" },
            { (char*)"max", (char*)"180" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"unlocked_position_offset_degrees", uidString);
    }

    if((int)advancedLockConfigAclPrefs[1] == 1)
    {
        publishHassTopic("number",
                         "locked_position_offset_degrees",
                         uidString,
                         "_locked_position_offset_degrees",
                         "Locked position offset degrees",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"lockedPositionOffsetDegrees\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.lockedPositionOffsetDegrees}}" },
            { (char*)"min", (char*)"-180" },
            { (char*)"max", (char*)"90" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"locked_position_offset_degrees", uidString);
    }

    if((int)advancedLockConfigAclPrefs[2] == 1)
    {
        publishHassTopic("number",
                         "single_locked_position_offset_degrees",
                         uidString,
                         "_single_locked_position_offset_degrees",
                         "Single locked position offset degrees",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"singleLockedPositionOffsetDegrees\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.singleLockedPositionOffsetDegrees}}" },
            { (char*)"min", (char*)"-180" },
            { (char*)"max", (char*)"180" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"single_locked_position_offset_degrees", uidString);
    }

    if((int)advancedLockConfigAclPrefs[3] == 1)
    {
        publishHassTopic("number",
                         "unlocked_locked_transition_offset_degrees",
                         uidString,
                         "_unlocked_locked_transition_offset_degrees",
                         "Unlocked to locked transition offset degrees",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"unlockedToLockedTransitionOffsetDegrees\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.unlockedToLockedTransitionOffsetDegrees}}" },
            { (char*)"min", (char*)"-180" },
            { (char*)"max", (char*)"180" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"unlocked_locked_transition_offset_degrees", uidString);
    }

    if((int)advancedLockConfigAclPrefs[4] == 1)
    {
        publishHassTopic("number",
                         "lockngo_timeout",
                         uidString,
                         "_lockngo_timeout",
                         "Lock n Go timeout",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"lockNgoTimeout\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.lockNgoTimeout}}" },
            { (char*)"min", (char*)"5" },
            { (char*)"max", (char*)"60" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"lockngo_timeout", uidString);
    }

    if((int)advancedLockConfigAclPrefs[5] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_single_button_press_action", "Single button press action", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.singleButtonPressAction}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"singleButtonPressAction\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Intelligent";
        json["options"][2] = "Unlock";
        json["options"][3] = "Lock";
        json["options"][4] = "Unlatch";
        json["options"][5] = "Lock n Go";
        json["options"][6] = "Show Status";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "single_button_press_action", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"single_button_press_action", uidString);
    }

    if((int)advancedLockConfigAclPrefs[6] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_double_button_press_action", "Double button press action", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.doubleButtonPressAction}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"doubleButtonPressAction\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Intelligent";
        json["options"][2] = "Unlock";
        json["options"][3] = "Lock";
        json["options"][4] = "Unlatch";
        json["options"][5] = "Lock n Go";
        json["options"][6] = "Show Status";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "double_button_press_action", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"double_button_press_action", uidString);
    }

    if((int)advancedLockConfigAclPrefs[7] == 1)
    {
        // Detached cylinder
        publishHassTopic("switch",
                         "detached_cylinder",
                         uidString,
                         "_detached_cylinder",
                         "Detached cylinder",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"detachedCylinder\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"detachedCylinder\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.detachedCylinder}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"detached_cylinder", uidString);
    }

    if((int)advancedLockConfigAclPrefs[8] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_battery_type", "Battery type", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.batteryType}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"batteryType\": \"{{ value }}\" }" }});
        json["options"][0] = "Alkali";
        json["options"][1] = "Accumulators";
        json["options"][2] = "Lithium";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "battery_type", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"battery_type", uidString);
    }

    if((int)advancedLockConfigAclPrefs[9] == 1)
    {
        // Automatic battery type detection
        publishHassTopic("switch",
                         "automatic_battery_type_detection",
                         uidString,
                         "_automatic_battery_type_detection",
                         "Automatic battery type detection",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"automaticBatteryTypeDetection\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"automaticBatteryTypeDetection\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.automaticBatteryTypeDetection}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"automatic_battery_type_detection", uidString);
    }

    if((int)advancedLockConfigAclPrefs[10] == 1)
    {
        publishHassTopic("number",
                         "unlatch_duration",
                         uidString,
                         "_unlatch_duration",
                         "Unlatch duration",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"unlatchDuration\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.unlatchDuration}}" },
            { (char*)"min", (char*)"1" },
            { (char*)"max", (char*)"30" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"unlatch_duration", uidString);
    }

    if((int)advancedLockConfigAclPrefs[11] == 1)
    {
        publishHassTopic("number",
                         "auto_lock_timeout",
                         uidString,
                         "_auto_lock_timeout",
                         "Auto lock timeout",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"autoLockTimeOut\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.autoLockTimeOut}}" },
            { (char*)"min", (char*)"30" },
            { (char*)"max", (char*)"1800" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"auto_lock_timeout", uidString);
    }

    if((int)advancedLockConfigAclPrefs[13] == 1)
    {
        // Nightmode enabled
        publishHassTopic("switch",
                         "nightmode_enabled",
                         uidString,
                         "_nightmode_enabled",
                         "Nightmode enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"nightModeEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"nightModeEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.nightModeEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"nightmode_enabled", uidString);
    }

    if((int)advancedLockConfigAclPrefs[14] == 1)
    {
        // Nightmode start time
        publishHassTopic("text",
                         "nightmode_start_time",
                         uidString,
                         "_nightmode_start_time",
                         "Nightmode start time",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pattern", (char*)"([0-1][0-9]|2[0-3]):[0-5][0-9]" },
            { (char*)"cmd_tpl", (char*)"{ \"nightModeStartTime\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.nightModeStartTime}}" },
            { (char*)"min", (char*)"5" },
            { (char*)"max", (char*)"5" }
        });
    }
    else
    {
        removeHassTopic((char*)"text", (char*)"nightmode_start_time", uidString);
    }

    if((int)advancedLockConfigAclPrefs[15] == 1)
    {
        // Nightmode end time
        publishHassTopic("text",
                         "nightmode_end_time",
                         uidString,
                         "_nightmode_end_time",
                         "Nightmode end time",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pattern", (char*)"([0-1][0-9]|2[0-3]):[0-5][0-9]" },
            { (char*)"cmd_tpl", (char*)"{ \"nightModeEndTime\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.nightModeEndTime}}" },
            { (char*)"min", (char*)"5" },
            { (char*)"max", (char*)"5" }
        });
    }
    else
    {
        removeHassTopic((char*)"text", (char*)"nightmode_end_time", uidString);
    }

    if((int)advancedLockConfigAclPrefs[16] == 1)
    {
        // Nightmode Auto Lock
        publishHassTopic("switch",
                         "nightmode_auto_lock",
                         uidString,
                         "_nightmode_auto_lock",
                         "Nightmode auto lock",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"nightModeAutoLockEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"nightModeAutoLockEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.nightModeAutoLockEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"nightmode_auto_lock", uidString);
    }

    if((int)advancedLockConfigAclPrefs[17] == 1)
    {
        // Nightmode Auto Unlock
        publishHassTopic("switch",
                         "nightmode_auto_unlock",
                         uidString,
                         "_nightmode_auto_unlock",
                         "Nightmode auto unlock",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"nightModeAutoUnlockDisabled\": \"0\"}" },
            { (char*)"pl_off", (char*)"{ \"nightModeAutoUnlockDisabled\": \"1\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.nightModeAutoUnlockDisabled}}" },
            { (char*)"stat_on", (char*)"0" },
            { (char*)"stat_off", (char*)"1" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"nightmode_auto_unlock", uidString);
    }

    if((int)advancedLockConfigAclPrefs[18] == 1)
    {
        // Nightmode immediate lock on start
        publishHassTopic("switch",
                         "nightmode_immediate_lock_start",
                         uidString,
                         "_nightmode_immediate_lock_start",
                         "Nightmode immediate lock on start",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"nightModeImmediateLockOnStart\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"nightModeImmediateLockOnStart\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.nightModeImmediateLockOnStart}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"nightmode_immediate_lock_start", uidString);
    }

    if((int)advancedLockConfigAclPrefs[20] == 1)
    {
        // Immediate auto lock enabled
        publishHassTopic("switch",
                         "immediate_auto_lock_enabled",
                         uidString,
                         "_immediate_auto_lock_enabled",
                         "Immediate auto lock enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"immediateAutoLockEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"immediateAutoLockEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.immediateAutoLockEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"immediate_auto_lock_enabled", uidString);
    }

    if((int)advancedLockConfigAclPrefs[21] == 1)
    {
        // Auto update enabled
        publishHassTopic("switch",
                         "auto_update_enabled",
                         uidString,
                         "_auto_update_enabled",
                         "Auto update enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"autoUpdateEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"autoUpdateEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.autoUpdateEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"auto_update_enabled", uidString);
    }

    if((int)advancedLockConfigAclPrefs[22] == 1)
    {
        // Reboot Nuki
        publishHassTopic("button",
                         "reboot_nuki",
                         uidString,
                         "_reboot_nuki",
                         "Reboot Nuki",
                         name,
                         baseTopic,
                         "",
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"rebootNuki\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"rebootNuki\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.rebootNuki}}" }
        });
    }
    else
    {
        removeHassTopic((char*)"button", (char*)"reboot_nuki", uidString);
    }
}

void HomeAssistantDiscovery::publishHASSConfigDoorSensor(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassTopic("binary_sensor",
                     "door_sensor",
                     uidString,
                     "_door_sensor",
                     "Door sensor",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_door_sensor_state,
                     deviceType,
                     "door",
                     "",
                     "",
                     "",
    {
        {(char*)"pl_on", (char*)"doorOpened"},
        {(char*)"pl_off", (char*)"doorClosed"},
        {(char*)"pl_not_avail", (char*)"unavailable"}
    });
}

void HomeAssistantDiscovery::publishHASSConfigAdditionalOpenerEntities(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));
    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if(_preferences->getBool(preference_conf_info_enabled, true))
    {
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));
    }

    if((int)aclPrefs[11])
    {
        // Unlatch
        publishHassTopic("button",
                         "unlatch",
                         uidString,
                         "_unlatch",
                         "Open",
                         name,
                         baseTopic,
                         "",
                         deviceType,
                         "",
                         "",
                         "",
                         String("~") + mqtt_topic_lock_action,
        {
            { (char*)"en", (char*)"false" },
            { (char*)"pl_prs", (char*)"electricStrikeActuation" }
        });
    }
    else
    {
        removeHassTopic((char*)"button", (char*)"unlatch", uidString);
    }

    publishHassTopic("binary_sensor",
                     "continuous_mode",
                     uidString,
                     "_continuous_mode",
                     "Continuous mode",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_continuous_mode,
                     deviceType,
                     "lock",
                     "",
                     "",
                     "",
    {
        {(char*)"pl_on", (char*)"on"},
        {(char*)"pl_off", (char*)"off"}
    });

    if((int)aclPrefs[12] == 1 && (int)aclPrefs[13] == 1)
    {
        publishHassTopic("switch",
                         "continuous_mode",
                         uidString,
                         "_continuous_mode",
                         "Continuous mode",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_lock_continuous_mode,
                         deviceType,
                         "",
                         "",
                         "",
                         String("~") + mqtt_topic_lock_action,
        {
            { (char*)"en", (char*)"true" },
            {(char*)"stat_on", (char*)"on"},
            {(char*)"stat_off", (char*)"off"},
            {(char*)"pl_on", (char*)"activateCM"},
            {(char*)"pl_off", (char*)"deactivateCM"}
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"continuous_mode", uidString);
    }

    publishHassTopic("binary_sensor",
                     "ring_detect",
                     uidString,
                     "_ring_detect",
                     "Ring detect",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_binary_ring,
                     deviceType,
                     "sound",
                     "",
                     "",
                     "",
    {
        {(char*)"pl_on", (char*)"ring"},
        {(char*)"pl_off", (char*)"standby"}
    });

    JsonDocument json;
    json = createHassJson(uidString, "_ring_event", "Ring", name, baseTopic, String("~") + mqtt_topic_lock_ring, deviceType, "doorbell", "", "", "", {{(char*)"val_tpl", (char*)"{ \"event_type\": \"{{ value }}\" }"}});
    json["event_types"][0] = "ring";
    json["event_types"][1] = "ringlocked";
    json["event_types"][2] = "standby";
    serializeJson(json, _buffer, _bufferSize);
    String path = createHassTopicPath("event", "ring", uidString);
    _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);

    if((int)basicOpenerConfigAclPrefs[5] == 1)
    {
        // LED enabled
        publishHassTopic("switch",
                         "led_enabled",
                         uidString,
                         "_led_enabled",
                         "LED enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:led-variant-on" },
            { (char*)"pl_on", (char*)"{ \"ledFlashEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"ledFlashEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.ledFlashEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"led_enabled", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[4] == 1)
    {
        // Button enabled
        publishHassTopic("switch",
                         "button_enabled",
                         uidString,
                         "_button_enabled",
                         "Button enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:radiobox-marked" },
            { (char*)"pl_on", (char*)"{ \"buttonEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"buttonEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.buttonEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"button_enabled", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[15] == 1)
    {
        publishHassTopic("number",
                         "sound_level",
                         uidString,
                         "_sound_level",
                         "Sound level",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:volume-source" },
            { (char*)"cmd_tpl", (char*)"{ \"soundLevel\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.soundLevel}}" },
            { (char*)"min", (char*)"0" },
            { (char*)"max", (char*)"255" },
            { (char*)"mode", (char*)"slider" },
            { (char*)"step", (char*)"25.5" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"sound_level", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[3] == 1)
    {
        // Pairing enabled
        publishHassTopic("switch",
                         "pairing_enabled",
                         uidString,
                         "_pairing_enabled",
                         "Pairing enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"pairingEnabled\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"pairingEnabled\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.pairingEnabled}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"pairing_enabled", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[6] == 1)
    {
        publishHassTopic("number",
                         "timezone_offset",
                         uidString,
                         "_timezone_offset",
                         "Timezone offset",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"ic", (char*)"mdi:timer-cog-outline" },
            { (char*)"cmd_tpl", (char*)"{ \"timeZoneOffset\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.timeZoneOffset}}" },
            { (char*)"min", (char*)"0" },
            { (char*)"max", (char*)"60" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"timezone_offset", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[7] == 1)
    {
        // DST Mode
        publishHassTopic("switch",
                         "dst_mode",
                         uidString,
                         "_dst_mode",
                         "DST mode European",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_basic_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"dstMode\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"dstMode\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.dstMode}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"dst_mode", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[8] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_fob_action_1", "Fob action 1", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.fobAction1}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"fobAction1\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Toggle RTO";
        json["options"][2] = "Activate RTO";
        json["options"][3] = "Deactivate RTO";
        json["options"][4] = "Open";
        json["options"][5] = "Ring";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "fob_action_1", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"fob_action_1", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[9] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_fob_action_2", "Fob action 2", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.fobAction2}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"fobAction2\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Toggle RTO";
        json["options"][2] = "Activate RTO";
        json["options"][3] = "Deactivate RTO";
        json["options"][4] = "Open";
        json["options"][5] = "Ring";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "fob_action_2", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"fob_action_2", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[10] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_fob_action_3", "Fob action 3", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.fobAction3}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"fobAction3\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Toggle RTO";
        json["options"][2] = "Activate RTO";
        json["options"][3] = "Deactivate RTO";
        json["options"][4] = "Open";
        json["options"][5] = "Ring";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "fob_action_3", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"fob_action_3", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[12] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_advertising_mode", "Advertising mode", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.advertisingMode}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"advertisingMode\": \"{{ value }}\" }" }});
        json["options"][0] = "Automatic";
        json["options"][1] = "Normal";
        json["options"][2] = "Slow";
        json["options"][3] = "Slowest";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "advertising_mode", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"advertising_mode", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[13] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_timezone", "Timezone", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.timeZone}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"timeZone\": \"{{ value }}\" }" }});
        json["options"][0] = "Africa/Cairo";
        json["options"][1] = "Africa/Lagos";
        json["options"][2] = "Africa/Maputo";
        json["options"][3] = "Africa/Nairobi";
        json["options"][4] = "America/Anchorage";
        json["options"][5] = "America/Argentina/Buenos_Aires";
        json["options"][6] = "America/Chicago";
        json["options"][7] = "America/Denver";
        json["options"][8] = "America/Halifax";
        json["options"][9] = "America/Los_Angeles";
        json["options"][10] = "America/Manaus";
        json["options"][11] = "America/Mexico_City";
        json["options"][12] = "America/New_York";
        json["options"][13] = "America/Phoenix";
        json["options"][14] = "America/Regina";
        json["options"][15] = "America/Santiago";
        json["options"][16] = "America/Sao_Paulo";
        json["options"][17] = "America/St_Johns";
        json["options"][18] = "Asia/Bangkok";
        json["options"][19] = "Asia/Dubai";
        json["options"][20] = "Asia/Hong_Kong";
        json["options"][21] = "Asia/Jerusalem";
        json["options"][22] = "Asia/Karachi";
        json["options"][23] = "Asia/Kathmandu";
        json["options"][24] = "Asia/Kolkata";
        json["options"][25] = "Asia/Riyadh";
        json["options"][26] = "Asia/Seoul";
        json["options"][27] = "Asia/Shanghai";
        json["options"][28] = "Asia/Tehran";
        json["options"][29] = "Asia/Tokyo";
        json["options"][30] = "Asia/Yangon";
        json["options"][31] = "Australia/Adelaide";
        json["options"][32] = "Australia/Brisbane";
        json["options"][33] = "Australia/Darwin";
        json["options"][34] = "Australia/Hobart";
        json["options"][35] = "Australia/Perth";
        json["options"][36] = "Australia/Sydney";
        json["options"][37] = "Europe/Berlin";
        json["options"][38] = "Europe/Helsinki";
        json["options"][39] = "Europe/Istanbul";
        json["options"][40] = "Europe/London";
        json["options"][41] = "Europe/Moscow";
        json["options"][42] = "Pacific/Auckland";
        json["options"][43] = "Pacific/Guam";
        json["options"][44] = "Pacific/Honolulu";
        json["options"][45] = "Pacific/Pago_Pago";
        json["options"][46] = "None";

        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "timezone", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"timezone", uidString);
    }

    if((int)basicOpenerConfigAclPrefs[11] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_operating_mode", "Operating mode", name, baseTopic, String("~") + mqtt_topic_config_basic_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.operatingMode}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"operatingMode\": \"{{ value }}\" }" }});
        json["options"][0] = "Generic door opener";
        json["options"][1] = "Analogue intercom";
        json["options"][2] = "Digital intercom";
        json["options"][3] = "Siedle";
        json["options"][4] = "TCS";
        json["options"][5] = "Bticino";
        json["options"][6] = "Siedle HTS";
        json["options"][7] = "STR";
        json["options"][8] = "Ritto";
        json["options"][9] = "Fermax";
        json["options"][10] = "Comelit";
        json["options"][11] = "Urmet BiBus";
        json["options"][12] = "Urmet 2Voice";
        json["options"][13] = "Golmar";
        json["options"][14] = "SKS";
        json["options"][15] = "Spare";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "operating_mode", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"operating_mode", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[1] == 1)
    {
        // BUS mode switch analogue
        publishHassTopic("switch",
                         "bus_mode_switch",
                         uidString,
                         "_bus_mode_switch",
                         "BUS mode switch analogue",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"busModeSwitch\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"busModeSwitch\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.busModeSwitch}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"bus_mode_switch", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[2] == 1)
    {
        publishHassTopic("number",
                         "short_circuit_duration",
                         uidString,
                         "_short_circuit_duration",
                         "Short circuit duration",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"shortCircuitDuration\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.shortCircuitDuration}}" },
            { (char*)"min", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"short_circuit_duration", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[3] == 1)
    {
        publishHassTopic("number",
                         "electric_strike_delay",
                         uidString,
                         "_electric_strike_delay",
                         "Electric strike delay",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"electricStrikeDelay\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.electricStrikeDelay}}" },
            { (char*)"min", (char*)"0" },
            { (char*)"max", (char*)"30000" },
            { (char*)"step", (char*)"3000" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"electric_strike_delay", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[4] == 1)
    {
        // Random Electric Strike Delay
        publishHassTopic("switch",
                         "random_electric_strike_delay",
                         uidString,
                         "_random_electric_strike_delay",
                         "Random electric strike delay",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"randomElectricStrikeDelay\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"randomElectricStrikeDelay\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.randomElectricStrikeDelay}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"random_electric_strike_delay", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[5] == 1)
    {
        publishHassTopic("number",
                         "electric_strike_duration",
                         uidString,
                         "_electric_strike_duration",
                         "Electric strike duration",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"electricStrikeDuration\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.electricStrikeDuration}}" },
            { (char*)"min", (char*)"1000" },
            { (char*)"max", (char*)"30000" },
            { (char*)"step", (char*)"3000" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"electric_strike_duration", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[6] == 1)
    {
        // Disable RTO after ring
        publishHassTopic("switch",
                         "disable_rto_after_ring",
                         uidString,
                         "_disable_rto_after_ring",
                         "Disable RTO after ring",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"disableRtoAfterRing\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"disableRtoAfterRing\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.disableRtoAfterRing}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"disable_rto_after_ring", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[7] == 1)
    {
        publishHassTopic("number",
                         "rto_timeout",
                         uidString,
                         "_rto_timeout",
                         "RTO timeout",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"rtoTimeout\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.rtoTimeout}}" },
            { (char*)"min", (char*)"5" },
            { (char*)"max", (char*)"60" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"rto_timeout", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[8] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_doorbell_suppression", "Doorbell suppression", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.doorbellSuppression}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"doorbellSuppression\": \"{{ value }}\" }" }});
        json["options"][0] = "Off";
        json["options"][1] = "CM";
        json["options"][2] = "RTO";
        json["options"][3] = "CM & RTO";
        json["options"][4] = "Ring";
        json["options"][5] = "CM & Ring";
        json["options"][6] = "RTO & Ring";
        json["options"][7] = "CM & RTO & Ring";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "doorbell_suppression", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"doorbell_suppression", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[9] == 1)
    {
        publishHassTopic("number",
                         "doorbell_suppression_duration",
                         uidString,
                         "_doorbell_suppression_duration",
                         "Doorbell suppression duration",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"cmd_tpl", (char*)"{ \"doorbellSuppressionDuration\": \"{{ value }}\" }" },
            { (char*)"val_tpl", (char*)"{{value_json.doorbellSuppressionDuration}}" },
            { (char*)"min", (char*)"500" },
            { (char*)"max", (char*)"10000" },
            { (char*)"step", (char*)"1000" }
        });
    }
    else
    {
        removeHassTopic((char*)"number", (char*)"doorbell_suppression_duration", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[10] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_sound_ring", "Sound ring", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.soundRing}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"soundRing\": \"{{ value }}\" }" }});
        json["options"][0] = "No Sound";
        json["options"][1] = "Sound 1";
        json["options"][2] = "Sound 2";
        json["options"][3] = "Sound 3";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "sound_ring", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"sound_ring", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[11] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_sound_open", "Sound open", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.soundOpen}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"soundOpen\": \"{{ value }}\" }" }});
        json["options"][0] = "No Sound";
        json["options"][1] = "Sound 1";
        json["options"][2] = "Sound 2";
        json["options"][3] = "Sound 3";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "sound_open", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"sound_open", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[12] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_sound_rto", "Sound RTO", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.soundRto}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"soundRto\": \"{{ value }}\" }" }});
        json["options"][0] = "No Sound";
        json["options"][1] = "Sound 1";
        json["options"][2] = "Sound 2";
        json["options"][3] = "Sound 3";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "sound_rto", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"sound_rto", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[13] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_sound_cm", "Sound CM", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.soundCm}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"soundCm\": \"{{ value }}\" }" }});
        json["options"][0] = "No Sound";
        json["options"][1] = "Sound 1";
        json["options"][2] = "Sound 2";
        json["options"][3] = "Sound 3";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "sound_cm", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"sound_cm", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[14] == 1)
    {
        // Sound confirmation
        publishHassTopic("switch",
                         "sound_confirmation",
                         uidString,
                         "_sound_confirmation",
                         "Sound confirmation",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"soundConfirmation\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"soundConfirmation\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.soundConfirmation}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"sound_confirmation", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[16] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_single_button_press_action", "Single button press action", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.singleButtonPressAction}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"singleButtonPressAction\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Toggle RTO";
        json["options"][2] = "Activate RTO";
        json["options"][3] = "Deactivate RTO";
        json["options"][4] = "Toggle CM";
        json["options"][5] = "Activate CM";
        json["options"][6] = "Deactivate CM";
        json["options"][7] = "Open";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "single_button_press_action", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"single_button_press_action", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[17] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_double_button_press_action", "Double button press action", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.doubleButtonPressAction}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"doubleButtonPressAction\": \"{{ value }}\" }" }});
        json["options"][0] = "No Action";
        json["options"][1] = "Toggle RTO";
        json["options"][2] = "Activate RTO";
        json["options"][3] = "Deactivate RTO";
        json["options"][4] = "Toggle CM";
        json["options"][5] = "Activate CM";
        json["options"][6] = "Deactivate CM";
        json["options"][7] = "Open";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "double_button_press_action", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"double_button_press_action", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[18] == 1)
    {
        JsonDocument json;
        json = createHassJson(uidString, "_battery_type", "Battery type", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.batteryType}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"batteryType\": \"{{ value }}\" }" }});
        json["options"][0] = "Alkali";
        json["options"][1] = "Accumulators";
        json["options"][2] = "Lithium";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "battery_type", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"battery_type", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[19] == 1)
    {
        // Automatic battery type detection
        publishHassTopic("switch",
                         "automatic_battery_type_detection",
                         uidString,
                         "_automatic_battery_type_detection",
                         "Automatic battery type detection",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_advanced_json,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"automaticBatteryTypeDetection\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"automaticBatteryTypeDetection\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.automaticBatteryTypeDetection}}" },
            { (char*)"stat_on", (char*)"1" },
            { (char*)"stat_off", (char*)"0" }
        });
    }
    else
    {
        removeHassTopic((char*)"switch", (char*)"automatic_battery_type_detection", uidString);
    }

    if((int)advancedOpenerConfigAclPrefs[20] == 1)
    {
        // Reboot Nuki
        publishHassTopic("button",
                         "reboot_nuki",
                         uidString,
                         "_reboot_nuki",
                         "Reboot Nuki",
                         name,
                         baseTopic,
                         "",
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         String("~") + mqtt_topic_config_action,
        {
            { (char*)"en", (char*)"true" },
            { (char*)"pl_on", (char*)"{ \"rebootNuki\": \"1\"}" },
            { (char*)"pl_off", (char*)"{ \"rebootNuki\": \"0\"}" },
            { (char*)"val_tpl", (char*)"{{value_json.rebootNuki}}" }
        });
    }
    else
    {
        removeHassTopic((char*)"button", (char*)"reboot_nuki", uidString);
    }
}

void HomeAssistantDiscovery::publishHASSConfigAccessLog(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassTopic("sensor",
                     "last_action_authorization",
                     uidString,
                     "_last_action_authorization",
                     "Last action authorization",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_log,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"ic", (char*)"mdi:format-list-bulleted" },
        { (char*)"val_tpl", (char*)"{{ (value_json|selectattr('type', 'eq', 'LockAction')|selectattr('action', 'in', ['Lock', 'Unlock', 'Unlatch'])|first|default).authorizationName|default }}" }
    });

    String rollingSate = "~";
    rollingSate.concat(mqtt_topic_lock_log_rolling);

    publishHassTopic("sensor",
                     "rolling_log",
                     uidString,
                     "_rolling_log",
                     "Rolling authorization log",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_log_rolling,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"ic", (char*)"mdi:format-list-bulleted" },
        { (char*)"json_attr_t", (char*)rollingSate.c_str() },
        { (char*)"val_tpl", (char*)"{{value_json.index}}" }
    });
}

void HomeAssistantDiscovery::publishHASSConfigKeypad(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    // Keypad battery critical
    publishHassTopic("binary_sensor",
                     "keypad_battery_low",
                     uidString,
                     "_keypad_battery_low",
                     "Keypad battery low",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_battery_basic_json,
                     deviceType,
                     "battery",
                     "",
                     "diagnostic",
                     "",
    {
        {(char*)"pl_on", (char*)"1"},
        {(char*)"pl_off", (char*)"0"},
        {(char*)"val_tpl", (char*)"{{value_json.keypadCritical}}" }
    });

    // Query Keypad
    publishHassTopic("button",
                     "query_keypad",
                     uidString,
                     "_query_keypad",
                     "Query keypad",
                     name,
                     baseTopic,
                     "",
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     String("~") + mqtt_topic_query_keypad,
    {
        { (char*)"en", (char*)"false" },
        { (char*)"pl_prs", (char*)"1" }
    });

    publishHassTopic("sensor",
                     "keypad_status",
                     uidString,
                     "_keypad_stats",
                     "Keypad status",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_log,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"ic", (char*)"mdi:drag-vertical" },
        { (char*)"val_tpl", (char*)"{{ (value_json|selectattr('type', 'eq', 'KeypadAction')|first|default).completionStatus|default }}" }
    });
}

void HomeAssistantDiscovery::publishHassTopic(const String& mqttDeviceType,
        const String& mqttDeviceName,
        const String& uidString,
        const String& uidStringPostfix,
        const String& displayName,
        const String& name,
        const String& baseTopic,
        const String& stateTopic,
        const String& deviceType,
        const String& deviceClass,
        const String& stateClass,
        const String& entityCat,
        const String& commandTopic,
        std::vector<std::pair<char*, char*>> additionalEntries
                                             )
{
    if (_discoveryTopic != "")
    {
        JsonDocument json;
        json = createHassJson(uidString, uidStringPostfix, displayName, name, baseTopic, stateTopic, deviceType, deviceClass, stateClass, entityCat, commandTopic, additionalEntries);
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath(mqttDeviceType, mqttDeviceName, uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
}

String HomeAssistantDiscovery::createHassTopicPath(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString)
{
    String path = _discoveryTopic;
    path.concat("/");
    path.concat(mqttDeviceType);
    path.concat("/");
    path.concat(uidString);
    path.concat("/");
    path.concat(mqttDeviceName);
    path.concat("/config");

    return path;
}

void HomeAssistantDiscovery::removeHassTopic(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString)
{
    if (_discoveryTopic != "")
    {
        String path = createHassTopicPath(mqttDeviceType, mqttDeviceName, uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");
    }
}

void HomeAssistantDiscovery::removeHASSConfig(char* uidString)
{
    removeHassTopic((char*)"lock", (char*)"smartlock", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"battery_low", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"keypad_battery_low", uidString);
    removeHassTopic((char*)"sensor", (char*)"battery_voltage", uidString);
    removeHassTopic((char*)"sensor", (char*)"trigger", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"mqtt_connected", uidString);
    removeHassTopic((char*)"switch", (char*)"reset", uidString);
    removeHassTopic((char*)"sensor", (char*)"firmware_version", uidString);
    removeHassTopic((char*)"sensor", (char*)"hardware_version", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_version", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_build", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_latest", uidString);
    removeHassTopic((char*)"update", (char*)"nuki_hub_update", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_ip", uidString);
    removeHassTopic((char*)"button", (char*)"unlatch", uidString);
    removeHassTopic((char*)"button", (char*)"lockngo", uidString);
    removeHassTopic((char*)"button", (char*)"lockngounlatch", uidString);
    removeHassTopic((char*)"sensor", (char*)"battery_level", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"door_sensor", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"ring_detect", uidString);
    removeHassTopic((char*)"sensor", (char*)"sound_level", uidString);
    removeHassTopic((char*)"sensor", (char*)"last_action_authorization", uidString);
    removeHassTopic((char*)"sensor", (char*)"keypad_status", uidString);
    removeHassTopic((char*)"sensor", (char*)"rolling_log", uidString);
    removeHassTopic((char*)"sensor", (char*)"wifi_signal_strength", uidString);
    removeHassTopic((char*)"sensor", (char*)"bluetooth_signal_strength", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"continuous_mode", uidString);
    removeHassTopic((char*)"switch", (char*)"continuous_mode", uidString);
    removeHassTopic((char*)"button", (char*)"query_lockstate", uidString);
    removeHassTopic((char*)"button", (char*)"query_config", uidString);
    removeHassTopic((char*)"button", (char*)"query_keypad", uidString);
    removeHassTopic((char*)"button", (char*)"query_battery", uidString);
    removeHassTopic((char*)"button", (char*)"query_commandresult", uidString);
    removeHassTopic((char*)"switch", (char*)"auto_lock", uidString);
    removeHassTopic((char*)"switch", (char*)"auto_unlock", uidString);
    removeHassTopic((char*)"button", (char*)"reboot_nuki", uidString);
    removeHassTopic((char*)"switch", (char*)"double_lock", uidString);
    removeHassTopic((char*)"switch", (char*)"automatic_battery_type_detection", uidString);
    removeHassTopic((char*)"select", (char*)"battery_type", uidString);
    removeHassTopic((char*)"select", (char*)"double_button_press_action", uidString);
    removeHassTopic((char*)"select", (char*)"single_button_press_action", uidString);
    removeHassTopic((char*)"switch", (char*)"sound_confirmation", uidString);
    removeHassTopic((char*)"select", (char*)"sound_cm", uidString);
    removeHassTopic((char*)"select", (char*)"sound_rto", uidString);
    removeHassTopic((char*)"select", (char*)"sound_open", uidString);
    removeHassTopic((char*)"select", (char*)"sound_ring", uidString);
    removeHassTopic((char*)"number", (char*)"doorbell_suppression_duration", uidString);
    removeHassTopic((char*)"select", (char*)"doorbell_suppression", uidString);
    removeHassTopic((char*)"number", (char*)"rto_timeout", uidString);
    removeHassTopic((char*)"switch", (char*)"disable_rto_after_ring", uidString);
    removeHassTopic((char*)"number", (char*)"electric_strike_duration", uidString);
    removeHassTopic((char*)"switch", (char*)"random_electric_strike_delay", uidString);
    removeHassTopic((char*)"number", (char*)"electric_strike_delay", uidString);
    removeHassTopic((char*)"number", (char*)"short_circuit_duration", uidString);
    removeHassTopic((char*)"switch", (char*)"bus_mode_switch", uidString);
    removeHassTopic((char*)"select", (char*)"operating_mode", uidString);
    removeHassTopic((char*)"select", (char*)"timezone", uidString);
    removeHassTopic((char*)"select", (char*)"advertising_mode", uidString);
    removeHassTopic((char*)"select", (char*)"fob_action_3", uidString);
    removeHassTopic((char*)"select", (char*)"fob_action_2", uidString);
    removeHassTopic((char*)"select", (char*)"fob_action_1", uidString);
    removeHassTopic((char*)"switch", (char*)"dst_mode", uidString);
    removeHassTopic((char*)"number", (char*)"timezone_offset", uidString);
    removeHassTopic((char*)"switch", (char*)"pairing_enabled", uidString);
    removeHassTopic((char*)"number", (char*)"sound_level", uidString);
    removeHassTopic((char*)"switch", (char*)"button_enabled", uidString);
    removeHassTopic((char*)"switch", (char*)"led_enabled", uidString);
    removeHassTopic((char*)"number", (char*)"led_brightness", uidString);
    removeHassTopic((char*)"switch", (char*)"auto_update_enabled", uidString);
    removeHassTopic((char*)"switch", (char*)"immediate_auto_lock_enabled", uidString);
    removeHassTopic((char*)"switch", (char*)"nightmode_immediate_lock_start", uidString);
    removeHassTopic((char*)"switch", (char*)"nightmode_auto_unlock", uidString);
    removeHassTopic((char*)"switch", (char*)"nightmode_auto_lock", uidString);
    removeHassTopic((char*)"text", (char*)"nightmode_end_time", uidString);
    removeHassTopic((char*)"text", (char*)"nightmode_start_time", uidString);
    removeHassTopic((char*)"switch", (char*)"nightmode_enabled", uidString);
    removeHassTopic((char*)"number", (char*)"auto_lock_timeout", uidString);
    removeHassTopic((char*)"number", (char*)"unlatch_duration", uidString);
    removeHassTopic((char*)"switch", (char*)"detached_cylinder", uidString);
    removeHassTopic((char*)"number", (char*)"lockngo_timeout", uidString);
    removeHassTopic((char*)"number", (char*)"unlocked_locked_transition_offset_degrees", uidString);
    removeHassTopic((char*)"number", (char*)"single_locked_position_offset_degrees", uidString);
    removeHassTopic((char*)"number", (char*)"locked_position_offset_degrees", uidString);
    removeHassTopic((char*)"number", (char*)"unlocked_position_offset_degrees", uidString);
    removeHassTopic((char*)"switch", (char*)"pairing_enabled", uidString);
    removeHassTopic((char*)"switch", (char*)"auto_unlatch", uidString);
    removeHassTopic((char*)"sensor", (char*)"network_device", uidString);
    removeHassTopic((char*)"switch", (char*)"webserver", uidString);
    removeHassTopic((char*)"sensor", (char*)"uptime", uidString);
    removeHassTopic((char*)"sensor", (char*)"mqtt_log", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"hybrid_connected", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_restart_reason", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_restart_reason_esp", uidString);
}

void HomeAssistantDiscovery::removeHASSConfigTopic(char *deviceType, char *name, char *uidString)
{
    removeHassTopic(deviceType, name, uidString);
}

JsonDocument HomeAssistantDiscovery::createHassJson(const String& uidString,
        const String& uidStringPostfix,
        const String& displayName,
        const String& name,
        const String& baseTopic,
        const String& stateTopic,
        const String& deviceType,
        const String& deviceClass,
        const String& stateClass,
        const String& entityCat,
        const String& commandTopic,
        std::vector<std::pair<char*, char*>> additionalEntries
                                                   )
{
    JsonDocument json;
    json.clear();
    JsonObject dev = json["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(String("nuki_") + uidString);

    json["~"] = baseTopic;
    json["name"] = displayName;
    json["unique_id"] = String(uidString) + uidStringPostfix;

    if(deviceClass != "")
    {
        json["dev_cla"] = deviceClass;
    }

    if(stateTopic != "")
    {
        json["stat_t"] = stateTopic;
    }

    if(stateClass != "")
    {
        json["stat_cla"] = stateClass;
    }

    if(entityCat != "")
    {
        json["ent_cat"] = entityCat;
    }

    if(commandTopic != "")
    {
        json["cmd_t"] = commandTopic;
    }

    json["avty"]["t"] = _baseTopic + mqtt_topic_mqtt_connection_state;

    for(const auto& entry : additionalEntries)
    {
        if(strcmp(entry.second, "true") == 0)
        {
            json[entry.first] = true;
        }
        else if(strcmp(entry.second, "false") == 0)
        {
            json[entry.first] = false;
        }
        else
        {
            json[entry.first] = entry.second;
        }
    }

    return json;
}
