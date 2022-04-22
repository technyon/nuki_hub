#pragma once

#define mqtt_topic_battery_level "/battery/level"
#define mqtt_topic_battery_critical "/battery/critical"
#define mqtt_topic_battery_charging "/battery/charging"
#define mqtt_topic_battery_voltage "/battery/voltage"
#define mqtt_topic_battery_drain "/battery/drain"
#define mqtt_topic_battery_max_turn_current "/battery/maxTurnCurrent"
#define mqtt_topic_battery_lock_distance "/battery/lockDistance"

#define mqtt_topic_lock_state "/lock/state"
#define mqtt_topic_lock_trigger "/lock/trigger"
#define mqtt_topic_lock_completionStatus "/lock/completionStatus"
#define mqtt_topic_lock_action_command_result "/lock/commandResult"
#define mqtt_topic_door_sensor_state "/lock/doorSensorState"
#define mqtt_topic_lock_action "/lock/action"

#define mqtt_topic_config_button_enabled "/configuration/buttonEnabled"
#define mqtt_topic_config_led_enabled "/configuration/ledEnabled"
#define mqtt_topic_config_led_brightness "/configuration/ledBrightness"
#define mqtt_topic_config_auto_unlock "/configuration/autoUnlock"
#define mqtt_topic_config_auto_lock "/configuration/autoLock"

#define mqtt_topic_presence "/presence/devices"
