#pragma once

#include <ArduinoJson.h>
#include "util/NukiHelper.h"

namespace NukiJson
{
inline void dateTime(char* out, int year, int month, int day, int hour, int minute, int second)
{
    sprintf(out, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
}

inline void timeOfDay(char* out, int hour, int minute)
{
    sprintf(out, "%02d:%02d", hour, minute);
}

template<typename TEntry>
void keypadEntry(const TEntry& entry, JsonVariant json, bool includeCode)
{
    char dt[20];
    char tod[6];

    json["codeId"] = entry.codeId;
    if(includeCode)
    {
        json["code"] = entry.code;
    }
    json["enabled"] = entry.enabled;
    json["name"] = entry.name;
    dateTime(dt, entry.dateCreatedYear, entry.dateCreatedMonth, entry.dateCreatedDay, entry.dateCreatedHour, entry.dateCreatedMin, entry.dateCreatedSec);
    json["dateCreated"] = dt;
    json["lockCount"] = entry.lockCount;
    dateTime(dt, entry.dateLastActiveYear, entry.dateLastActiveMonth, entry.dateLastActiveDay, entry.dateLastActiveHour, entry.dateLastActiveMin, entry.dateLastActiveSec);
    json["dateLastActive"] = dt;
    json["timeLimited"] = entry.timeLimited;
    dateTime(dt, entry.allowedFromYear, entry.allowedFromMonth, entry.allowedFromDay, entry.allowedFromHour, entry.allowedFromMin, entry.allowedFromSec);
    json["allowedFrom"] = dt;
    dateTime(dt, entry.allowedUntilYear, entry.allowedUntilMonth, entry.allowedUntilDay, entry.allowedUntilHour, entry.allowedUntilMin, entry.allowedUntilSec);
    json["allowedUntil"] = dt;
    JsonArray weekdays = json["allowedWeekdays"].to<JsonArray>();
    NukiHelper::weekdaysToJsonArray(entry.allowedWeekdays, weekdays);
    timeOfDay(tod, entry.allowedFromTimeHour, entry.allowedFromTimeMin);
    json["allowedFromTime"] = tod;
    timeOfDay(tod, entry.allowedUntilTimeHour, entry.allowedUntilTimeMin);
    json["allowedUntilTime"] = tod;
}

template<typename TEntry, typename TLockActionToString>
void timeControlEntry(const TEntry& entry, JsonVariant json, TLockActionToString lockActionToString)
{
    char tod[6];
    char action[50] = {0};

    json["entryId"] = entry.entryId;
    json["enabled"] = entry.enabled;
    JsonArray weekdays = json["weekdays"].to<JsonArray>();
    NukiHelper::weekdaysToJsonArray(entry.weekdays, weekdays);
    timeOfDay(tod, entry.timeHour, entry.timeMin);
    json["time"] = tod;
    lockActionToString(entry.lockAction, action);
    json["lockAction"] = action;
}

template<typename TEntry>
void authEntry(const TEntry& entry, JsonVariant json)
{
    char dt[20];
    char tod[6];

    json["authId"] = entry.authId;
    json["idType"] = entry.idType;
    json["enabled"] = entry.enabled;
    json["name"] = entry.name;
    json["remoteAllowed"] = entry.remoteAllowed;
    dateTime(dt, entry.createdYear, entry.createdMonth, entry.createdDay, entry.createdHour, entry.createdMinute, entry.createdSecond);
    json["dateCreated"] = dt;
    json["lockCount"] = entry.lockCount;
    dateTime(dt, entry.lastActYear, entry.lastActMonth, entry.lastActDay, entry.lastActHour, entry.lastActMinute, entry.lastActSecond);
    json["dateLastActive"] = dt;
    json["timeLimited"] = entry.timeLimited;
    dateTime(dt, entry.allowedFromYear, entry.allowedFromMonth, entry.allowedFromDay, entry.allowedFromHour, entry.allowedFromMinute, entry.allowedFromSecond);
    json["allowedFrom"] = dt;
    dateTime(dt, entry.allowedUntilYear, entry.allowedUntilMonth, entry.allowedUntilDay, entry.allowedUntilHour, entry.allowedUntilMinute, entry.allowedUntilSecond);
    json["allowedUntil"] = dt;
    JsonArray weekdays = json["allowedWeekdays"].to<JsonArray>();
    NukiHelper::weekdaysToJsonArray(entry.allowedWeekdays, weekdays);
    timeOfDay(tod, entry.allowedFromTimeHour, entry.allowedFromTimeMin);
    json["allowedFromTime"] = tod;
    timeOfDay(tod, entry.allowedUntilTimeHour, entry.allowedUntilTimeMin);
    json["allowedUntilTime"] = tod;
}
}
