#pragma once

#include <vector>
#include "Config.h"
#include "Logger.h"
#include "FS.h"
#include "SPIFFS.h"
#include "RestartReason.h"

#ifndef CONFIG_IDF_TARGET_ESP32H2
#include <WiFi.h>
#endif

//CHANGE REQUIRES REBOOT TO TAKE EFFECT
#define preference_ip_dhcp_enabled (char*)"dhcpena"
#define preference_ip_address (char*)"ipaddr"
#define preference_ip_subnet (char*)"ipsub"
#define preference_ip_gateway (char*)"ipgtw"
#define preference_ip_dns_server (char*)"dnssrv"
#define preference_mqtt_broker (char*)"mqttbroker"
#define preference_mqtt_broker_port (char*)"mqttport"
#define preference_mqtt_user (char*)"mqttuser"
#define preference_mqtt_password (char*)"mqttpass"
#define preference_mqtt_log_enabled (char*)"mqttlog"
#define preference_webserial_enabled (char*)"weblog"
#define preference_lock_enabled (char*)"lockena"
#define preference_mqtt_lock_path (char*)"mqttpath"
#define preference_opener_enabled (char*)"openerena"
#define preference_mqtt_ca (char*)"mqttca"
#define preference_mqtt_crt (char*)"mqttcrt"
#define preference_mqtt_key (char*)"mqttkey"
#define preference_network_hardware (char*)"nwhw"
#define preference_hostname (char*)"hostname"
#define preference_network_custom_phy (char*)"ntwPHY"
#define preference_network_custom_addr (char*)"ntwADDR"
#define preference_network_custom_irq (char*)"ntwIRQ"
#define preference_network_custom_rst (char*)"ntwRST"
#define preference_network_custom_cs (char*)"ntwCS"
#define preference_network_custom_sck (char*)"ntwSCK"
#define preference_network_custom_miso (char*)"ntwMISO"
#define preference_network_custom_mosi (char*)"ntwMOSI"
#define preference_network_custom_pwr (char*)"ntwPWR"
#define preference_network_custom_mdio (char*)"ntwMDIO"
#define preference_network_custom_mdc (char*)"ntwMDC"
#define preference_network_custom_clk (char*)"ntwCLK"
#define preference_auth_control_enabled (char*)"authCtrlEna"
#define preference_keypad_control_enabled (char*)"kpCntrlEnabled"
#define preference_timecontrol_control_enabled (char*)"tcCntrlEnabled"
#define preference_ota_main_url (char*)"otaMainUrl"
#define preference_ota_updater_url (char*)"otaUpdUrl"
#define preference_task_size_network (char*)"tsksznetw"
#define preference_task_size_nuki (char*)"tsksznuki"
#define preference_buffer_size (char*)"buffsize"
#define preference_cred_user (char*)"crdusr"
#define preference_cred_password (char*)"crdpass"
#define preference_gpio_configuration (char*)"gpiocfg"
#define preference_mqtt_hass_enabled (char*)"hassena"
#define preference_mqtt_hass_discovery (char*)"hassdiscovery"
#define preference_hass_device_discovery (char*)"hassdevdisc"
#define preference_webserver_enabled (char*)"websrvena"
#define preference_update_from_mqtt (char*)"updMqtt"
#define preference_disable_non_json (char*)"disnonjson"
#define preference_official_hybrid_enabled (char*)"offHybrid"
#define preference_wifi_ssid (char*)"wifiSSID"
#define preference_wifi_pass (char*)"wifiPass"
#define preference_disable_network_not_connected (char*)"disNtwNoCon"
#define preference_debug_connect (char*)"dbgConnect"
#define preference_debug_communication (char*)"dbgCommu"
#define preference_debug_readable_data (char*)"dbgReadData"
#define preference_debug_hex_data (char*)"dbgHexData"
#define preference_debug_command (char*)"dbgCommand"
#define preference_connect_mode (char*)"nukiConnMode"
#define preference_http_auth_type (char*)"httpdAuthType"
#define preference_update_time (char*)"updateTime"
#define preference_time_server (char*)"timeServer"
#define preference_mqtt_ssl_enabled (char*)"mqttSSLena"
#define preference_lock_gemini_pin (char*)"geminiPin"
#define preference_lock_gemini_enabled (char*)"geminiena"
#define preference_cred_duo_enabled (char*)"duoena"
#define preference_cred_duo_host (char*)"duoHost"
#define preference_cred_duo_ikey (char*)"duoIkey"
#define preference_cred_duo_skey (char*)"duoSkey"
#define preference_cred_duo_user (char*)"duoUser"
#define preference_https_fqdn (char*)"httpsFQDN"
#define preference_bypass_proxy (char*)"credBypass"
#define preference_cred_session_lifetime (char*)"credLf"
#define preference_cred_session_lifetime_remember (char*)"credLfRmbr"
#define preference_cred_session_lifetime_duo (char*)"credLfDuo"
#define preference_cred_session_lifetime_duo_remember (char*)"credLfDuoRmbr"
#define preference_cred_session_lifetime_totp (char*)"credLfTotp"
#define preference_cred_session_lifetime_totp_remember (char*)"credLfTotpRmbr"
#define preference_cred_duo_approval (char*)"duoApprove"
#define preference_cred_bypass_boot_btn_enabled (char*)"bypassBtBtn"
#define preference_cred_bypass_gpio_high (char*)"bypassHigh"
#define preference_cred_bypass_gpio_low (char*)"bypassLow"
#define preference_publish_config (char*)"nhPubConfig"
#define preference_config_from_mqtt (char*)"nhCntrlEnabled"
#define preference_totp_secret (char*)"totpsecret"

// CHANGE DOES NOT REQUIRE REBOOT TO TAKE EFFECT
#define preference_find_best_rssi (char*)"nwbestrssi"
#define preference_auth_max_entries (char*)"authmaxentry"
#define preference_auth_info_enabled (char*)"authInfoEna"
#define preference_auth_topic_per_entry (char*)"authPerEntry"
#define preference_authlog_max_entries (char*)"authmaxentry"
#define preference_keypad_max_entries (char*)"kpmaxentry"
#define preference_timecontrol_max_entries (char*)"tcmaxentry"
#define preference_register_as_app (char*)"regAsApp"
#define preference_register_opener_as_app (char*)"regOpnAsApp"
#define preference_acl (char*)"aclLckOpn"
#define preference_conf_lock_basic_acl (char*)"confLckBasAcl"
#define preference_conf_lock_advanced_acl (char*)"confLckAdvAcl"
#define preference_conf_opener_basic_acl (char*)"confOpnBasAcl"
#define preference_conf_opener_advanced_acl (char*)"confOpnAdvAcl"
#define preference_ble_tx_power (char*)"bleTxPwr"
#define preference_show_secrets (char*)"showSecr"
#define preference_enable_bootloop_reset (char*)"enabtlprst"
#define preference_keypad_info_enabled (char*)"kpInfoEnabled"
#define preference_keypad_topic_per_entry (char*)"kpPerEntry"
#define preference_keypad_publish_code (char*)"kpPubCode"
#define preference_timecontrol_topic_per_entry (char*)"tcPerEntry"
#define preference_timecontrol_info_enabled (char*)"tcInfoEnabled"
#define preference_publish_authdata (char*)"pubAuth"
#define preference_conf_info_enabled (char*)"cnfInfoEnabled"
#define preference_restart_ble_beacon_lost (char*)"rstbcn"
#define preference_query_interval_lockstate (char*)"lockStInterval"
#define preference_query_interval_configuration (char*)"configInterval"
#define preference_query_interval_battery (char*)"batInterval"
#define preference_query_interval_keypad (char*)"kpInterval"
#define preference_command_nr_of_retries (char*)"nrRetry"
#define preference_command_retry_delay (char*)"rtryDelay"
#define preference_query_interval_hybrid_lockstate (char*)"hybridTimer"
#define preference_mqtt_hass_cu_url (char*)"hassConfigUrl"
#define preference_check_updates (char*)"checkupdates"
#define preference_opener_continuous_mode (char*)"openercont"
#define preference_rssi_publish_interval (char*)"rssipb"
#define preference_network_timeout (char*)"nettmout"
#define preference_restart_on_disconnect (char*)"restdisc"
#define preference_publish_debug_info (char*)"pubdbg"
#define preference_enable_debug_mode (char*)"enadbg"
#define preference_official_hybrid_actions (char*)"hybridAct"
#define preference_official_hybrid_retry (char*)"hybridRtry"
#define preference_keypad_check_code_enabled (char*)"kpChkEna"
#define preference_retain_gpio (char*)"retGpio"
#define preference_lock_force_id (char*)"lckForceId"
#define preference_lock_force_doorsensor (char*)"lckForceDrsns"
#define preference_lock_force_keypad (char*)"lckForceKp"
#define preference_opener_force_id (char*)"opForceId"
#define preference_opener_force_keypad (char*)"opForceKp"
#define preference_hybrid_reboot_on_disconnect (char*)"hybridRbtLck"

//NOT USER CHANGABLE
#define preference_mfa_reconfigure (char*)"mfaRECONF"
#define preference_ntw_reconfigure (char*)"ntwRECONF"
#define preference_updater_version (char*)"updVer"
#define preference_updater_build (char*)"updBuild"
#define preference_updater_date (char*)"updDate"
#define preference_lock_max_auth_entry_count (char*)"maxauth"
#define preference_opener_max_auth_entry_count (char*)"opmaxauth"
#define preference_started_before (char*)"run"
#define preference_config_version (char*)"confVersion"
#define preference_device_id_lock (char*)"deviceId"
#define preference_device_id_opener (char*)"deviceIdOp"
#define preference_nuki_id_lock (char*)"nukiId"
#define preference_nuki_id_opener (char*)"nukidOp"
#define preference_lock_pin_status (char*)"lockpin"
#define preference_opener_pin_status (char*)"openerpin"
#define preference_lock_max_keypad_code_count (char*)"maxkpad"
#define preference_opener_max_keypad_code_count (char*)"opmaxkpad"
#define preference_lock_max_timecontrol_entry_count (char*)"maxtc"
#define preference_opener_max_timecontrol_entry_count (char*)"opmaxtc"
#define preference_latest_version (char*)"latest"
#define preference_reset_mqtt_topics (char*)"rstMqtt"
#define preference_nukihub_id (char*)"nukihubId"

//OBSOLETE
#define preference_access_level (char*)"accLvl"
#define preference_mqtt_opener_path (char*)"mqttoppath"

inline void initPreferences(Preferences* preferences)
{
    #ifdef NUKI_HUB_UPDATER
    return;
    #else
    bool firstStart = !preferences->getBool(preference_started_before);

    if(firstStart)
    {
        Serial.println("First start, setting preference defaults");

        preferences->putBool(preference_started_before, true);
        preferences->putBool(preference_lock_enabled, true);
        uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
        uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
        uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));

        preferences->putString(preference_mqtt_lock_path, "nukihub");
        preferences->putString(preference_time_server, "pool.ntp.org");
        preferences->putString(preference_cred_duo_host, "");
        preferences->putString(preference_cred_duo_ikey, "");
        preferences->putString(preference_cred_duo_skey, "");
        preferences->putString(preference_cred_duo_user, "");
        preferences->putString(preference_https_fqdn, "");
        preferences->putString(preference_bypass_proxy, "");

        preferences->putBool(preference_check_updates, true);
        preferences->putBool(preference_opener_continuous_mode, false);
        preferences->putBool(preference_official_hybrid_enabled, false);
        preferences->putBool(preference_official_hybrid_actions, false);
        preferences->putBool(preference_official_hybrid_retry, false);
        preferences->putBool(preference_hybrid_reboot_on_disconnect, false);
        preferences->putBool(preference_disable_non_json, false);
        preferences->putBool(preference_update_from_mqtt, false);
        preferences->putBool(preference_ip_dhcp_enabled, true);
        preferences->putBool(preference_enable_bootloop_reset, false);
        preferences->putBool(preference_show_secrets, false);
        preferences->putBool(preference_find_best_rssi, true);
        preferences->putBool(preference_conf_info_enabled, true);
        preferences->putBool(preference_keypad_info_enabled, false);
        preferences->putBool(preference_keypad_topic_per_entry, false);
        preferences->putBool(preference_keypad_publish_code, false);
        preferences->putBool(preference_keypad_control_enabled, false);
        preferences->putBool(preference_timecontrol_info_enabled, false);
        preferences->putBool(preference_timecontrol_topic_per_entry, false);
        preferences->putBool(preference_timecontrol_control_enabled, false);
        preferences->putBool(preference_publish_authdata, false);
        preferences->putBool(preference_register_as_app, false);
        preferences->putBool(preference_register_opener_as_app, false);
        preferences->putBool(preference_mqtt_ssl_enabled, false);
        preferences->putBool(preference_lock_gemini_enabled, false);
        preferences->putBool(preference_debug_connect, false);
        preferences->putBool(preference_debug_communication, false);
        preferences->putBool(preference_debug_readable_data, false);
        preferences->putBool(preference_debug_hex_data, false);
        preferences->putBool(preference_debug_command, false);
        preferences->putBool(preference_connect_mode, true);
        preferences->putBool(preference_retain_gpio, false);
        preferences->putBool(preference_enable_debug_mode, false);
        preferences->putBool(preference_cred_duo_enabled, false);
        preferences->putBool(preference_cred_duo_approval, false);
        preferences->putBool(preference_cred_bypass_boot_btn_enabled, false);
        preferences->putBool(preference_publish_config, false);
        preferences->putBool(preference_config_from_mqtt, false);

        preferences->putInt(preference_mqtt_broker_port, 1883);
        preferences->putInt(preference_buffer_size, CHAR_BUFFER_SIZE);
        preferences->putInt(preference_task_size_network, NETWORK_TASK_SIZE);
        preferences->putInt(preference_task_size_nuki, NUKI_TASK_SIZE);
        preferences->putInt(preference_authlog_max_entries, MAX_AUTHLOG);
        preferences->putInt(preference_keypad_max_entries, MAX_KEYPAD);
        preferences->putInt(preference_timecontrol_max_entries, MAX_TIMECONTROL);
        preferences->putInt(preference_query_interval_hybrid_lockstate, 600);
        preferences->putInt(preference_rssi_publish_interval, 60);
        preferences->putInt(preference_network_timeout, 60);
        preferences->putInt(preference_command_nr_of_retries, 3);
        preferences->putInt(preference_command_retry_delay, 100);
        preferences->putInt(preference_restart_ble_beacon_lost, 60);
        preferences->putInt(preference_query_interval_lockstate, 1800);
        preferences->putInt(preference_query_interval_configuration, 3600);
        preferences->putInt(preference_query_interval_battery, 1800);
        preferences->putInt(preference_query_interval_keypad, 1800);
        preferences->putInt(preference_http_auth_type, 0);
        preferences->putInt(preference_cred_session_lifetime, 3600);
        preferences->putInt(preference_cred_session_lifetime_remember, 720);
        preferences->putInt(preference_cred_session_lifetime_duo, 3600);
        preferences->putInt(preference_cred_session_lifetime_duo_remember, 720);
        preferences->putInt(preference_cred_session_lifetime_totp, 3600);
        preferences->putInt(preference_cred_session_lifetime_totp_remember, 720);
        preferences->putInt(preference_cred_bypass_gpio_high, -1);
        preferences->putInt(preference_cred_bypass_gpio_low, -1); 

#ifndef CONFIG_IDF_TARGET_ESP32H2
        WiFi.begin();
        WiFi.disconnect(true, true);
#endif
    }
    else
    {
        int lastConfigVer = preferences->getInt(preference_config_version);
        bool rebootOnMigrate = false;

        Log->print("Last config version: ");
        Log->println(lastConfigVer);
        Log->print("Current config version: ");
        Log->println(NUKI_HUB_VERSION_INT);

        if(lastConfigVer >= NUKI_HUB_VERSION_INT && lastConfigVer < 20000) return;

        if (lastConfigVer < 834)
        {
            Log->println("Migration 8.34");

            if(preferences->getInt(preference_keypad_control_enabled))
            {
                preferences->putBool(preference_keypad_info_enabled, true);
            }
            else
            {
                preferences->putBool(preference_keypad_info_enabled, false);
            }

            switch(preferences->getInt(preference_access_level, 10))
            {
                case 0:
                {
                    preferences->putBool(preference_keypad_control_enabled, true);
                    uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                    preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
                    preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                    uint32_t advancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                    uint32_t advancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                    break;
                }
                case 1:
                {
                    preferences->putBool(preference_keypad_control_enabled, false);
                    uint32_t aclPrefs[17] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0};
                    preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                    uint32_t advancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                    uint32_t advancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                    break;
                }
                case 2:
                {
                    preferences->putBool(preference_keypad_control_enabled, false);
                    uint32_t aclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                    uint32_t advancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                    uint32_t advancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                    break;
                }
                case 3:
                {
                    preferences->putBool(preference_keypad_control_enabled, false);
                    uint32_t aclPrefs[17] = {1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0};
                    preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                    uint32_t advancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                    uint32_t advancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                    break;
                }
                default:
                {
                    preferences->putBool(preference_keypad_control_enabled, true);
                    uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                    preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                    uint32_t basicLockConfigAclPrefs[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                    preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                    uint32_t basicOpenerConfigAclPrefs[14] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                    preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                    uint32_t advancedLockConfigAclPrefs[25] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                    preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                    uint32_t advancedOpenerConfigAclPrefs[21] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                    preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                    break;
                }
            }
        }
        if (lastConfigVer < 901)
        {
            Log->println("Migration 9.01");

            #if defined(CONFIG_IDF_TARGET_ESP32S3)
            if (preferences->getInt(preference_network_hardware) == 3)
            {
                preferences->putInt(preference_network_hardware, 10);
            }
            #endif
            if (preferences->getInt(preference_network_hardware) == 2)
            {
                preferences->putInt(preference_network_hardware, 3);
            }

        }
        if (lastConfigVer < 902)
        {
            Log->println("Migration 9.02");
            preferences->putBool(preference_reset_mqtt_topics, true);
        }
        if (lastConfigVer < 907)
        {
            Log->println("Migration 9.07");

            char ca[2200] = {0};
            char cert[2200] = {0};
            char key[2200] = {0};

            size_t caLength = preferences->getString(preference_mqtt_ca, ca, 2200);
            size_t crtLength = preferences->getString(preference_mqtt_crt, cert, 2200);
            size_t keyLength = preferences->getString(preference_mqtt_key, key, 2200);

            if (caLength > 1)
            {
                if (!SPIFFS.begin(true)) {
                    Log->println("SPIFFS Mount Failed");
                }
                else
                {
                    File file = SPIFFS.open("/mqtt_ssl.ca", FILE_WRITE);
                    if (!file) {
                        Log->println("Failed to open /mqtt_ssl.ca for writing");
                    }
                    else
                    {
                        if (!file.print(ca))
                        {
                            Log->println("Failed to write /mqtt_ssl.ca");
                        }
                        file.close();
                    }
                }

                rebootOnMigrate = true;
                preferences->putBool(preference_mqtt_ssl_enabled, true);
                preferences->putString(preference_mqtt_ca, "");
            }

            if (crtLength > 1)
            {
                if (!SPIFFS.begin(true)) {
                    Log->println("SPIFFS Mount Failed");
                }
                else
                {
                    File file = SPIFFS.open("/mqtt_ssl.crt", FILE_WRITE);
                    if (!file) {
                        Log->println("Failed to open /mqtt_ssl.crt for writing");
                    }
                    else
                    {
                        if (!file.print(cert))
                        {
                            Log->println("Failed to write /mqtt_ssl.crt");
                        }
                        file.close();
                    }
                }

                rebootOnMigrate = true;
                preferences->putString(preference_mqtt_crt, "");
            }

            if (keyLength > 1)
            {
                if (!SPIFFS.begin(true)) {
                    Log->println("SPIFFS Mount Failed");
                }
                else
                {
                    File file = SPIFFS.open("/mqtt_ssl.key", FILE_WRITE);
                    if (!file) {
                        Log->println("Failed to open /mqtt_ssl.key for writing");
                    }
                    else
                    {
                        if (!file.print(key))
                        {
                            Log->println("Failed to write /mqtt_ssl.key");
                        }
                        file.close();
                    }
                }

                rebootOnMigrate = true;
                preferences->putString(preference_mqtt_key, "");
            }
        }
        preferences->putInt(preference_config_version, NUKI_HUB_VERSION_INT);

        if (rebootOnMigrate)
        {
            restartEsp(RestartReason::OTACompleted);
        }
    }
    #endif
}

class DebugPreferences
{
private:
    std::vector<char*> _keys =
    {
        preference_started_before, preference_config_version, preference_device_id_lock, preference_device_id_opener, preference_nuki_id_lock, preference_nuki_id_opener,
        preference_mqtt_broker, preference_mqtt_broker_port, preference_mqtt_user, preference_mqtt_password, preference_mqtt_log_enabled, preference_check_updates,
        preference_webserver_enabled, preference_lock_enabled, preference_lock_pin_status, preference_mqtt_lock_path, preference_opener_enabled, preference_opener_pin_status,
        preference_opener_continuous_mode, preference_lock_max_keypad_code_count, preference_opener_max_keypad_code_count, preference_update_time, preference_time_server,
        preference_lock_max_timecontrol_entry_count, preference_opener_max_timecontrol_entry_count, preference_enable_bootloop_reset,
        preference_mqtt_hass_discovery, preference_mqtt_hass_cu_url, preference_buffer_size, preference_ip_dhcp_enabled, preference_ip_address,
        preference_ip_subnet, preference_ip_gateway, preference_ip_dns_server, preference_network_hardware, preference_http_auth_type, preference_lock_gemini_pin,
        preference_rssi_publish_interval, preference_hostname, preference_network_timeout, preference_restart_on_disconnect, preference_hybrid_reboot_on_disconnect,
        preference_restart_ble_beacon_lost, preference_query_interval_lockstate, preference_timecontrol_topic_per_entry, preference_keypad_topic_per_entry,
        preference_query_interval_configuration, preference_query_interval_battery, preference_query_interval_keypad, preference_keypad_control_enabled,
        preference_keypad_info_enabled, preference_keypad_publish_code, preference_timecontrol_control_enabled, preference_timecontrol_info_enabled, preference_conf_info_enabled,
        preference_register_as_app, preference_register_opener_as_app, preference_command_nr_of_retries, preference_command_retry_delay, preference_cred_user,
        preference_cred_password, preference_disable_non_json, preference_publish_authdata, preference_publish_debug_info, preference_mqtt_ssl_enabled, preference_enable_debug_mode,
        preference_official_hybrid_enabled, preference_query_interval_hybrid_lockstate, preference_official_hybrid_actions, preference_official_hybrid_retry,
        preference_task_size_network, preference_task_size_nuki, preference_authlog_max_entries, preference_keypad_max_entries, preference_timecontrol_max_entries,
        preference_update_from_mqtt, preference_show_secrets, preference_ble_tx_power, preference_webserial_enabled, preference_find_best_rssi, preference_lock_gemini_enabled,
        preference_network_custom_mdc, preference_network_custom_clk, preference_network_custom_phy, preference_network_custom_addr, preference_network_custom_irq,
        preference_network_custom_rst, preference_network_custom_cs, preference_network_custom_sck, preference_network_custom_miso, preference_network_custom_mosi,
        preference_network_custom_pwr, preference_network_custom_mdio, preference_lock_max_auth_entry_count, preference_opener_max_auth_entry_count,
        preference_auth_control_enabled, preference_auth_topic_per_entry, preference_auth_info_enabled, preference_auth_max_entries, preference_wifi_ssid, preference_wifi_pass,
        preference_keypad_check_code_enabled, preference_disable_network_not_connected, preference_mqtt_hass_enabled, preference_hass_device_discovery, preference_retain_gpio,
        preference_debug_connect, preference_debug_communication, preference_debug_readable_data, preference_debug_hex_data, preference_debug_command, preference_connect_mode,
        preference_lock_force_id, preference_lock_force_doorsensor, preference_lock_force_keypad, preference_opener_force_id, preference_opener_force_keypad, preference_nukihub_id,
        preference_cred_duo_host, preference_cred_duo_ikey, preference_cred_duo_skey, preference_cred_duo_user, preference_cred_duo_enabled, preference_https_fqdn, preference_bypass_proxy,
        preference_cred_session_lifetime, preference_cred_session_lifetime_remember, preference_cred_session_lifetime_duo, preference_cred_session_lifetime_duo_remember,
        preference_cred_duo_approval, preference_cred_bypass_boot_btn_enabled, preference_cred_bypass_gpio_high, preference_cred_bypass_gpio_low, preference_publish_config,
        preference_config_from_mqtt, preference_totp_secret, preference_cred_session_lifetime_totp, preference_cred_session_lifetime_totp_remember
    };
    std::vector<char*> _redact =
    {
        preference_mqtt_user, preference_mqtt_password, preference_cred_user, preference_cred_password, preference_nuki_id_lock, preference_nuki_id_opener, preference_wifi_pass,
        preference_lock_gemini_pin, preference_cred_duo_host, preference_cred_duo_ikey, preference_cred_duo_skey, preference_cred_duo_user, preference_bypass_proxy,
        preference_totp_secret
    };
    std::vector<char*> _boolPrefs =
    {
        preference_started_before, preference_mqtt_log_enabled, preference_check_updates, preference_lock_enabled, preference_opener_enabled, preference_opener_continuous_mode,
        preference_timecontrol_topic_per_entry, preference_keypad_topic_per_entry, preference_enable_bootloop_reset, preference_webserver_enabled, preference_update_time,
        preference_restart_on_disconnect, preference_keypad_control_enabled, preference_keypad_info_enabled, preference_keypad_publish_code, preference_show_secrets,
        preference_timecontrol_control_enabled, preference_timecontrol_info_enabled, preference_register_as_app, preference_register_opener_as_app, preference_ip_dhcp_enabled,
        preference_publish_authdata, preference_publish_debug_info, preference_official_hybrid_enabled, preference_mqtt_hass_enabled, preference_retain_gpio,
        preference_official_hybrid_actions, preference_official_hybrid_retry, preference_conf_info_enabled, preference_disable_non_json, preference_update_from_mqtt,
        preference_auth_control_enabled, preference_auth_topic_per_entry, preference_auth_info_enabled, preference_webserial_enabled, preference_hass_device_discovery,
        preference_keypad_check_code_enabled, preference_disable_network_not_connected, preference_find_best_rssi, preference_cred_bypass_boot_btn_enabled,
        preference_debug_connect, preference_debug_communication, preference_debug_readable_data, preference_debug_hex_data, preference_debug_command, preference_connect_mode,
        preference_lock_force_id, preference_lock_force_doorsensor, preference_lock_force_keypad, preference_opener_force_id, preference_opener_force_keypad, preference_mqtt_ssl_enabled,
        preference_hybrid_reboot_on_disconnect, preference_lock_gemini_enabled, preference_enable_debug_mode, preference_cred_duo_enabled, preference_cred_duo_approval, 
        preference_publish_config, preference_config_from_mqtt
    };
    std::vector<char*> _bytePrefs =
    {
        preference_acl, preference_conf_info_enabled, preference_conf_lock_basic_acl, preference_conf_lock_advanced_acl, preference_conf_opener_basic_acl,
        preference_conf_opener_advanced_acl, preference_gpio_configuration
    };
    std::vector<char*> _intPrefs =
    {
        preference_config_version, preference_mqtt_broker_port, preference_command_retry_delay, preference_query_interval_hybrid_lockstate,
        preference_lock_pin_status, preference_opener_pin_status, preference_lock_max_keypad_code_count, preference_opener_max_keypad_code_count,
        preference_lock_max_timecontrol_entry_count, preference_opener_max_timecontrol_entry_count, preference_buffer_size, preference_network_hardware,
        preference_rssi_publish_interval, preference_network_timeout, preference_restart_ble_beacon_lost, preference_query_interval_lockstate,
        preference_query_interval_configuration, preference_query_interval_battery, preference_query_interval_keypad, preference_command_nr_of_retries,
        preference_task_size_network, preference_task_size_nuki, preference_authlog_max_entries, preference_keypad_max_entries, preference_timecontrol_max_entries,
        preference_ble_tx_power, preference_network_custom_mdc, preference_network_custom_clk, preference_network_custom_phy, preference_network_custom_addr,
        preference_network_custom_irq, preference_network_custom_rst, preference_network_custom_cs, preference_network_custom_sck, preference_network_custom_miso,
        preference_network_custom_mosi, preference_network_custom_pwr, preference_network_custom_mdio, preference_http_auth_type,
        preference_cred_session_lifetime, preference_cred_session_lifetime_remember, preference_cred_session_lifetime_duo, preference_cred_session_lifetime_duo_remember,
        preference_cred_bypass_gpio_high, preference_cred_bypass_gpio_low, preference_cred_session_lifetime_totp, preference_cred_session_lifetime_totp_remember
    };
    std::vector<char*> _uintPrefs =
    {
        preference_device_id_lock, preference_device_id_opener, preference_nuki_id_lock, preference_nuki_id_opener
    };
    std::vector<char*> _uint64Prefs =
    {
        preference_nukihub_id
    };
public:
    const std::vector<char*> getPreferencesKeys()
    {
        return _keys;
    }
    const std::vector<char*> getPreferencesRedactedKeys()
    {
        return _redact;
    }
    const std::vector<char*> getPreferencesBoolKeys()
    {
        return _boolPrefs;
    }
    const std::vector<char*> getPreferencesByteKeys()
    {
        return _bytePrefs;
    }
    const std::vector<char*> getPreferencesIntKeys()
    {
        return _intPrefs;
    }
    const std::vector<char*> getPreferencesUIntKeys()
    {
        return _uintPrefs;
    }
    const std::vector<char*> getPreferencesUInt64Keys()
    {
        return _uint64Prefs;
    }
};