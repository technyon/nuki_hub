#include "HomeAssistantDiscovery.h"
#include "Config.h"
#include "EspMillis.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)
#include "WiFi.h"
#else
#include "esp_mac.h"
#endif

HomeAssistantDiscovery::HomeAssistantDiscovery(NetworkDevice* device, Preferences *preferences, char* buffer, size_t bufferSize)
    : _device(device),
      _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize)
{
    _baseTopic = _preferences->getString(preference_mqtt_lock_path);
    _hostname = _preferences->getString(preference_hostname, "");
}

void HomeAssistantDiscovery::setupHASS(int type, uint32_t nukiId, char* nukiName, const char* firmwareVersion, const char* hardwareVersion, bool hasDoorSensor, bool hasKeypad)
{
    uint64_t savedDevId = _preferences->getULong64(preference_nukihub_id, 0);
    uint64_t curDevId;

    #if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)
    curDevId = 0;
    std::string macString = std::string(WiFi.macAddress().c_str());
    std::string::iterator i;
    std::string::iterator end = macString.end();

    for(i = macString.begin(); i != end; ++i) {
        char let = *i;
        if (let >= '0' && let <= '9') {
            curDevId = curDevId*0xf + (let-'0');
        } else if (let >= 'a' && let <= 'f') {
            curDevId = curDevId*0xf + (let-'a'+10);
        } else if (let >= 'A' && let <= 'F') {
            curDevId = curDevId*0xf + (let-'A'+10);
        }
    }
    #else
    uint8_t mac[8] = {0};
    esp_efuse_mac_get_default(mac);
    memcpy(&curDevId, &mac, 8);
    #endif

    if (savedDevId == 0)
    {
        _preferences->putULong64(preference_nukihub_id, curDevId);
        char uidString[20];
        itoa(_preferences->getUInt(preference_device_id_lock, 0), uidString, 10);
        removeHASSConfig(uidString);
        espDelayAck(3000);
    }
    else if(savedDevId != curDevId)
    {
        _preferences->putULong64(preference_nukihub_id, curDevId);
        Log->print("Efuse ID: ");
        Log->println(curDevId);
        Log->print("Saved ID: ");
        Log->println(savedDevId);
        Log->println("Efuse ID and Nuki Hub device ID do not match, removing HomeAssistant setup for incorrect Nuki Hub device ID.");
        char uidString[20];
        itoa(_preferences->getUInt(preference_device_id_lock, 0), uidString, 10);
        removeHASSConfig(uidString);
        espDelayAck(3000);
        itoa(savedDevId, uidString, 10);
        removeHASSConfig(uidString);
        espDelayAck(3000);
    }

    sprintf(_nukiHubUidString, "%" PRIu64, curDevId);
    Log->print("HA UID: ");
    Log->println(_nukiHubUidString);

    char uidString[20];
    itoa(nukiId, uidString, 16);
    bool publishAuthData = _preferences->getBool(preference_publish_authdata, false);

    if(type == 0)
    {
        publishHASSNukiHubConfig();
        Log->println("HomeAssistant setup for Nuki Hub completed.");
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
        Log->println("HomeAssistant setup for lock completed.");
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

        Log->println("HomeAssistant setup for opener completed.");
    }
}

void HomeAssistantDiscovery::disableHASS()
{
    removeHASSConfig(_nukiHubUidString);
    espDelayAck(3000);

    char uidString[20];

    if(_preferences->getUInt(preference_nuki_id_lock, 0) != 0)
    {
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        removeHASSConfig(uidString);
        espDelayAck(3000);
    }
    if(_preferences->getUInt(preference_nuki_id_opener, 0) != 0)
    {
        itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
        removeHASSConfig(uidString);
        espDelayAck(3000);
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
    json["dev"]["mdl"] = "Nuki Hub";
    json["dev"]["name"] = _hostname.c_str();
    json["dev"]["sw"] = NUKI_HUB_VERSION;
    json["dev"]["hw"] = NUKI_HUB_HW;
    _uidToName[_nukiHubUidString] = _hostname.c_str();

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

    // Webserver enabled
    publishHassTopic("switch",
                     "webserver",
                     _nukiHubUidString,
                     "_webserver",
                     "Webserver enabled",
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
                     "Version",
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
                     "Build",
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

    // Restart reason
    publishHassTopic("sensor",
                     "nuki_hub_restart_reason",
                     _nukiHubUidString,
                     "_nuki_hub_restart_reason",
                     "Restart reason",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_restart_reason_fw,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    { { (char*)"en", (char*)"true" }});

    // Restart reason ESP
    publishHassTopic("sensor",
                     "nuki_hub_restart_reason_esp",
                     _nukiHubUidString,
                     "_nuki_hub_restart_reason_esp",
                     "Restart reason ESP",
                     _hostname.c_str(),
                     _baseTopic.c_str(),
                     String("~") + mqtt_topic_restart_reason_esp,
                     "NukiHub",
                     "",
                     "",
                     "diagnostic",
                     "",
    { { (char*)"en", (char*)"true" }});

    if(_preferences->getBool(preference_check_updates, false))
    {
        // Nuki Hub latest
        publishHassTopic("sensor",
                         "nuki_hub_latest",
                         _nukiHubUidString,
                         "_nuki_hub_latest",
                         "Latest Version",
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

        // Nuki Hub update
        char latest_version_topic[250];
        _baseTopic.toCharArray(latest_version_topic,_baseTopic.length() + 1);
        strcat(latest_version_topic, mqtt_topic_info_nuki_hub_latest);

        if(!_preferences->getBool(preference_update_from_mqtt, false))
        {
            publishHassTopic("update",
                             "nuki_hub_update",
                             _nukiHubUidString,
                             "_nuki_hub_update",
                             "Firmware update",
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
                             "Firmware update",
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

    // IP Address
    publishHassTopic("sensor",
                     "nuki_hub_ip",
                     _nukiHubUidString,
                     "_nuki_hub_ip",
                     "IP",
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
        removeHASSConfigTopic((char*)"sensor", (char*)"last_action_authorization", uidString);
    }
    else
    {
        removeHASSConfigTopic((char*)"sensor", (char*)"rolling_log", uidString);
    }
    if(hasKeypad)
    {
        publishHASSConfigKeypad(deviceType, baseTopic, name, uidString);
        removeHASSConfigTopic((char*)"sensor", (char*)"keypad_status", uidString);
    }
    else
    {
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
    _uidToName[uidString] = name;

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
    json["avty"][1]["t"] = String("~") + String(mqtt_topic_lock_availability);
    json["avty_mode"] = "all";
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

    // Battery charging
    publishHassTopic("binary_sensor",
                     "battery_charging",
                     uidString,
                     "_battery_charging",
                     "Battery charging",
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
        {(char*)"val_tpl", (char*)"{{value_json.charging}}" }
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

    if(_preferences->getBool(preference_official_hybrid_enabled, false) && strcmp(deviceType, "SmartLock") == 0)
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

static const HassEntity lockEntities[] =
{
    {"button", "unlatch", "Open", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 2, -1, 0, {{"en", "false"}, {"pl_prs", "unlatch"}}},
    {"button", "lockngo", "Lock 'n' Go", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 3, -1, 0, {{"en", "false"}, {"pl_prs", "lockNgo"}}},
    {"binary_sensor", "lock_n_go_active", "Lock 'n' Go active", mqtt_topic_lock_state, "running", "", "", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "1"}, {"pl_off", "0"}, {"val_tpl", "{{1 if value == 'unlockedLnga' else 0}}"}}},
    {"button", "lockngounlatch", "Lock 'n' Go with unlatch", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 4, -1, 0, {{"en", "false"}, {"pl_prs", "lockNgoUnlatch"}}},
    {"button", "query_battery", "Query battery", nullptr, "", "", "diagnostic", mqtt_topic_query_battery, HassAcl::None, 0, -1, 0, {{"en", "false"}, {"pl_prs", "1"}}},
    {"switch", "led_enabled", "LED enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 6, -1, 0, {{"en", "true"}, {"ic", "mdi:led-variant-on"}, {"pl_on", "{ \"ledEnabled\": \"1\"}"}, {"pl_off", "{ \"ledEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.ledEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "button_enabled", "Button enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 5, -1, 0, {{"en", "true"}, {"ic", "mdi:radiobox-marked"}, {"pl_on", "{ \"buttonEnabled\": \"1\"}"}, {"pl_off", "{ \"buttonEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.buttonEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "auto_lock", "Auto lock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 19, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoLockEnabled\": \"1\"}"}, {"pl_off", "{ \"autoLockEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.autoLockEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "auto_unlock", "Auto unlock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 12, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoUnLockDisabled\": \"0\"}"}, {"pl_off", "{ \"autoUnLockDisabled\": \"1\"}"}, {"val_tpl", "{{value_json.autoUnLockDisabled}}"}, {"stat_on", "0"}, {"stat_off", "1"}}},
    {"switch", "double_lock", "Double lock", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 13, -1, 0, {{"en", "true"}, {"pl_on", "{ \"singleLock\": \"0\"}"}, {"pl_off", "{ \"singleLock\": \"1\"}"}, {"val_tpl", "{{value_json.singleLock}}"}, {"stat_on", "0"}, {"stat_off", "1"}}},
    {"sensor", "battery_level", "Battery level", mqtt_topic_battery_basic_json, "battery", "measurement", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"unit_of_meas", "%"}, {"val_tpl", "{{value_json.level}}"}}},
    {"number", "led_brightness", "LED brightness", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 7, -1, 0, {{"en", "true"}, {"ic", "mdi:brightness-6"}, {"cmd_tpl", "{ \"ledBrightness\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.ledBrightness}}"}, {"min", "0"}, {"max", "5"}}},
    {"switch", "auto_unlatch", "Auto unlatch", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 3, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoUnlatch\": \"1\"}"}, {"pl_off", "{ \"autoUnlatch\": \"0\"}"}, {"val_tpl", "{{value_json.autoUnlatch}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "pairing_enabled", "Pairing enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 4, -1, 0, {{"en", "true"}, {"pl_on", "{ \"pairingEnabled\": \"1\"}"}, {"pl_off", "{ \"pairingEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.pairingEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "timezone_offset", "Timezone offset", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 8, -1, 0, {{"en", "true"}, {"ic", "mdi:timer-cog-outline"}, {"cmd_tpl", "{ \"timeZoneOffset\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.timeZoneOffset}}"}, {"min", "0"}, {"max", "60"}}},
    {"switch", "dst_mode", "DST mode European", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 9, -1, 0, {{"en", "true"}, {"pl_on", "{ \"dstMode\": \"1\"}"}, {"pl_off", "{ \"dstMode\": \"0\"}"}, {"val_tpl", "{{value_json.dstMode}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "unlocked_position_offset_degrees", "Unlocked position offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 0, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"unlockedPositionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.unlockedPositionOffsetDegrees}}"}, {"min", "-90"}, {"max", "180"}}},
    {"number", "locked_position_offset_degrees", "Locked position offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 1, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"lockedPositionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.lockedPositionOffsetDegrees}}"}, {"min", "-180"}, {"max", "90"}}},
    {"number", "single_locked_position_offset_degrees", "Single locked position offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 2, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"singleLockedPositionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.singleLockedPositionOffsetDegrees}}"}, {"min", "-180"}, {"max", "180"}}},
    {"number", "unlocked_locked_transition_offset_degrees", "Unlocked to locked transition offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 3, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"unlockedToLockedTransitionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.unlockedToLockedTransitionOffsetDegrees}}"}, {"min", "-180"}, {"max", "180"}}},
    {"number", "lockngo_timeout", "Lock n Go timeout", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 4, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"lockNgoTimeout\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.lockNgoTimeout}}"}, {"min", "5"}, {"max", "60"}}},
    {"switch", "detached_cylinder", "Detached cylinder", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 7, -1, 0, {{"en", "true"}, {"pl_on", "{ \"detachedCylinder\": \"1\"}"}, {"pl_off", "{ \"detachedCylinder\": \"0\"}"}, {"val_tpl", "{{value_json.detachedCylinder}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "automatic_battery_type_detection", "Automatic battery type detection", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 9, -1, -1, {{"en", "true"}, {"pl_on", "{ \"automaticBatteryTypeDetection\": \"1\"}"}, {"pl_off", "{ \"automaticBatteryTypeDetection\": \"0\"}"}, {"val_tpl", "{{value_json.automaticBatteryTypeDetection}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "unlatch_duration", "Unlatch duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 10, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"unlatchDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.unlatchDuration}}"}, {"min", "1"}, {"max", "30"}}},
    {"number", "auto_lock_timeout", "Auto lock timeout", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 11, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"autoLockTimeOut\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.autoLockTimeOut}}"}, {"min", "30"}, {"max", "1800"}}},
    {"switch", "nightmode_enabled", "Nightmode enabled", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 13, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeEnabled\": \"1\"}"}, {"pl_off", "{ \"nightModeEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.nightModeEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"text", "nightmode_start_time", "Nightmode start time", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 14, -1, 0, {{"en", "true"}, {"pattern", "([0-1][0-9]|2[0-3]):[0-5][0-9]"}, {"cmd_tpl", "{ \"nightModeStartTime\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.nightModeStartTime}}"}, {"min", "5"}, {"max", "5"}}},
    {"text", "nightmode_end_time", "Nightmode end time", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 15, -1, 0, {{"en", "true"}, {"pattern", "([0-1][0-9]|2[0-3]):[0-5][0-9]"}, {"cmd_tpl", "{ \"nightModeEndTime\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.nightModeEndTime}}"}, {"min", "5"}, {"max", "5"}}},
    {"switch", "nightmode_auto_lock", "Nightmode auto lock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 16, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeAutoLockEnabled\": \"1\"}"}, {"pl_off", "{ \"nightModeAutoLockEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.nightModeAutoLockEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "nightmode_auto_unlock", "Nightmode auto unlock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 17, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeAutoUnlockDisabled\": \"0\"}"}, {"pl_off", "{ \"nightModeAutoUnlockDisabled\": \"1\"}"}, {"val_tpl", "{{value_json.nightModeAutoUnlockDisabled}}"}, {"stat_on", "0"}, {"stat_off", "1"}}},
    {"switch", "nightmode_immediate_lock_start", "Nightmode immediate lock on start", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 18, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeImmediateLockOnStart\": \"1\"}"}, {"pl_off", "{ \"nightModeImmediateLockOnStart\": \"0\"}"}, {"val_tpl", "{{value_json.nightModeImmediateLockOnStart}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "immediate_auto_lock_enabled", "Immediate auto lock enabled", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 20, -1, 0, {{"en", "true"}, {"pl_on", "{ \"immediateAutoLockEnabled\": \"1\"}"}, {"pl_off", "{ \"immediateAutoLockEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.immediateAutoLockEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "auto_update_enabled", "Auto update enabled", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 21, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoUpdateEnabled\": \"1\"}"}, {"pl_off", "{ \"autoUpdateEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.autoUpdateEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "enable_slow_speed_during_nightmode", "Enable slow speed during nightmode", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 24, -1, 1, {{"en", "true"}, {"pl_on", "{ \"enableSlowSpeedDuringNightMode\": \"1\"}"}, {"pl_off", "{ \"enableSlowSpeedDuringNightMode\": \"0\"}"}, {"val_tpl", "{{value_json.enableSlowSpeedDuringNightMode}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"button", "reboot_nuki", "Reboot Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 22, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"rebootNuki\": \"1\"}"}}},
    {"button", "recalibrate_nuki", "Recalibrate Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 25, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"recalibrateNuki\": \"1\"}"}}},
};

static const HassEntity openerEntities[] =
{
    {"button", "unlatch", "Open", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 11, -1, 0, {{"en", "false"}, {"pl_prs", "electricStrikeActuation"}}},
    {"binary_sensor", "continuous_mode", "Continuous mode", mqtt_topic_lock_continuous_mode, "lock", "", "", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "on"}, {"pl_off", "off"}}},
    {"switch", "continuous_mode", "Continuous mode", mqtt_topic_lock_continuous_mode, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 12, 13, 0, {{"en", "true"}, {"stat_on", "on"}, {"stat_off", "off"}, {"pl_on", "activateCM"}, {"pl_off", "deactivateCM"}}},
    {"binary_sensor", "ring_detect", "Ring detect", mqtt_topic_lock_binary_ring, "sound", "", "", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "ring"}, {"pl_off", "standby"}}},
    {"switch", "led_enabled", "LED enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 5, -1, 0, {{"en", "true"}, {"ic", "mdi:led-variant-on"}, {"pl_on", "{ \"ledFlashEnabled\": \"1\"}"}, {"pl_off", "{ \"ledFlashEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.ledFlashEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "button_enabled", "Button enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 4, -1, 0, {{"en", "true"}, {"ic", "mdi:radiobox-marked"}, {"pl_on", "{ \"buttonEnabled\": \"1\"}"}, {"pl_off", "{ \"buttonEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.buttonEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "sound_level", "Sound level", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 15, -1, 0, {{"en", "true"}, {"ic", "mdi:volume-source"}, {"cmd_tpl", "{ \"soundLevel\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.soundLevel}}"}, {"min", "0"}, {"max", "255"}, {"mode", "slider"}, {"step", "25.5"}}},
    {"switch", "pairing_enabled", "Pairing enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 3, -1, 0, {{"en", "true"}, {"pl_on", "{ \"pairingEnabled\": \"1\"}"}, {"pl_off", "{ \"pairingEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.pairingEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "timezone_offset", "Timezone offset", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 6, -1, 0, {{"en", "true"}, {"ic", "mdi:timer-cog-outline"}, {"cmd_tpl", "{ \"timeZoneOffset\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.timeZoneOffset}}"}, {"min", "0"}, {"max", "60"}}},
    {"switch", "dst_mode", "DST mode European", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 7, -1, 0, {{"en", "true"}, {"pl_on", "{ \"dstMode\": \"1\"}"}, {"pl_off", "{ \"dstMode\": \"0\"}"}, {"val_tpl", "{{value_json.dstMode}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "bus_mode_switch", "BUS mode switch analogue", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 1, -1, 0, {{"en", "true"}, {"pl_on", "{ \"busModeSwitch\": \"1\"}"}, {"pl_off", "{ \"busModeSwitch\": \"0\"}"}, {"val_tpl", "{{value_json.busModeSwitch}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "short_circuit_duration", "Short circuit duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 2, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"shortCircuitDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.shortCircuitDuration}}"}, {"min", "0"}}},
    {"number", "electric_strike_delay", "Electric strike delay", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 3, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"electricStrikeDelay\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.electricStrikeDelay}}"}, {"min", "0"}, {"max", "30000"}, {"step", "3000"}}},
    {"switch", "random_electric_strike_delay", "Random electric strike delay", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 4, -1, 0, {{"en", "true"}, {"pl_on", "{ \"randomElectricStrikeDelay\": \"1\"}"}, {"pl_off", "{ \"randomElectricStrikeDelay\": \"0\"}"}, {"val_tpl", "{{value_json.randomElectricStrikeDelay}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "electric_strike_duration", "Electric strike duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 5, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"electricStrikeDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.electricStrikeDuration}}"}, {"min", "1000"}, {"max", "30000"}, {"step", "3000"}}},
    {"switch", "disable_rto_after_ring", "Disable RTO after ring", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 6, -1, 0, {{"en", "true"}, {"pl_on", "{ \"disableRtoAfterRing\": \"1\"}"}, {"pl_off", "{ \"disableRtoAfterRing\": \"0\"}"}, {"val_tpl", "{{value_json.disableRtoAfterRing}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "rto_timeout", "RTO timeout", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 7, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"rtoTimeout\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.rtoTimeout}}"}, {"min", "5"}, {"max", "60"}}},
    {"number", "doorbell_suppression_duration", "Doorbell suppression duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 9, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"doorbellSuppressionDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.doorbellSuppressionDuration}}"}, {"min", "500"}, {"max", "10000"}, {"step", "1000"}}},
    {"switch", "sound_confirmation", "Sound confirmation", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 14, -1, 0, {{"en", "true"}, {"pl_on", "{ \"soundConfirmation\": \"1\"}"}, {"pl_off", "{ \"soundConfirmation\": \"0\"}"}, {"val_tpl", "{{value_json.soundConfirmation}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "automatic_battery_type_detection", "Automatic battery type detection", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 19, -1, 0, {{"en", "true"}, {"pl_on", "{ \"automaticBatteryTypeDetection\": \"1\"}"}, {"pl_off", "{ \"automaticBatteryTypeDetection\": \"0\"}"}, {"val_tpl", "{{value_json.automaticBatteryTypeDetection}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"button", "reboot_nuki", "Reboot Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 20, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"rebootNuki\": \"1\"}"}}},
    {"button", "recalibrate_nuki", "Recalibrate Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 21, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"recalibrateNuki\": \"1\"}"}}},
};

void HomeAssistantDiscovery::publishHassEntities(const HassEntity* entities, size_t count, char* deviceType, const char* baseTopic, char* name, char* uidString, const uint32_t* aclPrefs, const uint32_t* basicAclPrefs, const uint32_t* advancedAclPrefs)
{
    bool geminiEnabled = _preferences->getBool(preference_lock_gemini_enabled, false);

    for(size_t i = 0; i < count; i++)
    {
        const HassEntity& entity = entities[i];
        bool allowed = true;

        switch(entity.acl)
        {
        case HassAcl::Action:
            allowed = aclPrefs[entity.aclIndex] != 0 && (entity.aclIndex2 < 0 || aclPrefs[entity.aclIndex2] != 0);
            break;
        case HassAcl::Basic:
            allowed = basicAclPrefs[entity.aclIndex] == 1;
            break;
        case HassAcl::Advanced:
            allowed = advancedAclPrefs[entity.aclIndex] == 1;
            break;
        default:
            break;
        }

        if(entity.gemini > 0)
        {
            allowed = allowed && geminiEnabled;
        }
        else if(entity.gemini < 0)
        {
            allowed = allowed && !geminiEnabled;
        }

        if(!allowed)
        {
            removeHassTopic(entity.component, entity.name, uidString);
            continue;
        }

        publishHassTopic(entity.component,
                         entity.name,
                         uidString,
                         String("_") + entity.name,
                         entity.displayName,
                         name,
                         baseTopic,
                         entity.stateTopic != nullptr ? String("~") + entity.stateTopic : String(""),
                         deviceType,
                         entity.deviceClass,
                         entity.stateClass,
                         entity.entityCategory,
                         entity.commandTopic != nullptr ? String("~") + entity.commandTopic : String(""),
                         entity.additionalEntries);
    }
}

void HomeAssistantDiscovery::publishHASSConfigAdditionalLockEntities(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedLockConfigAclPrefs[26] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if(_preferences->getBool(preference_conf_info_enabled, true))
    {
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
    }

    publishHassEntities(lockEntities, sizeof(lockEntities) / sizeof(lockEntities[0]), deviceType, baseTopic, name, uidString, aclPrefs, basicLockConfigAclPrefs, advancedLockConfigAclPrefs);

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

    if((int)advancedLockConfigAclPrefs[8] == 1 && !_preferences->getBool(preference_lock_gemini_enabled, false))
    {
        JsonDocument json;
        json = createHassJson(uidString, "_battery_type", "Battery type", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.batteryType}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"batteryType\": \"{{ value }}\" }" }});
        json["options"][0] = "Alkali";
        json["options"][1] = "Accumulators";
        json["options"][2] = "Lithium";
        json["options"][3] = "No Warnings";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "battery_type", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"battery_type", uidString);
    }

    if((int)advancedLockConfigAclPrefs[23] == 1 && _preferences->getBool(preference_lock_gemini_enabled, false))
    {
        JsonDocument json;
        json = createHassJson(uidString, "_motor_speed", "Motor speed", name, baseTopic, String("~") + mqtt_topic_config_advanced_json, deviceType, "", "", "config", String("~") + mqtt_topic_config_action, {{ (char*)"val_tpl", (char*)"{{value_json.motorSpeed}}" }, { (char*)"en", (char*)"true" }, { (char*)"cmd_tpl", (char*)"{ \"motorSpeed\": \"{{ value }}\" }" }});
        json["options"][0] = "Standard";
        json["options"][1] = "Insane";
        json["options"][2] = "Gentle";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "motor_speed", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"motor_speed", uidString);
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
    uint32_t advancedOpenerConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if(_preferences->getBool(preference_conf_info_enabled, true))
    {
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));
    }

    publishHassEntities(openerEntities, sizeof(openerEntities) / sizeof(openerEntities[0]), deviceType, baseTopic, name, uidString, aclPrefs, basicOpenerConfigAclPrefs, advancedOpenerConfigAclPrefs);

    JsonDocument json;

    json = createHassJson(uidString, "_ring_event", "Ring", name, baseTopic, String("~") + mqtt_topic_lock_ring, deviceType, "doorbell", "", "", "", {{(char*)"val_tpl", (char*)"{ \"event_type\": \"{{ value }}\" }"}});

    json["event_types"][0] = "ring";

    json["event_types"][1] = "ringlocked";

    json["event_types"][2] = "standby";

    serializeJson(json, _buffer, _bufferSize);

    String path = createHassTopicPath("event", "ring", uidString);

    _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);

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
        json["options"][3] = "No Warnings";
        serializeJson(json, _buffer, _bufferSize);
        String path = createHassTopicPath("select", "battery_type", uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
    else
    {
        removeHassTopic((char*)"select", (char*)"battery_type", uidString);
    }
}

void HomeAssistantDiscovery::publishHASSConfigAccessLog(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
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
        const std::vector<std::pair<const char*, const char*>>& additionalEntries
                                             )
{
    if (_preferences->getString(preference_mqtt_hass_discovery, "") != "")
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
    String path = _preferences->getString(preference_mqtt_hass_discovery, "");
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
    if (_preferences->getString(preference_mqtt_hass_discovery, "") != "")
    {
        String path = createHassTopicPath(mqttDeviceType, mqttDeviceName, uidString);
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");
    }
}

void HomeAssistantDiscovery::removeHASSConfig(char* uidString)
{
    Log->println("Removing HASS entities with ID:");
    Log->println(uidString);

    removeHassTopic((char*)"lock", (char*)"smartlock", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"battery_low", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"battery_charging", uidString);
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
    removeHassTopic((char*)"binary_sensor", (char*)"door_sensor", uidString);
    removeHassTopic((char*)"sensor", (char*)"sound_level", uidString);
    removeHassTopic((char*)"sensor", (char*)"last_action_authorization", uidString);
    removeHassTopic((char*)"sensor", (char*)"keypad_status", uidString);
    removeHassTopic((char*)"sensor", (char*)"rolling_log", uidString);
    removeHassTopic((char*)"sensor", (char*)"wifi_signal_strength", uidString);
    removeHassTopic((char*)"sensor", (char*)"bluetooth_signal_strength", uidString);
    removeHassTopic((char*)"button", (char*)"query_lockstate", uidString);
    removeHassTopic((char*)"button", (char*)"query_config", uidString);
    removeHassTopic((char*)"button", (char*)"query_keypad", uidString);
    removeHassTopic((char*)"button", (char*)"query_commandresult", uidString);
    removeHassTopic((char*)"select", (char*)"battery_type", uidString);
    removeHassTopic((char*)"select", (char*)"double_button_press_action", uidString);
    removeHassTopic((char*)"select", (char*)"single_button_press_action", uidString);
    removeHassTopic((char*)"select", (char*)"sound_cm", uidString);
    removeHassTopic((char*)"select", (char*)"sound_rto", uidString);
    removeHassTopic((char*)"select", (char*)"sound_open", uidString);
    removeHassTopic((char*)"select", (char*)"sound_ring", uidString);
    removeHassTopic((char*)"select", (char*)"doorbell_suppression", uidString);
    removeHassTopic((char*)"select", (char*)"operating_mode", uidString);
    removeHassTopic((char*)"select", (char*)"timezone", uidString);
    removeHassTopic((char*)"select", (char*)"advertising_mode", uidString);
    removeHassTopic((char*)"select", (char*)"fob_action_3", uidString);
    removeHassTopic((char*)"select", (char*)"fob_action_2", uidString);
    removeHassTopic((char*)"select", (char*)"fob_action_1", uidString);
    removeHassTopic((char*)"sensor", (char*)"network_device", uidString);
    removeHassTopic((char*)"switch", (char*)"webserver", uidString);
    removeHassTopic((char*)"sensor", (char*)"uptime", uidString);
    removeHassTopic((char*)"sensor", (char*)"mqtt_log", uidString);
    removeHassTopic((char*)"binary_sensor", (char*)"hybrid_connected", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_restart_reason", uidString);
    removeHassTopic((char*)"sensor", (char*)"nuki_hub_restart_reason_esp", uidString);

    for(const HassEntity& entity : lockEntities)
    {
        removeHassTopic(entity.component, entity.name, uidString);
    }
    for(const HassEntity& entity : openerEntities)
    {
        removeHassTopic(entity.component, entity.name, uidString);
    }
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
        const std::vector<std::pair<const char*, const char*>>& additionalEntries
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

    json["dev"]["name"] = _uidToName[uidString];

    if (displayName != "Rolling authorization log")
    {
        json["avty"][0]["t"] = _baseTopic + mqtt_topic_mqtt_connection_state;

        if (displayName != "Query lock state" && uidString.length() < 10)
        {
            json["avty"][1]["t"] = String("~") + String(mqtt_topic_lock_availability);
            json["avty_mode"] = "all";
        }
    }

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
