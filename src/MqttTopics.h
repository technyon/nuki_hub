#pragma once
#include <vector>

#define mqtt_topic_lock_action (char*)"/action"
#define mqtt_topic_lock_status_updated (char*)"/statusUpdated"
#define mqtt_topic_lock_state (char*)"/state"
#define mqtt_topic_lock_ha_state (char*)"/hastate"
#define mqtt_topic_lock_json (char*)"/json"
#define mqtt_topic_lock_binary_state (char*)"/binaryState"
#define mqtt_topic_lock_continuous_mode (char*)"/continuousMode"
#define mqtt_topic_lock_ring (char*)"/ring"
#define mqtt_topic_lock_binary_ring (char*)"/binaryRing"
#define mqtt_topic_lock_trigger (char*)"/trigger"
#define mqtt_topic_lock_last_lock_action (char*)"/lastLockAction"
#define mqtt_topic_lock_log (char*)"/log"
#define mqtt_topic_lock_log_latest (char*)"/shortLog"
#define mqtt_topic_lock_log_rolling (char*)"/rollingLog"
#define mqtt_topic_lock_log_rolling_last (char*)"/lastRollingLog"
#define mqtt_topic_lock_auth_id (char*)"/authorizationId"
#define mqtt_topic_lock_auth_name (char*)"/authorizationName"
#define mqtt_topic_lock_completionStatus (char*)"/completionStatus"
#define mqtt_topic_lock_action_command_result (char*)"/commandResult"
#define mqtt_topic_lock_door_sensor_state (char*)"/doorSensorState"
#define mqtt_topic_lock_rssi (char*)"/rssi"
#define mqtt_topic_lock_address (char*)"/address"
#define mqtt_topic_lock_retry (char*)"/retry"

#define mqtt_topic_official_lock_action (char*)"/lockAction"
//#define mqtt_topic_official_mode (char*)"/mode"
#define mqtt_topic_official_state (char*)"/state"
#define mqtt_topic_official_batteryCritical (char*)"/batteryCritical"
#define mqtt_topic_official_batteryChargeState (char*)"/batteryChargeState"
#define mqtt_topic_official_batteryCharging (char*)"/batteryCharging"
#define mqtt_topic_official_keypadBatteryCritical (char*)"/keypadBatteryCritical"
#define mqtt_topic_official_doorsensorState (char*)"/doorsensorState"
#define mqtt_topic_official_doorsensorBatteryCritical (char*)"/doorsensorBatteryCritical"
#define mqtt_topic_official_connected (char*)"/connected"
#define mqtt_topic_official_commandResponse (char*)"/commandResponse"
#define mqtt_topic_official_lockActionEvent (char*)"/lockActionEvent"

#define mqtt_topic_config_action (char*)"/configuration/action"
#define mqtt_topic_config_action_command_result (char*)"/configuration/commandResult"
#define mqtt_topic_config_basic_json (char*)"/configuration/basicJson"
#define mqtt_topic_config_advanced_json (char*)"/configuration/advancedJson"
#define mqtt_topic_config_button_enabled (char*)"/configuration/buttonEnabled"
#define mqtt_topic_config_led_enabled (char*)"/configuration/ledEnabled"
#define mqtt_topic_config_led_brightness (char*)"/configuration/ledBrightness"
#define mqtt_topic_config_auto_unlock (char*)"/configuration/autoUnlock"
#define mqtt_topic_config_auto_lock (char*)"/configuration/autoLock"
#define mqtt_topic_config_single_lock (char*)"/configuration/singleLock"
#define mqtt_topic_config_sound_level (char*)"/configuration/soundLevel"

#define mqtt_topic_query_config (char*)"/query/config"
#define mqtt_topic_query_lockstate (char*)"/query/lockstate"
#define mqtt_topic_query_keypad (char*)"/query/keypad"
#define mqtt_topic_query_battery (char*)"/query/battery"
#define mqtt_topic_query_lockstate_command_result (char*)"/query/lockstateCommandResult"

#define mqtt_topic_battery_level (char*)"/battery/level"
#define mqtt_topic_battery_debug (char*)"/battery/debug"
#define mqtt_topic_battery_critical (char*)"/battery/critical"
#define mqtt_topic_battery_charging (char*)"/battery/charging"
#define mqtt_topic_battery_voltage (char*)"/battery/voltage"
#define mqtt_topic_battery_drain (char*)"/battery/drain"
#define mqtt_topic_battery_max_turn_current (char*)"/battery/maxTurnCurrent"
#define mqtt_topic_battery_lock_distance (char*)"/battery/lockDistance"
#define mqtt_topic_battery_keypad_critical (char*)"/battery/keypadCritical"
#define mqtt_topic_battery_doorsensor_critical (char*)"/battery/doorSensorCritical"
#define mqtt_topic_battery_basic_json (char*)"/battery/basicJson"
#define mqtt_topic_battery_advanced_json (char*)"/battery/advancedJson"

#define mqtt_topic_keypad (char*)"/keypad"
#define mqtt_topic_keypad_codes (char*)"/keypad/codes"
#define mqtt_topic_keypad_command_action (char*)"/keypad/command/action"
#define mqtt_topic_keypad_command_id (char*)"/keypad/command/id"
#define mqtt_topic_keypad_command_name (char*)"/keypad/command/name"
#define mqtt_topic_keypad_command_code (char*)"/keypad/command/code"
#define mqtt_topic_keypad_command_enabled (char*)"/keypad/command/enabled"
#define mqtt_topic_keypad_command_result (char*)"/keypad/command/commandResult"
#define mqtt_topic_keypad_json (char*)"/keypad/json"
#define mqtt_topic_keypad_json_action (char*)"/keypad/actionJson"
#define mqtt_topic_keypad_json_command_result (char*)"/keypad/commandResultJson"

#define mqtt_topic_timecontrol (char*)"/timecontrol"
#define mqtt_topic_timecontrol_entries (char*)"/timecontrol/entries"
#define mqtt_topic_timecontrol_json (char*)"/timecontrol/json"
#define mqtt_topic_timecontrol_action (char*)"/timecontrol/action"
#define mqtt_topic_timecontrol_command_result (char*)"/timecontrol/commandResult"

#define mqtt_topic_auth (char*)"/authorization"
#define mqtt_topic_auth_entries (char*)"/authorization/entries"
#define mqtt_topic_auth_json (char*)"/authorization/json"
#define mqtt_topic_auth_action (char*)"/authorization/action"
#define mqtt_topic_auth_command_result (char*)"/authorization/commandResult"

#define mqtt_topic_info_hardware_version (char*)"/info/hardwareVersion"
#define mqtt_topic_info_firmware_version (char*)"/info/firmwareVersion"
#define mqtt_topic_info_nuki_hub_version (char*)"/info/nukiHubVersion"
#define mqtt_topic_info_nuki_hub_build (char*)"/info/nukiHubBuild"
#define mqtt_topic_info_nuki_hub_latest (char*)"/info/nukiHubLatest"
#define mqtt_topic_info_nuki_hub_ip (char*)"/info/nukiHubIp"

#define mqtt_topic_reset (char*)"/maintenance/reset"
#define mqtt_topic_update (char*)(char*)"/maintenance/update"
#define mqtt_topic_webserver_state (char*)"/maintenance/webserver/state"
#define mqtt_topic_webserver_action (char*)"/maintenance/webserver/enable"
#define mqtt_topic_uptime (char*)"/maintenance/uptime"
#define mqtt_topic_wifi_rssi (char*)"/maintenance/wifiRssi"
#define mqtt_topic_log (char*)"/maintenance/log"
#define mqtt_topic_freeheap (char*)"/maintenance/freeHeap"
#define mqtt_topic_restart_reason_fw (char*)"/maintenance/restartReasonNukiHub"
#define mqtt_topic_restart_reason_esp (char*)"/maintenance/restartReasonNukiEsp"
#define mqtt_topic_mqtt_connection_state (char*)"/maintenance/mqttConnectionState"
#define mqtt_topic_network_device (char*)"/maintenance/networkDevice"
#define mqtt_topic_hybrid_state (char*)"/hybridConnected"

#define mqtt_topic_gpio_prefix (char*)"/gpio"
#define mqtt_topic_gpio_pin (char*)"/pin_"
#define mqtt_topic_gpio_role (char*)"/role"
#define mqtt_topic_gpio_state (char*)"/state"

class MqttTopics
{
private:
    std::vector<char*> _keys =
    {
        mqtt_topic_lock_action, mqtt_topic_lock_status_updated, mqtt_topic_lock_state, mqtt_topic_lock_ha_state, mqtt_topic_lock_json, mqtt_topic_lock_binary_state,
        mqtt_topic_lock_continuous_mode, mqtt_topic_lock_ring, mqtt_topic_lock_binary_ring, mqtt_topic_lock_trigger, mqtt_topic_lock_last_lock_action, mqtt_topic_lock_log,
        mqtt_topic_lock_log_latest, mqtt_topic_lock_log_rolling, mqtt_topic_lock_log_rolling_last, mqtt_topic_lock_auth_id, mqtt_topic_lock_auth_name, mqtt_topic_lock_completionStatus,
        mqtt_topic_lock_action_command_result, mqtt_topic_lock_door_sensor_state, mqtt_topic_lock_rssi, mqtt_topic_lock_address, mqtt_topic_lock_retry, mqtt_topic_config_action,
        mqtt_topic_config_action_command_result, mqtt_topic_config_basic_json, mqtt_topic_config_advanced_json, mqtt_topic_config_button_enabled, mqtt_topic_config_led_enabled,
        mqtt_topic_config_led_brightness, mqtt_topic_config_auto_unlock, mqtt_topic_config_auto_lock, mqtt_topic_config_single_lock, mqtt_topic_config_sound_level,
        mqtt_topic_query_config, mqtt_topic_query_lockstate, mqtt_topic_query_keypad, mqtt_topic_query_battery, mqtt_topic_query_lockstate_command_result,
        mqtt_topic_battery_level, mqtt_topic_battery_critical, mqtt_topic_battery_charging, mqtt_topic_battery_voltage, mqtt_topic_battery_drain,
        mqtt_topic_battery_max_turn_current, mqtt_topic_battery_lock_distance, mqtt_topic_battery_keypad_critical, mqtt_topic_battery_doorsensor_critical,
        mqtt_topic_battery_basic_json,mqtt_topic_battery_advanced_json, mqtt_topic_keypad, mqtt_topic_keypad_codes, mqtt_topic_keypad_command_action, 
        mqtt_topic_keypad_command_id, mqtt_topic_keypad_command_name, mqtt_topic_keypad_command_code, mqtt_topic_keypad_command_enabled, mqtt_topic_keypad_command_result,
        mqtt_topic_keypad_json, mqtt_topic_keypad_json_action, mqtt_topic_keypad_json_command_result, mqtt_topic_timecontrol, mqtt_topic_timecontrol_entries, 
        mqtt_topic_timecontrol_json, mqtt_topic_timecontrol_action, mqtt_topic_timecontrol_command_result, mqtt_topic_auth, mqtt_topic_auth_entries, 
        mqtt_topic_auth_json, mqtt_topic_auth_action, mqtt_topic_auth_command_result, mqtt_topic_info_hardware_version, mqtt_topic_info_firmware_version, 
        mqtt_topic_info_nuki_hub_version, mqtt_topic_info_nuki_hub_build, mqtt_topic_info_nuki_hub_latest, mqtt_topic_info_nuki_hub_ip, mqtt_topic_reset, 
        mqtt_topic_update, mqtt_topic_webserver_state, mqtt_topic_webserver_action, mqtt_topic_uptime, mqtt_topic_wifi_rssi, mqtt_topic_log, mqtt_topic_freeheap, 
        mqtt_topic_restart_reason_fw, mqtt_topic_restart_reason_esp, mqtt_topic_mqtt_connection_state, mqtt_topic_network_device, mqtt_topic_hybrid_state
    };
public:
    const std::vector<char*> getMqttTopics()
    {
        return _keys;
    }
};
