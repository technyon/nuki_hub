#include <cstring>
#include "NukiOfficial.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "../lib/nuki_ble/src/NukiLockUtils.h"
#include <stdlib.h>
#include <ctype.h>


NukiOfficial::NukiOfficial(Preferences *preferences)
{
    offEnabled = preferences->getBool(preference_official_hybrid_enabled, false);
    _disableNonJSON = preferences->getBool(preference_disable_non_json, false);
}

void NukiOfficial::setUid(const uint32_t& uid)
{
    char uidString[20];
    itoa(uid, uidString, 16);

    for(char* c=uidString; *c=toupper(*c); ++c);

    strcpy(mqttPath, "nuki/");
    strcat(mqttPath, uidString);

    offTopics.reserve(10);
    //_offTopics.push_back(mqtt_topic_official_mode);
    offTopics.push_back((char*)mqtt_topic_official_state);
    offTopics.push_back((char*)mqtt_topic_official_batteryCritical);
    offTopics.push_back((char*)mqtt_topic_official_batteryChargeState);
    offTopics.push_back((char*)mqtt_topic_official_batteryCharging);
    offTopics.push_back((char*)mqtt_topic_official_keypadBatteryCritical);
    offTopics.push_back((char*)mqtt_topic_official_doorsensorState);
    offTopics.push_back((char*)mqtt_topic_official_doorsensorBatteryCritical);
    offTopics.push_back((char*)mqtt_topic_official_connected);
    offTopics.push_back((char*)mqtt_topic_official_commandResponse);
    offTopics.push_back((char*)mqtt_topic_official_lockActionEvent);
}

void NukiOfficial::setPublisher(NukiPublisher *publisher)
{
    _publisher = publisher;
}

const char *NukiOfficial::getMqttPath() const
{
    return mqttPath;
}

void NukiOfficial::buildMqttPath(const char *path, char *outPath)
{
    int offset = 0;
    char inPath[181] = {0};

    memcpy(inPath, mqttPath, sizeof(mqttPath));

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

bool NukiOfficial::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

void NukiOfficial::onOfficialUpdateReceived(const char *topic, const char *value)
{
    char str[50];
    bool publishBatteryJson = false;
    memset(&str, 0, sizeof(str));

    Log->println("Official Nuki change received");
    Log->print(F("Topic: "));
    Log->println(topic);
    Log->print(F("Value: "));
    Log->println(value);

    if(strcmp(topic, mqtt_topic_official_connected) == 0)
    {
        Log->print(F("Connected: "));
        Log->println((strcmp(value, "true") == 0 ? 1 : 0));
        offConnected = (strcmp(value, "true") == 0 ? 1 : 0);
        _publisher->publishBool(mqtt_topic_hybrid_state, offConnected, true);
    }
    else if(strcmp(topic, mqtt_topic_official_state) == 0)
    {
        offState = atoi(value);
        _statusUpdated = true;
        Log->println(F("Lock: Updating status on Hybrid state change"));
        _publisher->publishBool(mqtt_topic_hybrid_state, offConnected, true);
        NukiLock::lockstateToString((NukiLock::LockState)offState, str);
        _publisher->publishString(mqtt_topic_lock_state, str, true);

        Log->print(F("Lockstate: "));
        Log->println(str);

        _offStateToPublish = (NukiLock::LockState)offState;
        _hasOffStateToPublish = true;
    }
    else if(strcmp(topic, mqtt_topic_official_doorsensorState) == 0)
    {
        offDoorsensorState = atoi(value);
        _statusUpdated = true;
        Log->println(F("Lock: Updating status on Hybrid door sensor state change"));
        _publisher->publishBool(mqtt_topic_lock_status_updated, _statusUpdated, true);
        NukiLock::doorSensorStateToString((NukiLock::DoorSensorState)offDoorsensorState, str);

        Log->print(F("Doorsensor state: "));
        Log->println(str);

        _publisher->publishString(mqtt_topic_lock_door_sensor_state, str, true);
    }
    else if(strcmp(topic, mqtt_topic_official_batteryCritical) == 0)
    {
        offCritical = (strcmp(value, "true") == 0 ? 1 : 0);

        Log->print(F("Battery critical: "));
        Log->println(offCritical);

        if(!_disableNonJSON)
        {
            _publisher->publishBool(mqtt_topic_battery_critical, offCritical, true);
        }
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_batteryCharging) == 0)
    {
        offCharging = (strcmp(value, "true") == 0 ? 1 : 0);

        Log->print(F("Battery charging: "));
        Log->println(offCharging);

        if(!_disableNonJSON)
        {
            _publisher->publishBool(mqtt_topic_battery_charging, offCharging, true);
        }
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_batteryChargeState) == 0)
    {
        offChargeState = atoi(value);

        Log->print(F("Battery level: "));
        Log->println(offChargeState);

        if(!_disableNonJSON)
        {
            _publisher->publishInt(mqtt_topic_battery_level, offChargeState, true);
        }
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_keypadBatteryCritical) == 0)
    {
        offKeypadCritical = (strcmp(value, "true") == 0 ? 1 : 0);
        if(!_disableNonJSON)
        {
            _publisher->publishBool(mqtt_topic_battery_keypad_critical, offKeypadCritical, true);
        }
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_doorsensorBatteryCritical) == 0)
    {
        offDoorsensorCritical = (strcmp(value, "true") == 0 ? 1 : 0);
        if(!_disableNonJSON)
        {
            _publisher->publishBool(mqtt_topic_battery_doorsensor_critical, offDoorsensorCritical, true);
        }
        publishBatteryJson = true;
    }
    else if(strcmp(topic, mqtt_topic_official_commandResponse) == 0)
    {
        offCommandResponse = atoi(value);
        if(offCommandResponse == 0)
        {
            clearOffCommandExecutedTs();
        }
        char resultStr[15] = {0};
        NukiLock::cmdResultToString((Nuki::CmdResult)offCommandResponse, resultStr);
        _publisher->publishString(mqtt_topic_lock_action_command_result, resultStr, true);
    }
    else if(strcmp(topic, mqtt_topic_official_lockActionEvent) == 0)
    {
        clearOffCommandExecutedTs();
        offLockActionEvent = (char*)value;
        String LockActionEvent = offLockActionEvent;
        const int ind1 = LockActionEvent.indexOf(',');
        const int ind2 = LockActionEvent.indexOf(',', ind1+1);
        const int ind3 = LockActionEvent.indexOf(',', ind2+1);
        const int ind4 = LockActionEvent.indexOf(',', ind3+1);
        const int ind5 = LockActionEvent.indexOf(',', ind4+1);

        offLockAction = atoi(LockActionEvent.substring(0, ind1).c_str());
        offTrigger = atoi(LockActionEvent.substring(ind1 + 1, ind2 + 1).c_str());
        offAuthId = atoi(LockActionEvent.substring(ind2 + 1, ind3 + 1).c_str());
        offCodeId = atoi(LockActionEvent.substring(ind3 + 1, ind4 + 1).c_str());
//        offContext = atoi(LockActionEvent.substring(ind4 + 1, ind5 + 1).c_str());

        memset(&str, 0, sizeof(str));
        lockactionToString((NukiLock::LockAction)offLockAction, str);
        _publisher->publishString(mqtt_topic_lock_last_lock_action, str, true);

        memset(&str, 0, sizeof(str));
        triggerToString((NukiLock::Trigger)offTrigger, str);
        _publisher->publishString(mqtt_topic_lock_trigger, str, true);

        if(offAuthId > 0 || offCodeId > 0)
        {
            if(offCodeId > 0)
            {
                _authId = offCodeId;
            }
            else
            {
                _authId = offAuthId;
            }
            _hasAuthId = true;

            /*
            _network->_authName = RETRIEVE FROM VECTOR AFTER AUTHORIZATION ENTRIES ARE IMPLEMENTED;
            _offContext = BASE ON CONTEXT OF TRIGGER AND PUBLISH TO MQTT;
            */
        }
    }

    if(publishBatteryJson)
    {
        JsonDocument jsonBattery;
        char _resbuf[2048];
        jsonBattery["critical"] = offCritical ? "1" : "0";
        jsonBattery["charging"] = offCharging ? "1" : "0";
        jsonBattery["level"] = offChargeState;
        jsonBattery["keypadCritical"] = offKeypadCritical ? "1" : "0";
        jsonBattery["doorSensorCritical"] = offDoorsensorCritical ? "1" : "0";
        serializeJson(jsonBattery, _resbuf, sizeof(_resbuf));
        _publisher->publishString(mqtt_topic_battery_basic_json, _resbuf, true);
    }
}

const bool NukiOfficial::getStatusUpdated()
{
    bool stu = _statusUpdated;
    _statusUpdated = false;
    return stu;
}

const bool NukiOfficial::hasOffStateToPublish()
{
    bool hasOff = _hasOffStateToPublish;
    _hasOffStateToPublish = false;
    return hasOff;
}

const NukiLock::LockState NukiOfficial::getOffStateToPublish() const
{
    return _offStateToPublish;
}

const uint32_t NukiOfficial::getAuthId() const
{
    return _authId;
}

const bool NukiOfficial::hasAuthId() const
{
    return _hasAuthId;
}

void NukiOfficial::clearAuthId()
{
    _hasAuthId = false;
}

const bool NukiOfficial::getOffConnected() const
{
    return offConnected;
}

const bool NukiOfficial::getOffEnabled() const
{
    return offEnabled;
}

const uint8_t NukiOfficial::getOffDoorsensorState() const
{
    return offDoorsensorState;
}

const uint8_t NukiOfficial::getOffState() const
{
    return offState;
}

const uint8_t NukiOfficial::getOffLockAction() const
{
    return offLockAction;
}

const uint8_t NukiOfficial::getOffTrigger() const
{
    return offTrigger;
}

const int64_t NukiOfficial::getOffCommandExecutedTs() const
{
    return offCommandExecutedTs;
}

void NukiOfficial::setOffCommandExecutedTs(const int64_t &value)
{
    offCommandExecutedTs = value;
}

void NukiOfficial::clearOffCommandExecutedTs()
{
    offCommandExecutedTs = 0;
}

const std::vector<char *> NukiOfficial::getOffTopics() const
{
    return offTopics;
}
