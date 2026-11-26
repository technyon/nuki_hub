#pragma once

#include <ArduinoJson.h>
#include <list>
#include <vector>
#include "NukiDataTypes.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "EspMillis.h"
#include "TaskWake.h"
#include "Config.h"
#include "NukiCommandCore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum class NukiCommandType : uint8_t
{
    KeypadJson,
    Keypad,
    Config,
    TimeControl,
    Auth
};

struct NukiQueuedCommand
{
    NukiCommandType type;
    char payload[NUKI_COMMAND_MAX_LENGTH];
};

template<typename TDerived, typename TTraits>
class NukiCommandHandler
{
public:
    void onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    void onKeypadJsonCommandReceived(const char* value);
    void onConfigUpdateReceived(const char* value);
    void onTimeControlCommandReceived(const char* value);
    void onAuthCommandReceived(const char* value);
    void processCommandQueue();
    bool hasQueuedCommands();

protected:
    void executeKeypadCommand(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    void executeKeypadJsonCommand(const char* value);
    void executeTimeControlCommand(const char* value);
    void executeAuthCommand(const char* value);

private:
    NukiCommandContext commandContext();
    void enqueueCommand(NukiCommandType type, const char* payload);
    void publishBusy(NukiCommandType type, const char* result);
};

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::publishBusy(NukiCommandType type, const char* result)
{
    TDerived& d = static_cast<TDerived&>(*this);

    switch(type)
    {
    case NukiCommandType::KeypadJson:
        d._network->publishKeypadJsonCommandResult(result);
        break;
    case NukiCommandType::Keypad:
        d._network->publishKeypadCommandResult(result);
        break;
    case NukiCommandType::Config:
        d._network->publishConfigCommandResult(result);
        break;
    case NukiCommandType::TimeControl:
        d._network->publishTimeControlCommandResult(result);
        break;
    case NukiCommandType::Auth:
        d._network->publishAuthCommandResult(result);
        break;
    }
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::enqueueCommand(NukiCommandType type, const char* payload)
{
    TDerived& d = static_cast<TDerived&>(*this);
    NukiQueuedCommand cmd;

    if(strlen(payload) >= sizeof(cmd.payload))
    {
        publishBusy(type, "invalidJson");
        return;
    }

    cmd.type = type;
    strcpy(cmd.payload, payload);

    if(xQueueSend(d._commandQueue, &cmd, 0) != pdTRUE)
    {
        publishBusy(type, "busy");
        return;
    }
    nukiTaskWake.wake();
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::onKeypadJsonCommandReceived(const char* value)
{
    enqueueCommand(NukiCommandType::KeypadJson, value);
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::onConfigUpdateReceived(const char* value)
{
    enqueueCommand(NukiCommandType::Config, value);
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::onTimeControlCommandReceived(const char* value)
{
    enqueueCommand(NukiCommandType::TimeControl, value);
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::onAuthCommandReceived(const char* value)
{
    enqueueCommand(NukiCommandType::Auth, value);
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled)
{
    char payload[NUKI_COMMAND_MAX_LENGTH];
    snprintf(payload, sizeof(payload), "%s\x1f%u\x1f%s\x1f%s\x1f%d", command, id, name.c_str(), code.c_str(), enabled);
    enqueueCommand(NukiCommandType::Keypad, payload);
}

template<typename TDerived, typename TTraits>
bool NukiCommandHandler<TDerived, TTraits>::hasQueuedCommands()
{
    TDerived& d = static_cast<TDerived&>(*this);
    return uxQueueMessagesWaiting(d._commandQueue) > 0;
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::processCommandQueue()
{
    TDerived& d = static_cast<TDerived&>(*this);
    NukiQueuedCommand cmd;

    if(xQueueReceive(d._commandQueue, &cmd, 0) != pdTRUE)
    {
        return;
    }

    switch(cmd.type)
    {
    case NukiCommandType::KeypadJson:
        executeKeypadJsonCommand(cmd.payload);
        break;
    case NukiCommandType::Config:
        d.executeConfigUpdate(cmd.payload);
        break;
    case NukiCommandType::TimeControl:
        executeTimeControlCommand(cmd.payload);
        break;
    case NukiCommandType::Auth:
        executeAuthCommand(cmd.payload);
        break;
    case NukiCommandType::Keypad:
    {
        char* fields[5] = {cmd.payload, nullptr, nullptr, nullptr, nullptr};
        int n = 1;
        for(char* p = cmd.payload; *p != '\0' && n < 5; p++)
        {
            if(*p == '\x1f')
            {
                *p = '\0';
                fields[n++] = p + 1;
            }
        }
        if(n == 5)
        {
            executeKeypadCommand(fields[0], (uint)strtoul(fields[1], nullptr, 10), String(fields[2]), String(fields[3]), atoi(fields[4]));
        }
        break;
    }
    }

    d.postponeBleWatchdog();
}

template<typename TDerived, typename TTraits>
NukiCommandContext NukiCommandHandler<TDerived, TTraits>::commandContext()
{
    TDerived& d = static_cast<TDerived&>(*this);

    return NukiCommandContext
    {
        &d,
        [](void* self) -> bool { return static_cast<TDerived*>(self)->isPinValid(); },
        [](void* self) -> bool { return static_cast<TDerived*>(self)->hasKeypad(); },
        [](void* self, bool retrieved) { static_cast<TDerived*>(self)->updateKeypad(retrieved); },
        [](void* self, bool retrieved) { static_cast<TDerived*>(self)->updateAuth(retrieved); },
        d.nukiDevice(),
        d._network,
        d._nukiRetryHandler,
        d._preferences,
        d._keypadCodeIds,
        d._keypadCodes,
        d._authIds,
        d._nrOfRetries,
        d._retryDelay,
        d._invalidCount,
        d._lastCodeCheck,
        d._nukiConfigValid,
        d._keypadEnabled,
        d._keypadCacheDirty,
        d._disableNonJSON,
        d._nextKeypadUpdateTs,
        d._keypadEntriesCache,
        d._keypadDeleted
    };
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::executeKeypadJsonCommand(const char *value)
{
    NukiCommandContext context = commandContext();
    NukiCommandCore::executeKeypadJsonCommand(context, value);
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::executeAuthCommand(const char *value)
{
    NukiCommandContext context = commandContext();
    NukiCommandCore::executeAuthCommand(context, value);
}

template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::executeTimeControlCommand(const char *value)
{
    TDerived& d = static_cast<TDerived&>(*this);

    if(!d._nukiConfigValid)
    {
        d._network->publishTimeControlCommandResult("configNotReady");
        return;
    }

    if(!d.isPinValid())
    {
        d._network->publishTimeControlCommandResult("noValidPinSet");
        return;
    }

    if(!d._preferences->getBool(preference_timecontrol_control_enabled))
    {
        d._network->publishTimeControlCommandResult("timeControlControlDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        d._network->publishTimeControlCommandResult("invalidJson");
        return;
    }

    const char *action = json["action"].as<const char*>();
    uint8_t entryId = json["entryId"].as<unsigned int>();
    uint8_t enabled;
    String weekdays;
    String time;
    String lockAction;
    typename TTraits::LockAction timeControlLockAction;

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["weekdays"].is<JsonVariant>())
    {
        weekdays = json["weekdays"].as<String>();
    }
    if(json["time"].is<JsonVariant>())
    {
        time = json["time"].as<String>();
    }
    if(json["lockAction"].is<JsonVariant>())
    {
        lockAction = json["lockAction"].as<String>();
    }

    if(lockAction.length() > 0)
    {
        timeControlLockAction = TTraits::lockActionToEnum(lockAction.c_str());

        if((int)timeControlLockAction == 0xff)
        {
            d._network->publishTimeControlCommandResult("invalidLockAction");
            return;
        }
    }

    if(action)
    {
        bool idExists = false;

        if(entryId)
        {
            idExists = std::find(d._timeControlIds.begin(), d._timeControlIds.end(), entryId) != d._timeControlIds.end();
        }

        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while(retryCount < d._nrOfRetries + 1)
        {
            if(strcmp(action, "delete") == 0)
            {
                if(idExists)
                {
                    result = d.nukiDevice().removeTimeControlEntry(entryId);
                    Log->print("Delete timecontrol: ");
                    TTraits::printCommandResult(result);
                }
                else
                {
                    d._network->publishTimeControlCommandResult("noExistingEntryIdSet");
                    return;
                }
            }
            else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
            {
                uint8_t timeHour;
                uint8_t timeMin;
                uint8_t weekdaysInt = 0;
                unsigned int timeAr[2];

                if(time.length() > 0)
                {
                    if(time.length() == 5)
                    {
                        timeAr[0] = (uint8_t)time.substring(0, 2).toInt();
                        timeAr[1] = (uint8_t)time.substring(3, 5).toInt();

                        if(timeAr[0] < 0 || timeAr[0] > 23 || timeAr[1] < 0 || timeAr[1] > 59)
                        {
                            d._network->publishTimeControlCommandResult("invalidTime");
                            return;
                        }
                    }
                    else
                    {
                        d._network->publishTimeControlCommandResult("invalidTime");
                        return;
                    }
                }

                if(weekdays.indexOf("mon") >= 0)
                {
                    weekdaysInt += 64;
                }
                if(weekdays.indexOf("tue") >= 0)
                {
                    weekdaysInt += 32;
                }
                if(weekdays.indexOf("wed") >= 0)
                {
                    weekdaysInt += 16;
                }
                if(weekdays.indexOf("thu") >= 0)
                {
                    weekdaysInt += 8;
                }
                if(weekdays.indexOf("fri") >= 0)
                {
                    weekdaysInt += 4;
                }
                if(weekdays.indexOf("sat") >= 0)
                {
                    weekdaysInt += 2;
                }
                if(weekdays.indexOf("sun") >= 0)
                {
                    weekdaysInt += 1;
                }

                if(strcmp(action, "add") == 0)
                {
                    typename TTraits::NewTimeControlEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    entry.weekdays = weekdaysInt;

                    if(time.length() > 0)
                    {
                        entry.timeHour = timeAr[0];
                        entry.timeMin = timeAr[1];
                    }

                    entry.lockAction = timeControlLockAction;

                    result = d.nukiDevice().addTimeControlEntry(entry);
                    Log->print("Add timecontrol: ");
                    TTraits::printCommandResult(result);
                }
                else if (strcmp(action, "update") == 0)
                {
                    if(!idExists)
                    {
                        d._network->publishTimeControlCommandResult("noExistingEntryIdSet");
                        return;
                    }

                    Nuki::CmdResult resultTc = d._nukiRetryHandler->retryComm([&]()
                    {
                        return d.nukiDevice().retrieveTimeControlEntries();
                    });
                    bool foundExisting = false;

                    if(resultTc == Nuki::CmdResult::Success)
                    {
                        espDelayAck(5000);
                        std::list<typename TTraits::TimeControlEntry> timeControlEntries;
                        d.nukiDevice().getTimeControlEntries(&timeControlEntries);

                        for(const auto& entry : timeControlEntries)
                        {
                            if (entryId != entry.entryId)
                            {
                                continue;
                            }
                            else
                            {
                                foundExisting = true;
                            }

                            if(enabled == 2)
                            {
                                enabled = entry.enabled;
                            }
                            if(weekdays.length() < 1)
                            {
                                weekdaysInt = entry.weekdays;
                            }
                            if(time.length() < 1)
                            {
                                time = "old";
                                timeAr[0] = entry.timeHour;
                                timeAr[1] = entry.timeMin;
                            }
                            if(lockAction.length() < 1)
                            {
                                timeControlLockAction = entry.lockAction;
                            }
                        }

                        if(!foundExisting)
                        {
                            d._network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
                            return;
                        }
                    }
                    else
                    {
                        Log->print("Update timecontrol: failed to retrieve existing entries: ");
                        TTraits::printCommandResult(resultTc);
                        d._network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
                        return;
                    }

                    typename TTraits::TimeControlEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    entry.entryId = entryId;
                    entry.enabled = enabled;
                    entry.weekdays = weekdaysInt;

                    if(time.length() > 0)
                    {
                        entry.timeHour = timeAr[0];
                        entry.timeMin = timeAr[1];
                    }

                    entry.lockAction = timeControlLockAction;

                    result = d.nukiDevice().updateTimeControlEntry(entry);
                    Log->print("Update timecontrol: ");
                    TTraits::printCommandResult(result);
                }
            }
            else
            {
                d._network->publishTimeControlCommandResult("invalidAction");
                return;
            }

            if(result == Nuki::CmdResult::Success || result == Nuki::CmdResult::Failed || result == Nuki::CmdResult::NotPaired)
            {
                break;
            }
            ++retryCount;
            if(retryCount <= d._nrOfRetries)
            {
                espDelayAck(d._retryDelay);
            }
        }

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            TTraits::cmdResultToString(result, resultStr);
            d._network->publishTimeControlCommandResult(resultStr);
        }

        d._nextConfigUpdateTs = espMillis() + 300;
    }
    else
    {
        d._network->publishTimeControlCommandResult("noActionSet");
        return;
    }
}
template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::executeKeypadCommand(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    NukiCommandContext context = commandContext();
    NukiCommandCore::executeKeypadCommand(context, command, id, name, code, enabled);
}
