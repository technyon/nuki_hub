#pragma once

#include <vector>
#include "Config.h"

#define preference_started_before (char*)"run"
#define preference_config_version (char*)"confVersion"
#define preference_device_id_lock (char*)"deviceId"
#define preference_device_id_opener (char*)"deviceIdOp"
#define preference_nuki_id_lock (char*)"nukiId"
#define preference_nuki_id_opener (char*)"nukidOp"
#define preference_mqtt_broker (char*)"mqttbroker"
#define preference_mqtt_broker_port (char*)"mqttport"
#define preference_mqtt_user (char*)"mqttuser"
#define preference_mqtt_password (char*)"mqttpass"
#define preference_mqtt_log_enabled (char*)"mqttlog"
#define preference_webserver_enabled (char*)"websrvena"
#define preference_lock_enabled (char*)"lockena"
#define preference_lock_pin_status (char*)"lockpin"
#define preference_mqtt_lock_path (char*)"mqttpath"
#define preference_opener_enabled (char*)"openerena"
#define preference_opener_pin_status (char*)"openerpin"
#define preference_opener_continuous_mode (char*)"openercont"
#define preference_mqtt_opener_path (char*)"mqttoppath"
#define preference_check_updates (char*)"checkupdates"
#define preference_lock_max_keypad_code_count (char*)"maxkpad"
#define preference_opener_max_keypad_code_count (char*)"opmaxkpad"
#define preference_lock_max_timecontrol_entry_count (char*)"maxtc"
#define preference_opener_max_timecontrol_entry_count (char*)"opmaxtc"
#define preference_mqtt_ca (char*)"mqttca"
#define preference_mqtt_crt (char*)"mqttcrt"
#define preference_mqtt_key (char*)"mqttkey"
#define preference_mqtt_hass_discovery (char*)"hassdiscovery"
#define preference_mqtt_hass_cu_url (char*)"hassConfigUrl"
#define preference_ip_dhcp_enabled (char*)"dhcpena"
#define preference_ip_address (char*)"ipaddr"
#define preference_ip_subnet (char*)"ipsub"
#define preference_ip_gateway (char*)"ipgtw"
#define preference_ip_dns_server (char*)"dnssrv"
#define preference_network_hardware (char*)"nwhw"
#define preference_network_hardware_gpio (char*)"nwhwdt" // obsolete
#define preference_network_wifi_fallback_disabled (char*)"nwwififb"
#define preference_find_best_rssi (char*)"nwbestrssi"
#define preference_rssi_publish_interval (char*)"rssipb"
#define preference_hostname (char*)"hostname"
#define preference_network_timeout (char*)"nettmout"
#define preference_restart_on_disconnect (char*)"restdisc"
#define preference_restart_ble_beacon_lost (char*)"rstbcn"
#define preference_query_interval_lockstate (char*)"lockStInterval"
#define preference_query_interval_configuration (char*)"configInterval"
#define preference_query_interval_battery (char*)"batInterval"
#define preference_query_interval_keypad (char*)"kpInterval"
#define preference_access_level (char*)"accLvl"
#define preference_keypad_info_enabled (char*)"kpInfoEnabled"
#define preference_keypad_topic_per_entry (char*)"kpPerEntry"
#define preference_keypad_control_enabled (char*)"kpCntrlEnabled"
#define preference_keypad_publish_code (char*)"kpPubCode"
#define preference_timecontrol_control_enabled (char*)"tcCntrlEnabled"
#define preference_timecontrol_topic_per_entry (char*)"tcPerEntry"
#define preference_timecontrol_info_enabled (char*)"tcInfoEnabled"
#define preference_publish_authdata (char*)"pubAuth"
#define preference_acl (char*)"aclLckOpn"
#define preference_conf_info_enabled (char*)"cnfInfoEnabled"
#define preference_conf_lock_basic_acl (char*)"confLckBasAcl"
#define preference_conf_lock_advanced_acl (char*)"confLckAdvAcl"
#define preference_conf_opener_basic_acl (char*)"confOpnBasAcl"
#define preference_conf_opener_advanced_acl (char*)"confOpnAdvAcl"
#define preference_register_as_app (char*)"regAsApp" // true = register as hub; false = register as app
#define preference_register_opener_as_app (char*)"regOpnAsApp"
#define preference_command_nr_of_retries (char*)"nrRetry"
#define preference_command_retry_delay (char*)"rtryDelay"
#define preference_cred_user (char*)"crdusr"
#define preference_cred_password (char*)"crdpass"
#define preference_gpio_locking_enabled (char*)"gpiolck" // obsolete
#define preference_gpio_configuration (char*)"gpiocfg"
#define preference_publish_debug_info (char*)"pubdbg"
#define preference_presence_detection_timeout (char*)"prdtimeout"
#define preference_has_mac_saved (char*)"hasmac"
#define preference_has_mac_byte_0 (char*)"macb0"
#define preference_has_mac_byte_1 (char*)"macb1"
#define preference_has_mac_byte_2 (char*)"macb2"
#define preference_latest_version (char*)"latest"
#define preference_task_size_network (char*)"tsksznetw"
#define preference_task_size_nuki (char*)"tsksznuki"
#define preference_authlog_max_entries (char*)"authmaxentry"
#define preference_keypad_max_entries (char*)"kpmaxentry"
#define preference_timecontrol_max_entries (char*)"tcmaxentry"
#define preference_bootloop_counter (char*)"btlpcounter"
#define preference_enable_bootloop_reset (char*)"enabtlprst"
#define preference_buffer_size (char*)"buffsize"
#define preference_disable_non_json (char*)"disnonjson"
#define preference_official_hybrid (char*)"offHybrid"
#define preference_official_hybrid_actions (char*)"hybridAct"
#define preference_official_hybrid_retry (char*)"hybridRtry"
#define preference_query_interval_hybrid_lockstate (char*)"hybridTimer"
#define preference_ota_main_url (char*)"otaMainUrl"
#define preference_ota_updater_url (char*)"otaUpdUrl"
#define preference_update_from_mqtt (char*)"updMqtt"
#define preference_show_secrets (char*)"showSecr"
#define preference_ble_tx_power (char*)"bleTxPwr"
#define preference_recon_netw_on_mqtt_discon (char*)"recNtwMqttDis"
#define preference_lock_max_auth_entry_count (char*)"maxauth"
#define preference_opener_max_auth_entry_count (char*)"opmaxauth"
#define preference_auth_control_enabled (char*)"authCtrlEna"
#define preference_auth_topic_per_entry (char*)"authPerEntry"
#define preference_auth_info_enabled (char*)"authInfoEna"
#define preference_auth_max_entries (char*)"authmaxentry"

inline bool initPreferences(Preferences* preferences)
{
#ifdef NUKI_HUB_UPDATER
    bool firstStart = false;
    return firstStart;
#else
    bool firstStart = !preferences->getBool(preference_started_before);
#endif

    preferences->remove(preference_bootloop_counter);

    if(firstStart)
    {
        preferences->putBool(preference_started_before, true);
        preferences->putBool(preference_lock_enabled, true);
        uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
        uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
        uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
    }
    else
    {
        int configVer = preferences->getInt(preference_config_version);

        if(configVer < (atof(NUKI_HUB_VERSION) * 100))
        {
            if (configVer < 834)
            {
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
                        uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
                        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                        uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
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
                        uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                        uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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
                        uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                        uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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
                        uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                        uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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
                        uint32_t advancedLockConfigAclPrefs[22] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                        uint32_t advancedOpenerConfigAclPrefs[20] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                        preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                        break;
                    }
                }
            }

            preferences->putInt(preference_config_version, atof(NUKI_HUB_VERSION) * 100);
        }
    }

    return firstStart;
}

class DebugPreferences
{
private:
    std::vector<char*> _keys =
    {
            preference_started_before, preference_config_version, preference_device_id_lock, preference_device_id_opener, preference_nuki_id_lock, preference_nuki_id_opener,
            preference_mqtt_broker, preference_mqtt_broker_port, preference_mqtt_user, preference_mqtt_password, preference_mqtt_log_enabled, preference_check_updates,
            preference_webserver_enabled, preference_lock_enabled, preference_lock_pin_status, preference_mqtt_lock_path, preference_opener_enabled, preference_opener_pin_status,
            preference_opener_continuous_mode, preference_mqtt_opener_path, preference_lock_max_keypad_code_count, preference_opener_max_keypad_code_count,
            preference_lock_max_timecontrol_entry_count, preference_opener_max_timecontrol_entry_count, preference_enable_bootloop_reset, preference_mqtt_ca, preference_mqtt_crt,
            preference_mqtt_key, preference_mqtt_hass_discovery, preference_mqtt_hass_cu_url, preference_buffer_size, preference_ip_dhcp_enabled, preference_ip_address,
            preference_ip_subnet, preference_ip_gateway, preference_ip_dns_server, preference_network_hardware, preference_network_wifi_fallback_disabled,
            preference_rssi_publish_interval, preference_hostname, preference_find_best_rssi, preference_network_timeout, preference_restart_on_disconnect,
            preference_restart_ble_beacon_lost, preference_query_interval_lockstate, preference_timecontrol_topic_per_entry, preference_keypad_topic_per_entry,
            preference_query_interval_configuration, preference_query_interval_battery, preference_query_interval_keypad, preference_keypad_control_enabled,
            preference_keypad_info_enabled, preference_keypad_publish_code, preference_timecontrol_control_enabled, preference_timecontrol_info_enabled, preference_conf_info_enabled,
            preference_register_as_app, preference_register_opener_as_app, preference_command_nr_of_retries, preference_command_retry_delay, preference_cred_user,
            preference_cred_password, preference_disable_non_json, preference_publish_authdata, preference_publish_debug_info, preference_presence_detection_timeout,
            preference_official_hybrid, preference_query_interval_hybrid_lockstate, preference_official_hybrid_actions, preference_official_hybrid_retry, preference_has_mac_saved,
            preference_has_mac_byte_0, preference_has_mac_byte_1, preference_has_mac_byte_2, preference_latest_version, preference_task_size_network, preference_task_size_nuki,
            preference_authlog_max_entries, preference_keypad_max_entries, preference_timecontrol_max_entries, preference_update_from_mqtt, preference_show_secrets,
            preference_lock_max_auth_entry_count, preference_opener_max_auth_entry_count, preference_auth_control_enabled, preference_auth_topic_per_entry,
            preference_auth_info_enabled, preference_auth_max_entries, preference_ble_tx_power, preference_recon_netw_on_mqtt_discon
    };
    std::vector<char*> _redact =
    {
        preference_mqtt_user, preference_mqtt_password, preference_mqtt_ca, preference_mqtt_crt, preference_mqtt_key, preference_cred_user, preference_cred_password,
        preference_nuki_id_lock, preference_nuki_id_opener,
    };
    std::vector<char*> _boolPrefs =
    {
            preference_started_before, preference_mqtt_log_enabled, preference_check_updates, preference_lock_enabled, preference_opener_enabled, preference_opener_continuous_mode,
            preference_timecontrol_topic_per_entry, preference_keypad_topic_per_entry, preference_enable_bootloop_reset, preference_webserver_enabled, preference_find_best_rssi,
            preference_restart_on_disconnect, preference_keypad_control_enabled, preference_keypad_info_enabled, preference_keypad_publish_code, preference_show_secrets,
            preference_timecontrol_control_enabled, preference_timecontrol_info_enabled, preference_register_as_app, preference_register_opener_as_app, preference_ip_dhcp_enabled,
            preference_publish_authdata, preference_has_mac_saved, preference_publish_debug_info, preference_network_wifi_fallback_disabled, preference_official_hybrid,
            preference_official_hybrid_actions, preference_official_hybrid_retry, preference_conf_info_enabled, preference_disable_non_json, preference_update_from_mqtt,
            preference_auth_control_enabled, preference_auth_topic_per_entry, preference_auth_info_enabled, preference_recon_netw_on_mqtt_discon
    };
    std::vector<char*> _bytePrefs =
    {
            preference_acl, preference_conf_info_enabled, preference_conf_lock_basic_acl, preference_conf_lock_advanced_acl, preference_conf_opener_basic_acl,
            preference_conf_opener_advanced_acl, preference_gpio_configuration
    };
    std::vector<char*> _intPrefs =
    {
            preference_config_version, preference_device_id_lock, preference_device_id_opener, preference_nuki_id_lock, preference_nuki_id_opener, preference_mqtt_broker_port,
            preference_lock_pin_status, preference_opener_pin_status, preference_lock_max_keypad_code_count, preference_opener_max_keypad_code_count,
            preference_lock_max_timecontrol_entry_count, preference_opener_max_timecontrol_entry_count, preference_buffer_size, preference_network_hardware,
            preference_rssi_publish_interval, preference_network_timeout, preference_restart_ble_beacon_lost, preference_query_interval_lockstate,
            preference_query_interval_configuration, preference_query_interval_battery, preference_query_interval_keypad, preference_command_nr_of_retries,
            preference_command_retry_delay, preference_presence_detection_timeout, preference_query_interval_hybrid_lockstate, preference_latest_version,
            preference_task_size_network, preference_task_size_nuki, preference_authlog_max_entries, preference_keypad_max_entries, preference_timecontrol_max_entries,
            preference_ble_tx_power
    };
    std::vector<char*> _charPrefs =
    {
        preference_has_mac_byte_0, preference_has_mac_byte_1, preference_has_mac_byte_2
    };
    const bool isRedacted(const char* key) const
    {
        return std::find(_redact.begin(), _redact.end(), key) != _redact.end();
    }
    const String redact(const String s) const
    {
        return s == "" ? "" : "***";
    }
    const String redact(const int32_t i) const
    {
        return i == 0 ? "" : "***";
    }
    const String redact(const uint32_t i) const
    {
        return i == 0 ? "" : "***";
    }
    const String redact(const int64_t i) const
    {
        return i == 0 ? "" : "***";
    }
    const String redact(const uint64_t i) const
    {
        return i == 0 ? "" : "***";
    }

    const void appendPreferenceInt8(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const int32_t)preferences->getChar(key)) : String(preferences->getChar(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt8(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const uint32_t)preferences->getUChar(key)) : String(preferences->getUChar(key)));
        s.concat("\n");
    }
    const void appendPreferenceInt16(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const int32_t)preferences->getShort(key)) : String(preferences->getShort(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt16(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const uint32_t)preferences->getUShort(key)) : String(preferences->getUShort(key)));
        s.concat("\n");
    }
    const void appendPreferenceInt32(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const int32_t)preferences->getInt(key)) : String(preferences->getInt(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt32(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const uint32_t)preferences->getUInt(key)) : String(preferences->getUInt(key)));
        s.concat("\n");
    }
    const void appendPreferenceInt64(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const int64_t)preferences->getLong64(key)) : String(preferences->getLong64(key)));
        s.concat("\n");
    }
    const void appendPreferenceUInt64(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const uint64_t)preferences->getULong64(key)) : String(preferences->getULong64(key)));
        s.concat("\n");
    }
    const void appendPreferenceBool(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(preferences->getBool(key) ? "true" : "false");
        s.concat("\n");
    }
    const void appendPreferenceString(Preferences *preferences, String& s, const char* description, const char* key)
    {
        s.concat(description);
        s.concat(": ");
        s.concat(isRedacted(key) ? redact((const String)preferences->getString(key)) : preferences->getString(key));
        s.concat("\n");
    }

    const void appendPreference(Preferences *preferences, String& s, const char* key)
    {
        if(std::find(_boolPrefs.begin(), _boolPrefs.end(), key) != _boolPrefs.end())
        {
            appendPreferenceBool(preferences, s, key, key);
            return;
        }

        switch(preferences->getType(key))
        {
            case PT_I8:
                appendPreferenceInt8(preferences, s, key, key);
                break;
            case PT_I16:
                appendPreferenceInt16(preferences, s, key, key);
                break;
            case PT_I32:
                appendPreferenceInt32(preferences, s, key, key);
                break;
            case PT_I64:
                appendPreferenceInt64(preferences, s, key, key);
                break;
            case PT_U8:
                appendPreferenceUInt8(preferences, s, key, key);
                break;
            case PT_U16:
                appendPreferenceUInt16(preferences, s, key, key);
                break;
            case PT_U32:
                appendPreferenceUInt32(preferences, s, key, key);
                break;
            case PT_U64:
                appendPreferenceUInt64(preferences, s, key, key);
                break;
            case PT_STR:
                appendPreferenceString(preferences, s, key, key);
                break;
            default:
                appendPreferenceString(preferences, s, key, key);
                break;
        }
    }

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
    const std::vector<char*> getPreferencesCharKeys()
    {
        return _charPrefs;
    }
    const String preferencesToString(Preferences *preferences)
    {
        String s = "";

        for(const auto& key : _keys)
        {
            appendPreference(preferences, s, key);
        }

        return s;
    }

};