#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "FS.h"
#include "SPIFFS.h"
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
#include "esp_psram.h"
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
#include <esp_wifi.h>
#include <WiFi.h>
#endif
#include "driver/gpio.h"

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)
#include "esp_hosted.h"
static esp_hosted_coprocessor_fwver_t slave_version_struct = {
  .major1 = 0,
  .minor1 = 0,
  .patch1 = 0
};
static esp_hosted_coprocessor_fwver_t host_version_struct = {
  .major1 = ESP_HOSTED_VERSION_MAJOR_1,
  .minor1 = ESP_HOSTED_VERSION_MINOR_1,
  .patch1 = ESP_HOSTED_VERSION_PATCH_1
};
#endif

#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::buildInfoHtml(PsychicRequest *request, PsychicResponse* resp)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<h3>System Information</h3><div class=\"actions\"><button type=\"button\" data-copy=\"#preInfo\">Copy for GitHub issue with all personal information redacted</button></div><pre id=\"preInfo\">");
    response.print("------------ NUKI HUB ------------");
    response.print("\nDevice: ");
    response.print(NUKI_HUB_HW);
    response.print("\nVersion: ");
    response.print(NUKI_HUB_VERSION);
    response.print("\nBuild: ");
    response.print(NUKI_HUB_BUILD);
#ifndef DEBUG_NUKIHUB
    response.print("\nBuild type: Release");
#else
    response.print("\nBuild type: Debug");
#endif
    response.print("\nBuild date: ");
    response.print(NUKI_HUB_DATE);
    response.print("\nUpdater version: ");
    response.print(_preferences->getString(preference_updater_version, ""));
    response.print("\nUpdater build: ");
    response.print(_preferences->getString(preference_updater_build, ""));
    response.print("\nUpdater build date: ");
    response.print(_preferences->getString(preference_updater_date, ""));
    #if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)
    response.printf("\nHost hosted firmware version: %u.%u.%u", host_version_struct.major1, host_version_struct.minor1, host_version_struct.patch1);
    if(esp_hosted_get_coprocessor_fwversion(&slave_version_struct) == ESP_OK)
    {
        response.printf("\nSlave hosted firmware version: %u.%u.%u", slave_version_struct.major1, slave_version_struct.minor1, slave_version_struct.patch1);
    }
    else
    {
        response.print("\nSlave hosted firmware version: unknown");
    }
    #endif
    response.print("\nUptime (min): ");
    response.print(espMillis() / 1000 / 60);
    response.print("\nConfig version: ");
    response.print(_preferences->getInt(preference_config_version));
    response.print("\nLast restart reason FW: ");
    response.print(getRestartReason());
    response.print("\nLast restart reason ESP: ");
    response.print(getEspRestartReason());
    response.print("\nFree internal heap: ");
    response.print(ESP.getFreeHeap());
    response.print("\nLargest free internal block: ");
    response.print(ESP.getMaxAllocHeap());
    response.print("\nMinimum free internal heap since boot: ");
    response.print(ESP.getMinFreeHeap());
    response.print("\nTotal internal heap: ");
    response.print(ESP.getHeapSize());
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
    if(esp_psram_get_size() > 0)
    {
        response.print("\nPSRAM Available: Yes");
        response.print("\nFree usable PSRAM: ");
        response.print(ESP.getFreePsram());
        response.print("\nTotal usable PSRAM: ");
        response.print(ESP.getPsramSize());
        response.print("\nTotal PSRAM: ");
        response.print(esp_psram_get_size());
        response.print("\nTotal free heap: ");
        response.print(esp_get_free_heap_size());
    }
    else
    {
        response.print("\nPSRAM Available: No");
    }
#else
    response.print("\nPSRAM Available: No");
#endif
    response.print("\nNetwork task stack high watermark: ");
    response.print(uxTaskGetStackHighWaterMark(networkTaskHandle));
    response.print("\nNuki task stack high watermark: ");
    response.print(uxTaskGetStackHighWaterMark(nukiTaskHandle));
    SPIFFS.begin(true);
    response.print("\n\n------------ SPIFFS ------------");
    response.printf("\nSPIFFS Total Bytes: %u", SPIFFS.totalBytes());
    response.printf("\nSPIFFS Used Bytes: %u", SPIFFS.usedBytes());
    response.printf("\nSPIFFS Free Bytes: %u", SPIFFS.totalBytes() - SPIFFS.usedBytes());
    response.print("\n\n------------ GENERAL SETTINGS ------------");
    response.print("\nNetwork task stack size: ");
    response.print(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE));
    response.print("\nNuki task stack size: ");
    response.print(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE));
    response.print("\nCheck for updates: ");
    response.print(_preferences->getBool(preference_check_updates, false) ? "Yes" : "No");
    response.print("\nLatest version: ");
    response.print(_preferences->getString(preference_latest_version, ""));
    response.print("\nAllow update from MQTT: ");
    response.print(_preferences->getBool(preference_update_from_mqtt, false) ? "Yes" : "No");
    response.print("\nUpdate Nuki Hub and Nuki devices time using NTP: ");
    response.print(_preferences->getBool(preference_update_time, false) ? "Yes" : "No");
    response.print("\nNTP Server: ");
    response.print(_preferences->getString(preference_time_server, "pool.ntp.org"));
    response.print("\nWeb configurator username: ");
    response.print(_preferences->getString(preference_cred_user, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator password: ");
    response.print(_preferences->getString(preference_cred_password, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator bypass for proxy IP: ");
    response.print(_preferences->getString(preference_bypass_proxy, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator authentication: ");
    response.print(_preferences->getInt(preference_http_auth_type, 0) == 0 ? "Basic" : _preferences->getInt(preference_http_auth_type, 0) == 1 ? "Digest" : "Form");
    response.print("\nSession validity (in seconds): ");
    response.print(_preferences->getInt(preference_cred_session_lifetime, 3600));
    response.print("\nSession validity remember (in hours): ");
    response.print(_preferences->getInt(preference_cred_session_lifetime_remember, 720));
    response.print("\nDuo Push MFA enabled: ");
    response.print(_preferences->getBool(preference_cred_duo_enabled, false) ? "Yes" : "No");

    if (_preferences->getBool(preference_cred_duo_enabled, false))
    {
        response.print("\nDuo Host: ");
        response.print(_preferences->getString(preference_cred_duo_host, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo IKey: ");
        response.print(_preferences->getString(preference_cred_duo_ikey, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo SKey: ");
        response.print(_preferences->getString(preference_cred_duo_skey, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo User: ");
        response.print(_preferences->getString(preference_cred_duo_user, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo Session validity (in seconds): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_duo, 3600));
        response.print("\nDuo Session validity remember (in hours): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_duo_remember, 720));
    }

    response.print("\nTOTP MFA enabled: ");
    response.print(_importExport->getTOTPEnabled() ? "Yes" : "No");

    if (_importExport->getTOTPEnabled())
    {
        response.print("\nTOTP Session validity (in seconds): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_totp, 3600));
        response.print("\nTOTP Session validity remember (in hours): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_totp_remember, 720));
    }

    response.print("\nWeb configurator enabled: ");
    response.print(_preferences->getBool(preference_webserver_enabled, true) ? "Yes" : "No");
    response.print("\nHTTP SSL: ");
    if (!SPIFFS.begin(true) || !nuki_hub_https_server_enabled)
    {
        response.print("Disabled");
    }
    else
    {
        File file = SPIFFS.open("/http_ssl.crt");
        File file2 = SPIFFS.open("/http_ssl.key");
        response.print((!file || file.isDirectory() || !file2 || file2.isDirectory()) ? "Disabled" : "Enabled");
        file.close();
        file2.close();
        response.print("\nNuki Hub FQDN for HTTP redirect: ");
        response.print(_preferences->getString(preference_https_fqdn, "").length() > 0 ? "***" : "Not set");
    }
    response.print("\nAdvanced menu enabled: ");
    response.print(_preferences->getBool(preference_enable_debug_mode, false) ? "Yes" : "No");
    response.print("\nPublish free heap over MQTT: ");
    response.print(_preferences->getBool(preference_publish_debug_info, false) ? "Yes" : "No");
    response.print("\nNuki connect debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_connect, false) ? "Yes" : "No");
    response.print("\nNuki communication debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_communication, false) ? "Yes" : "No");
    response.print("\nNuki readable data debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_readable_data, false) ? "Yes" : "No");
    response.print("\nNuki hex data debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_hex_data, false) ? "Yes" : "No");
    response.print("\nNuki command debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_command, false) ? "Yes" : "No");
    response.print("\nMQTT log enabled: ");
    response.print(_preferences->getBool(preference_mqtt_log_enabled, false) ? "Yes" : "No");
    response.print("\nWebserial enabled: ");
    response.print(_preferences->getBool(preference_webserial_enabled, false) ? "Yes" : "No");
    response.print("\nBootloop protection enabled: ");
    response.print(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Yes" : "No");
    response.print("\n\n------------ NETWORK ------------");
    response.print("\nNetwork device: ");
    response.print(_network->networkDeviceName());
    response.print("\nNetwork connected: ");
    response.print(_network->isConnected() ? "Yes" : "No");
    if(_network->isConnected())
    {
        response.print("\nInternet connected: ");
        response.print(_network->isInternetConnected() ? "Yes" : "No");

        response.print("\nIP Address: <span class=\"redact\">");
        response.print(_network->localIP());
        response.print("</span>");

        if(_network->networkDeviceName() == "Built-in Wi-Fi")
        {
#ifndef CONFIG_IDF_TARGET_ESP32H2
            response.print("\nSSID: <span class=\"redact\">");
            response.print(WiFi.SSID());
            response.print("</span>");
            response.print("\nMAC address of AP (BSSID): <span class=\"redact\">");
            response.print(_network->networkBSSID());
            response.print("</span>");
            response.print("\nESP32 MAC address: <span class=\"redact\">");
            response.print(WiFi.macAddress());
            response.print("</span>");
#endif
        }
        else
        {
            //Ethernet info
        }
    }
    response.print("\n\n------------ NETWORK SETTINGS ------------");
    response.print("\nNuki Hub hostname: ");
    response.print(_preferences->getString(preference_hostname, ""));
    if(_preferences->getBool(preference_ip_dhcp_enabled, true))
    {
        response.print("\nDHCP enabled: Yes");
    }
    else
    {
        response.print("\nDHCP enabled: No");
        response.print("\nStatic IP address: <span class=\"redact\">");
        response.print(_preferences->getString(preference_ip_address, ""));
        response.print("</span>");
        response.print("\nStatic IP subnet: <span class=\"redact\">");
        response.print(_preferences->getString(preference_ip_subnet, ""));
        response.print("</span>");
        response.print("\nStatic IP gateway: <span class=\"redact\">");
        response.print(_preferences->getString(preference_ip_gateway, ""));
        response.print("</span>");
        response.print("\nStatic IP DNS server: <span class=\"redact\">");
        response.print(_preferences->getString(preference_ip_dns_server, ""));
        response.print("</span>");
    }
    if(_network->networkDeviceName() == "Built-in Wi-Fi")
    {
        response.print("\nRSSI Publish interval (s): ");

        if(_preferences->getInt(preference_rssi_publish_interval, 60) < 0)
        {
            response.print("Disabled");
        }
        else
        {
            response.print(_preferences->getInt(preference_rssi_publish_interval, 60));
        }

        response.print("\nFind WiFi AP with strongest signal: ");
        response.print(_preferences->getBool(preference_find_best_rssi, false) ? "Yes" : "No");
        response.print("\nWi-Fi power save disabled: ");
        response.print(_preferences->getBool(preference_wifi_power_save_disabled, false) ? "Yes" : "No");
        response.print("\nPreferred access point MAC address: <span class=\"redact\">");
        response.print(_preferences->getString(preference_wifi_preferred_bssid, ""));
        response.print("</span>");
    }
    /*
    else if(network->networkDeviceType() == NetworkDeviceType::CUSTOM)
    {

    }
    */
    response.print("\nRestart ESP32 on network disconnect enabled: ");
    response.print(_preferences->getBool(preference_restart_on_disconnect, false) ? "Yes" : "No");
    response.print("\nDisable Network if not connected within 60s: ");
    response.print(_preferences->getBool(preference_disable_network_not_connected, false) ? "Yes" : "No");
    response.print("\nMQTT Timeout until restart (s): ");
    if(_preferences->getInt(preference_network_timeout, 60) < 0)
    {
        response.print("Disabled");
    }
    else
    {
        response.print(_preferences->getInt(preference_network_timeout, 60));
    }
    response.print("\n\n------------ MQTT ------------");
    response.print("\nMQTT connected: ");
    response.print(_network->mqttConnectionState() > 0 ? "Yes" : "No");
    response.print("\nMQTT broker address: <span class=\"redact\">");
    response.print(_preferences->getString(preference_mqtt_broker, ""));
    response.print("</span>");
    response.print("\nMQTT broker port: <span class=\"redact\">");
    response.print(_preferences->getInt(preference_mqtt_broker_port, 1883));
    response.print("</span>");
    response.print("\nMQTT username: ");
    response.print(_preferences->getString(preference_mqtt_user, "").length() > 0 ? "***" : "Not set");
    response.print("\nMQTT password: ");
    response.print(_preferences->getString(preference_mqtt_password, "").length() > 0 ? "***" : "Not set");
    response.print("\nMQTT base topic: ");
    response.print(_preferences->getString(preference_mqtt_lock_path, ""));
    response.print("\nMQTT SSL: ");
    if(_preferences->getBool(preference_mqtt_ssl_enabled, false))
    {
        if (!SPIFFS.begin(true))
        {
            response.print("Disabled");
        }
        else
        {
            File file = SPIFFS.open("/mqtt_ssl.ca");
            if (!file || file.isDirectory())
            {
                response.print("Disabled");
            }
            else
            {
                response.print("Enabled");
                response.print("\nMQTT SSL CA: ***");
                File file2 = SPIFFS.open("/mqtt_ssl.crt");
                File file3 = SPIFFS.open("/mqtt_ssl.key");
                response.print("\nMQTT SSL CRT: ");
                response.print((!file2 || file2.isDirectory()) ? "Not set" : "***");
                response.print("\nMQTT SSL Key: ");
                response.print((!file3 || file3.isDirectory()) ? "Not set" : "***");
                file2.close();
                file3.close();
            }
            file.close();
        }
    }
    else
    {
        response.print("Disabled");
    }
    response.print("\n\n------------ BLUETOOTH ------------");
    response.print("\nBluetooth TX power (dB): ");
    response.print(_preferences->getInt(preference_ble_tx_power, 9));
    response.print("\nBluetooth command nr of retries: ");
    response.print(_preferences->getInt(preference_command_nr_of_retries, 3));
    response.print("\nBluetooth command retry delay (ms): ");
    response.print(_preferences->getInt(preference_command_retry_delay, 100));
    response.print("\nSeconds until reboot when no BLE beacons received: ");
    response.print(_preferences->getInt(preference_restart_ble_beacon_lost, 60));
    response.print("\n\n------------ QUERY / PUBLISH SETTINGS ------------");
    response.print("\nLock/Opener state query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_lockstate, 1800));
    response.print("\nPublish Nuki device authorization log: ");
    response.print(_preferences->getBool(preference_publish_authdata, false) ? "Yes" : "No");
    response.print("\nMax authorization log entries to retrieve: ");
    response.print(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
    response.print("\nBattery state query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_battery, 1800));
    response.print("\nMost non-JSON MQTT topics disabled: ");
    response.print(_preferences->getBool(preference_disable_non_json, false) ? "Yes" : "No");
    response.print("\nPublish Nuki device config: ");
    response.print(_preferences->getBool(preference_conf_info_enabled, false) ? "Yes" : "No");
    response.print("\nConfig query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_configuration, 3600));
    response.print("\nPublish Keypad info: ");
    response.print(_preferences->getBool(preference_keypad_info_enabled, false) ? "Yes" : "No");
    response.print("\nKeypad query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_keypad, 1800));
    response.print("\nEnable Keypad control: ");
    response.print(_preferences->getBool(preference_keypad_control_enabled, false) ? "Yes" : "No");
    response.print("\nPublish Keypad topic per entry: ");
    response.print(_preferences->getBool(preference_keypad_topic_per_entry, false) ? "Yes" : "No");
    response.print("\nPublish Keypad codes: ");
    response.print(_preferences->getBool(preference_keypad_publish_code, false) ? "Yes" : "No");
    response.print("\nAllow checking Keypad codes: ");
    response.print(_preferences->getBool(preference_keypad_check_code_enabled, false) ? "Yes" : "No");
    response.print("\nMax keypad entries to retrieve: ");
    response.print(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
    response.print("\nPublish timecontrol info: ");
    response.print(_preferences->getBool(preference_timecontrol_info_enabled, false) ? "Yes" : "No");
    response.print("\nEnable timecontrol control: ");
    response.print(_preferences->getBool(preference_timecontrol_control_enabled, false) ? "Yes" : "No");
    response.print("\nPublish timecontrol topic per entry: ");
    response.print(_preferences->getBool(preference_timecontrol_topic_per_entry, false) ? "Yes" : "No");
    response.print("\nMax timecontrol entries to retrieve: ");
    response.print(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL));
    response.print("\nEnable authorization control: ");
    response.print(_preferences->getBool(preference_auth_info_enabled, false) ? "Yes" : "No");
    response.print("\nPublish authorization topic per entry: ");
    response.print(_preferences->getBool(preference_auth_topic_per_entry, false) ? "Yes" : "No");
    response.print("\nMax authorization entries to retrieve: ");
    response.print(_preferences->getInt(preference_auth_max_entries, MAX_AUTH));
    response.print("\n\n------------ HOME ASSISTANT ------------");
    response.print("\nHome Assistant auto discovery enabled: ");
    if(_preferences->getBool(preference_mqtt_hass_enabled, false) && _preferences->getString(preference_mqtt_hass_discovery, "").length() > 0)
    {
        response.print("Yes");
        response.print("\nHome Assistant auto discovery topic: ");
        response.print(_preferences->getString(preference_mqtt_hass_discovery, "") + "/");
        response.print("\nNuki Hub configuration URL for HA: <span class=\"redact\">");
        response.print(_preferences->getString(preference_mqtt_hass_cu_url, "").length() > 0 ? _preferences->getString(preference_mqtt_hass_cu_url, "") : "http://" + _network->localIP());
        response.print("</span>");
        response.print("\nNuki Hub ID: <span class=\"redact\">");
        response.print(_preferences->getULong64(preference_nukihub_id, 0));
        response.print("</span>");
    }
    else
    {
        response.print("No");
    }
    response.print("\n\n------------ NUKI LOCK ------------");
    if(_nuki == nullptr || !_preferences->getBool(preference_lock_enabled, true))
    {
        response.print("\nLock enabled: No");
    }
    else
    {
        response.print("\nLock enabled: Yes");
        response.print("\nLock Ultra/Go/5th gen enabled: ");
        response.print(_preferences->getBool(preference_lock_gemini_enabled, false) ? "Yes" : "No");
        response.print("\nPaired: ");
        response.print(_nuki->isPaired() ? "Yes" : "No");
        response.print("\nNuki Hub device ID: ");
        response.print(_preferences->getUInt(preference_device_id_lock, 0));
        response.print("\nNuki device ID: ");
        response.print(_preferences->getUInt(preference_nuki_id_lock, 0) > 0 ? "***" : "Not set");
        response.print("\nFirmware version: ");
        response.print(_nuki->firmwareVersion().c_str());
        response.print("\nHardware version: ");
        response.print(_nuki->hardwareVersion().c_str());
        response.print("\nValid PIN set: ");
        response.print(_nuki->isPaired() ? _nuki->isPinValid() ? "Yes" : "No" : "-");
        response.print("\nHas door sensor: ");
        response.print(_nuki->hasDoorSensor() ? "Yes" : "No");
        response.print("\nHas keypad: ");
        response.print(_nuki->hasKeypad() ? "Yes" : "No");
        if(_nuki->hasKeypad())
        {
            response.print("\nKeypad highest entries count: ");
            response.print(_preferences->getUInt(preference_lock_max_keypad_code_count, 0));
        }
        response.print("\nTimecontrol highest entries count: ");
        response.print(_preferences->getUInt(preference_lock_max_timecontrol_entry_count, 0));
        response.print("\nAuthorizations highest entries count: ");
        response.print(_preferences->getUInt(preference_lock_max_auth_entry_count, 0));
        response.print("\nRegister as: ");
        response.print(_preferences->getBool(preference_register_as_app, false) ? "App" : "Bridge");
        response.print("\nForce Lock ID: ");
        response.print(_preferences->getBool(preference_lock_force_id, false) ? "Yes" : "No");
        response.print("\nForce Lock Keypad: ");
        response.print(_preferences->getBool(preference_lock_force_keypad, false) ? "Yes" : "No");
        response.print("\nForce Lock Doorsensor: ");
        response.print(_preferences->getBool(preference_lock_force_doorsensor, false) ? "Yes" : "No");
        response.print("\n\n------------ HYBRID MODE ------------");
        if(!_preferences->getBool(preference_official_hybrid_enabled, false))
        {
            response.print("\nHybrid mode enabled: No");
        }
        else
        {
            response.print("\nHybrid mode enabled: Yes");
            response.print("\nHybrid mode connected: ");
            response.print(_nuki->offConnected() ? "Yes": "No");
            response.print("\nReboot Nuki lock on official MQTT failure: ");
            response.print(_preferences->getBool(preference_hybrid_reboot_on_disconnect, false) ? "Yes" : "No");
            response.print("\nSending actions through official MQTT enabled: ");
            response.print(_preferences->getBool(preference_official_hybrid_actions, false) ? "Yes" : "No");
            if(_preferences->getBool(preference_official_hybrid_actions, false))
            {
                response.print("\nRetry actions through BLE enabled: ");
                response.print(_preferences->getBool(preference_official_hybrid_retry, false) ? "Yes" : "No");
            }
            response.print("\nTime between status updates when official MQTT is offline (s): ");
            response.print(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600));
        }
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[26];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
        response.print("\n\n------------ NUKI LOCK ACL ------------");
        response.print("\nLock: ");
        response.print((int)aclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nUnlock: ");
        response.print((int)aclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nUnlatch: ");
        response.print((int)aclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nLock N Go: ");
        response.print((int)aclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nLock N Go Unlatch: ");
        response.print((int)aclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nFull Lock: ");
        response.print((int)aclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)aclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)aclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)aclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\n\n------------ NUKI LOCK CONFIG ACL ------------");
        response.print("\nName: ");
        response.print((int)basicLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nLatitude: ");
        response.print((int)basicLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nLongitude: ");
        response.print((int)basicLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nAuto Unlatch: ");
        response.print((int)basicLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nPairing enabled: ");
        response.print((int)basicLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nButton enabled: ");
        response.print((int)basicLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nLED flash enabled: ");
        response.print((int)basicLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nLED brightness: ");
        response.print((int)basicLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nTimezone offset: ");
        response.print((int)basicLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nDST mode: ");
        response.print((int)basicLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)basicLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)basicLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)basicLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nSingle Lock: ");
        response.print((int)basicLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nAdvertising Mode: ");
        response.print((int)basicLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nTimezone ID: ");
        response.print((int)basicLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nUnlocked Position Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nLocked Position Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nSingle Locked Position Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nUnlocked To Locked Transition Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nLock n Go timeout: ");
        response.print((int)advancedLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nSingle button press action: ");
        response.print((int)advancedLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nDouble button press action: ");
        response.print((int)advancedLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nDetached cylinder: ");
        response.print((int)advancedLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nBattery type: ");
        response.print((int)advancedLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nAutomatic battery type detection: ");
        response.print((int)advancedLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nUnlatch duration: ");
        response.print((int)advancedLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nAuto lock timeout: ");
        response.print((int)advancedLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nAuto unlock disabled: ");
        response.print((int)advancedLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nNightmode enabled: ");
        response.print((int)advancedLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nNightmode start time: ");
        response.print((int)advancedLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nNightmode end time: ");
        response.print((int)advancedLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nNightmode auto lock enabled: ");
        response.print((int)advancedLockConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        response.print("\nNightmode auto unlock disabled: ");
        response.print((int)advancedLockConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        response.print("\nNightmode immediate lock on start: ");
        response.print((int)advancedLockConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        response.print("\nAuto lock enabled: ");
        response.print((int)advancedLockConfigAclPrefs[19] ? "Allowed" : "Disallowed");
        response.print("\nImmediate auto lock enabled: ");
        response.print((int)advancedLockConfigAclPrefs[20] ? "Allowed" : "Disallowed");
        response.print("\nAuto update enabled: ");
        response.print((int)advancedLockConfigAclPrefs[21] ? "Allowed" : "Disallowed");
        response.print("\nReboot Nuki: ");
        response.print((int)advancedLockConfigAclPrefs[22] ? "Allowed" : "Disallowed");
        response.print("\nMotor speed: ");
        response.print((int)advancedLockConfigAclPrefs[23] ? "Allowed" : "Disallowed");
        response.print("\nEnable slow speed during nightmode: ");
        response.print((int)advancedLockConfigAclPrefs[24] ? "Allowed" : "Disallowed");
        response.print("\nRecalibrate Nuki: ");
        response.print((int)advancedLockConfigAclPrefs[25] ? "Allowed" : "Disallowed");

        if(_preferences->getBool(preference_show_secrets))
        {
            char tmp[16];
            unsigned char currentBleAddress[6];
            unsigned char authorizationId[4] = {0x00};
            unsigned char secretKeyK[32] = {0x00};
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHub", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddress, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyK, 32);
            nukiBlePref.getBytes("authorizationId", authorizationId, 4);
            nukiBlePref.end();
            response.print("\n\n------------ NUKI LOCK PAIRING ------------");
            response.print("\nBLE Address: <span class=\"redact\">");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddress[i]);
                response.print(tmp);
            }
            response.print("</span>");
            response.print("\nSecretKeyK: <span class=\"redact\">");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyK[i]);
                response.print(tmp);
            }
            response.print("</span>");
            response.print("\nAuthorizationId: <span class=\"redact\">");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationId[i]);
                response.print(tmp);
            }
            uint32_t authorizationIdInt = authorizationId[0] + 256U*authorizationId[1] + 65536U*authorizationId[2] + 16777216U*authorizationId[3];
            response.print("</span>");
            response.print("\nAuthorizationId (UINT32_T): <span class=\"redact\">");
            response.print(authorizationIdInt);
            response.print("</span>");
            response.print("\nPaired to Nuki Lock Ultra/Go/5th gen: ");
            response.print(nukiBlePref.getBool("isUltra", false) ? "Yes" : "No");
        }
    }

    response.print("\n\n------------ NUKI OPENER ------------");
    if(_nukiOpener == nullptr || !_preferences->getBool(preference_opener_enabled, false))
    {
        response.print("\nOpener enabled: No");
    }
    else
    {
        response.print("\nOpener enabled: Yes");
        response.print("\nPaired: ");
        response.print(_nukiOpener->isPaired() ? "Yes" : "No");
        response.print("\nNuki Hub device ID: ");
        response.print(_preferences->getUInt(preference_device_id_opener, 0));
        response.print("\nNuki device ID: ");
        response.print(_preferences->getUInt(preference_nuki_id_opener, 0) > 0 ? "***" : "Not set");
        response.print("\nFirmware version: ");
        response.print(_nukiOpener->firmwareVersion().c_str());
        response.print("\nHardware version: ");
        response.print(_nukiOpener->hardwareVersion().c_str());
        response.print("\nOpener valid PIN set: ");
        response.print(_nukiOpener->isPaired() ? _nukiOpener->isPinValid() ? "Yes" : "No" : "-");
        response.print("\nOpener has keypad: ");
        response.print(_nukiOpener->hasKeypad() ? "Yes" : "No");
        if(_nukiOpener->hasKeypad())
        {
            response.print("\nKeypad highest entries count: ");
            response.print(_preferences->getUInt(preference_opener_max_keypad_code_count, 0));
        }
        response.print("\nTimecontrol highest entries count: ");
        response.print(_preferences->getUInt(preference_opener_max_timecontrol_entry_count, 0));
        response.print("\nAuthorizations highest entries count: ");
        response.print(_preferences->getUInt(preference_opener_max_auth_entry_count, 0));
        response.print("\nRegister as: ");
        response.print(_preferences->getBool(preference_register_opener_as_app, false) ? "App" : "Bridge");
        response.print("\nNuki Opener Lock/Unlock action set to Continuous mode in Home Assistant: ");
        response.print(_preferences->getBool(preference_opener_continuous_mode, false) ? "Yes" : "No");
        response.print("\nForce Opener ID: ");
        response.print(_preferences->getBool(preference_opener_force_id, false) ? "Yes" : "No");
        response.print("\nForce Opener Keypad: ");
        response.print(_preferences->getBool(preference_opener_force_keypad, false) ? "Yes" : "No");
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[22];
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));
        response.print("\n\n------------ NUKI OPENER ACL ------------");
        response.print("\nActivate Ring-to-Open: ");
        response.print((int)aclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nDeactivate Ring-to-Open: ");
        response.print((int)aclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nElectric Strike Actuation: ");
        response.print((int)aclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nActivate Continuous Mode: ");
        response.print((int)aclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nDeactivate Continuous Mode: ");
        response.print((int)aclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)aclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)aclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)aclPrefs[16] ? "Allowed" : "Disallowed");
        response.print("\n\n------------ NUKI OPENER CONFIG ACL ------------");
        response.print("\nName: ");
        response.print((int)basicOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nLatitude: ");
        response.print((int)basicOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nLongitude: ");
        response.print((int)basicOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nPairing enabled: ");
        response.print((int)basicOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nButton enabled: ");
        response.print((int)basicOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nLED flash enabled: ");
        response.print((int)basicOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nTimezone offset: ");
        response.print((int)basicOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nDST mode: ");
        response.print((int)basicOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)basicOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)basicOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)basicOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nOperating Mode: ");
        response.print((int)basicOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nAdvertising Mode: ");
        response.print((int)basicOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nTimezone ID: ");
        response.print((int)basicOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nIntercom ID: ");
        response.print((int)advancedOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nBUS mode Switch: ");
        response.print((int)advancedOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nShort Circuit Duration: ");
        response.print((int)advancedOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nEletric Strike Delay: ");
        response.print((int)advancedOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nRandom Electric Strike Delay: ");
        response.print((int)advancedOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nElectric Strike Duration: ");
        response.print((int)advancedOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nDisable RTO after ring: ");
        response.print((int)advancedOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nRTO timeout: ");
        response.print((int)advancedOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nDoorbell suppression: ");
        response.print((int)advancedOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nDoorbell suppression duration: ");
        response.print((int)advancedOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nSound Ring: ");
        response.print((int)advancedOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nSound Open: ");
        response.print((int)advancedOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nSound RTO: ");
        response.print((int)advancedOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nSound CM: ");
        response.print((int)advancedOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nSound confirmation: ");
        response.print((int)advancedOpenerConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nSound level: ");
        response.print((int)advancedOpenerConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nSingle button press action: ");
        response.print((int)advancedOpenerConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        response.print("\nDouble button press action: ");
        response.print((int)advancedOpenerConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        response.print("\nBattery type: ");
        response.print((int)advancedOpenerConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        response.print("\nAutomatic battery type detection: ");
        response.print((int)advancedOpenerConfigAclPrefs[19] ? "Allowed" : "Disallowed");
        response.print("\nReboot Nuki: ");
        response.print((int)advancedOpenerConfigAclPrefs[20] ? "Allowed" : "Disallowed");
        response.print("\nRecalibrate Nuki: ");
        response.print((int)advancedOpenerConfigAclPrefs[21] ? "Allowed" : "Disallowed");
        if(_preferences->getBool(preference_show_secrets))
        {
            char tmp[16];
            unsigned char currentBleAddressOpn[6];
            unsigned char authorizationIdOpn[4] = {0x00};
            unsigned char secretKeyKOpn[32] = {0x00};
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHubopener", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddressOpn, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyKOpn, 32);
            nukiBlePref.getBytes("authorizationId", authorizationIdOpn, 4);
            nukiBlePref.end();
            response.print("\n\n------------ NUKI OPENER PAIRING ------------");
            response.print("\nBLE Address: <span class=\"redact\">");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddressOpn[i]);
                response.print(tmp);
            }
            response.print("</span>");
            response.print("\nSecretKeyK: <span class=\"redact\">");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyKOpn[i]);
                response.print(tmp);
            }
            response.print("</span>");
            response.print("\nAuthorizationId: <span class=\"redact\">");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationIdOpn[i]);
                response.print(tmp);
            }
            uint32_t authorizationIdOpnInt = authorizationIdOpn[0] + 256U*authorizationIdOpn[1] + 65536U*authorizationIdOpn[2] + 16777216U*authorizationIdOpn[3];
            response.print("</span>");
            response.print("\nAuthorizationId (UINT32_T): <span class=\"redact\">");
            response.print(authorizationIdOpnInt);
            response.print("</span>");
        }
    }

    response.print("\n\n------------ GPIO ------------\n");
    response.print("\nRetain Input GPIO MQTT state: ");
    response.print(_preferences->getBool(preference_retain_gpio, false) ? "Yes\n" : "No\n");
    String gpioStr = "";
    _gpio->getConfigurationText(gpioStr, _gpio->pinConfiguration());
    response.print(gpioStr);
    response.print("</pre>");
    buildHtmlFooter(&response);
    return response.endSend();
}

esp_err_t WebCfgServer::buildStatusHtml(PsychicRequest *request, PsychicResponse* resp)
{
    JsonDocument json;
    String jsonStr;
    bool mqttDone = false;
    bool lockDone = false;
    bool openerDone = false;

    json["stop"] = 0;

    if(_network->mqttConnectionState() > 0)
    {
        json["mqttState"] = "Yes";
        mqttDone = true;
    }
    else
    {
        json["mqttState"] = "No";
    }

    if(_nuki != nullptr)
    {
        char lockStateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockStateArr);
        String lockState = lockStateArr;
        String LockPaired = (_nuki->isPaired() ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str() : "No");
        json["lockPaired"] = LockPaired;
        json["lockState"] = lockState;

        if(_nuki->isPaired())
        {
            json["lockPin"] = pinStateToString((NukiPinState)_preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured));
            if(strcmp(lockStateArr, "undefined") != 0)
            {
                lockDone = true;
            }
        }
        else
        {
            json["lockPin"] = "Not Paired";
        }
    }
    else
    {
        lockDone = true;
    }
    if(_nukiOpener != nullptr)
    {
        char openerStateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, openerStateArr);
        String openerState = openerStateArr;
        String openerPaired = (_nukiOpener->isPaired() ? ("Yes (BLE Address " + _nukiOpener->getBleAddress().toString() + ")").c_str() : "No");
        json["openerPaired"] = openerPaired;

        if(_nukiOpener->keyTurnerState().nukiState == NukiOpener::State::ContinuousMode)
        {
            json["openerState"] = "Open (Continuous Mode)";
        }
        else
        {
            json["openerState"] = openerState;
        }

        if(_nukiOpener->isPaired())
        {
            json["openerPin"] = pinStateToString((NukiPinState)_preferences->getInt(preference_opener_pin_status, (int)NukiPinState::NotConfigured));
            if(strcmp(openerStateArr, "undefined") != 0)
            {
                openerDone = true;
            }
        }
        else
        {
            json["openerPin"] = "Not Paired";
        }
    }
    else
    {
        openerDone = true;
    }

    if(_preferences->getBool(preference_check_updates))
    {
        json["latestFirmware"] = _preferences->getString(preference_latest_version);
    }

    if(mqttDone && lockDone && openerDone)
    {
        json["stop"] = 1;
    }

    serializeJson(json, jsonStr);
    resp->setCode(200);
    resp->setContentType("application/json");
    resp->setContent(jsonStr.c_str());
    return resp->send();
}

#endif
