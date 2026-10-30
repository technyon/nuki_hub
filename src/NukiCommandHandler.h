#pragma once

#include <ArduinoJson.h>
#include <list>
#include <vector>
#include "NukiDataTypes.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "EspMillis.h"
#include "Config.h"
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
    }
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
void NukiCommandHandler<TDerived, TTraits>::executeKeypadJsonCommand(const char *value)
{
    TDerived& d = static_cast<TDerived&>(*this);

    if(!d.isPinValid())
    {
        d._network->publishKeypadJsonCommandResult("noValidPinSet");
        return;
    }

    if(!d._preferences->getBool(preference_keypad_control_enabled))
    {
        d._network->publishKeypadJsonCommandResult("keypadControlDisabled");
        return;
    }

    if(!d.hasKeypad())
    {
        if(d._nukiConfigValid)
        {
            d._network->publishKeypadJsonCommandResult("keypadNotAvailable");
            return;
        }

        d._network->publishKeypadJsonCommandResult("configNotReady");
        return;
    }

    if(!d._keypadEnabled)
    {
        d._network->publishKeypadJsonCommandResult("keypadDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        d._network->publishKeypadJsonCommandResult("invalidJson");
        return;
    }

    char oldName[21];
    const char *action = json["action"].as<const char*>();
    uint16_t codeId = json["codeId"].as<unsigned int>();
    uint32_t code;
    uint8_t enabled;
    uint8_t timeLimited;
    String name;
    String allowedFrom;
    String allowedUntil;
    String allowedWeekdays;
    String allowedFromTime;
    String allowedUntilTime;

    if(json["code"].is<JsonVariant>())
    {
        code = json["code"].as<unsigned int>();
    }
    else
    {
        code = 12;
    }

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["timeLimited"].is<JsonVariant>())
    {
        timeLimited = json["timeLimited"].as<unsigned int>();
    }
    else
    {
        timeLimited = 2;
    }

    if(json["name"].is<JsonVariant>())
    {
        name = json["name"].as<String>();
    }
    if(json["allowedFrom"].is<JsonVariant>())
    {
        allowedFrom = json["allowedFrom"].as<String>();
    }
    if(json["allowedUntil"].is<JsonVariant>())
    {
        allowedUntil = json["allowedUntil"].as<String>();
    }
    if(json["allowedWeekdays"].is<JsonVariant>())
    {
        allowedWeekdays = json["allowedWeekdays"].as<String>();
    }
    if(json["allowedFromTime"].is<JsonVariant>())
    {
        allowedFromTime = json["allowedFromTime"].as<String>();
    }
    if(json["allowedUntilTime"].is<JsonVariant>())
    {
        allowedUntilTime = json["allowedUntilTime"].as<String>();
    }

    if(action)
    {
        bool idExists = false;

        if(codeId)
        {
            idExists = std::find(d._keypadCodeIds.begin(), d._keypadCodeIds.end(), codeId) != d._keypadCodeIds.end();
        }

        if(strcmp(action, "check") == 0)
        {
            if(!d._preferences->getBool(preference_keypad_check_code_enabled, false))
            {
                d._network->publishKeypadJsonCommandResult("checkingKeypadCodesDisabled");
                return;
            }

            if((pow(d._invalidCount, 5) + d._lastCodeCheck) > espMillis())
            {
                d._network->publishKeypadJsonCommandResult("checkingCodesBlockedTooManyInvalid");
                d._lastCodeCheck = espMillis();
                return;
            }

            d._lastCodeCheck = espMillis();

            if(idExists)
            {
                auto it1 = std::find(d._keypadCodeIds.begin(), d._keypadCodeIds.end(), codeId);
                int index = it1 - d._keypadCodeIds.begin();
                Log->print("Check keypad code: ");

                if(code == d._keypadCodes[index])
                {
                    d._invalidCount = 0;
                    d._network->publishKeypadJsonCommandResult("codeValid");
                    Log->println("Valid");
                    return;
                }
                else
                {
                    d._invalidCount++;
                    d._network->publishKeypadJsonCommandResult("codeInvalid");
                    Log->print("Invalid\nInvalid count: ");
                    Log->println(d._invalidCount);
                    return;
                }
            }
            else
            {
                d._invalidCount++;
                d._network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                Log->print("Invalid count: ");
                Log->println(d._invalidCount);
                return;
            }
        }
        else
        {

            Nuki::CmdResult result = (Nuki::CmdResult)-1;
            int retryCount = 0;

            while(retryCount < d._nrOfRetries + 1)
            {
                if(strcmp(action, "delete") == 0)
                {
                    if(idExists)
                    {
                        result = d._nukiRetryHandler->retryComm([&]()
                        {
                            return d.nukiDevice().deleteKeypadEntry(codeId);
                        });
                        if(result == Nuki::CmdResult::Success)
                        {
                            d._keypadDeleted.push_back(std::make_pair(codeId, espMillis()));
                            auto it = d._keypadEntriesCache.begin();
                            while(it != d._keypadEntriesCache.end())
                            {
                                if(it->codeId == codeId)
                                {
                                    it = d._keypadEntriesCache.erase(it);
                                }
                                else
                                {
                                    ++it;
                                }
                            }
                            d._keypadCacheDirty = true;
                        }
                        Log->print("Delete keypad code: ");
                        TTraits::printCommandResult(result);
                    }
                    else
                    {
                        d._network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                        return;
                    }
                }
                else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
                {
                    if(name.length() < 1)
                    {
                        if (strcmp(action, "update") != 0)
                        {
                            d._network->publishKeypadJsonCommandResult("noNameSet");
                            return;
                        }
                    }

                    if(code != 12)
                    {
                        String codeStr = json["code"].as<String>();
                        bool codeValid = code > 100000 && code < 1000000 && (codeStr.indexOf('0') == -1);

                        if (!codeValid)
                        {
                            d._network->publishKeypadJsonCommandResult("noValidCodeSet");
                            return;
                        }
                    }
                    else if (strcmp(action, "update") != 0)
                    {
                        d._network->publishKeypadJsonCommandResult("noCodeSet");
                        return;
                    }

                    unsigned int allowedFromAr[6];
                    unsigned int allowedUntilAr[6];
                    unsigned int allowedFromTimeAr[2];
                    unsigned int allowedUntilTimeAr[2];
                    uint8_t allowedWeekdaysInt = 0;

                    if(timeLimited == 1)
                    {
                        if(allowedFrom.length() > 0)
                        {
                            if(allowedFrom.length() == 19)
                            {
                                allowedFromAr[0] = (uint16_t)allowedFrom.substring(0, 4).toInt();
                                allowedFromAr[1] = (uint8_t)allowedFrom.substring(5, 7).toInt();
                                allowedFromAr[2] = (uint8_t)allowedFrom.substring(8, 10).toInt();
                                allowedFromAr[3] = (uint8_t)allowedFrom.substring(11, 13).toInt();
                                allowedFromAr[4] = (uint8_t)allowedFrom.substring(14, 16).toInt();
                                allowedFromAr[5] = (uint8_t)allowedFrom.substring(17, 19).toInt();

                                if(allowedFromAr[0] < 2000 || allowedFromAr[0] > 3000 || allowedFromAr[1] < 1 || allowedFromAr[1] > 12 || allowedFromAr[2] < 1 || allowedFromAr[2] > 31 || allowedFromAr[3] < 0 || allowedFromAr[3] > 23 || allowedFromAr[4] < 0 || allowedFromAr[4] > 59 || allowedFromAr[5] < 0 || allowedFromAr[5] > 59)
                                {
                                    d._network->publishKeypadJsonCommandResult("invalidAllowedFrom");
                                    return;
                                }
                            }
                            else
                            {
                                d._network->publishKeypadJsonCommandResult("invalidAllowedFrom");
                                return;
                            }
                        }

                        if(allowedUntil.length() > 0)
                        {
                            if(allowedUntil.length() == 19)
                            {
                                allowedUntilAr[0] = (uint16_t)allowedUntil.substring(0, 4).toInt();
                                allowedUntilAr[1] = (uint8_t)allowedUntil.substring(5, 7).toInt();
                                allowedUntilAr[2] = (uint8_t)allowedUntil.substring(8, 10).toInt();
                                allowedUntilAr[3] = (uint8_t)allowedUntil.substring(11, 13).toInt();
                                allowedUntilAr[4] = (uint8_t)allowedUntil.substring(14, 16).toInt();
                                allowedUntilAr[5] = (uint8_t)allowedUntil.substring(17, 19).toInt();

                                if(allowedUntilAr[0] < 2000 || allowedUntilAr[0] > 3000 || allowedUntilAr[1] < 1 || allowedUntilAr[1] > 12 || allowedUntilAr[2] < 1 || allowedUntilAr[2] > 31 || allowedUntilAr[3] < 0 || allowedUntilAr[3] > 23 || allowedUntilAr[4] < 0 || allowedUntilAr[4] > 59 || allowedUntilAr[5] < 0 || allowedUntilAr[5] > 59)
                                {
                                    d._network->publishKeypadJsonCommandResult("invalidAllowedUntil");
                                    return;
                                }
                            }
                            else
                            {
                                d._network->publishKeypadJsonCommandResult("invalidAllowedUntil");
                                return;
                            }
                        }

                        if(allowedFromTime.length() > 0)
                        {
                            if(allowedFromTime.length() == 5)
                            {
                                allowedFromTimeAr[0] = (uint8_t)allowedFromTime.substring(0, 2).toInt();
                                allowedFromTimeAr[1] = (uint8_t)allowedFromTime.substring(3, 5).toInt();

                                if(allowedFromTimeAr[0] < 0 || allowedFromTimeAr[0] > 23 || allowedFromTimeAr[1] < 0 || allowedFromTimeAr[1] > 59)
                                {
                                    d._network->publishKeypadJsonCommandResult("invalidAllowedFromTime");
                                    return;
                                }
                            }
                            else
                            {
                                d._network->publishKeypadJsonCommandResult("invalidAllowedFromTime");
                                return;
                            }
                        }

                        if(allowedUntilTime.length() > 0)
                        {
                            if(allowedUntilTime.length() == 5)
                            {
                                allowedUntilTimeAr[0] = (uint8_t)allowedUntilTime.substring(0, 2).toInt();
                                allowedUntilTimeAr[1] = (uint8_t)allowedUntilTime.substring(3, 5).toInt();

                                if(allowedUntilTimeAr[0] < 0 || allowedUntilTimeAr[0] > 23 || allowedUntilTimeAr[1] < 0 || allowedUntilTimeAr[1] > 59)
                                {
                                    d._network->publishKeypadJsonCommandResult("invalidAllowedUntilTime");
                                    return;
                                }
                            }
                            else
                            {
                                d._network->publishKeypadJsonCommandResult("invalidAllowedUntilTime");
                                return;
                            }
                        }

                        if(allowedWeekdays.indexOf("mon") >= 0)
                        {
                            allowedWeekdaysInt += 64;
                        }
                        if(allowedWeekdays.indexOf("tue") >= 0)
                        {
                            allowedWeekdaysInt += 32;
                        }
                        if(allowedWeekdays.indexOf("wed") >= 0)
                        {
                            allowedWeekdaysInt += 16;
                        }
                        if(allowedWeekdays.indexOf("thu") >= 0)
                        {
                            allowedWeekdaysInt += 8;
                        }
                        if(allowedWeekdays.indexOf("fri") >= 0)
                        {
                            allowedWeekdaysInt += 4;
                        }
                        if(allowedWeekdays.indexOf("sat") >= 0)
                        {
                            allowedWeekdaysInt += 2;
                        }
                        if(allowedWeekdays.indexOf("sun") >= 0)
                        {
                            allowedWeekdaysInt += 1;
                        }
                    }

                    if(strcmp(action, "add") == 0)
                    {
                        typename TTraits::NewKeypadEntry entry;
                        memset(&entry, 0, sizeof(entry));
                        size_t nameLen = name.length();
                        memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                        entry.code = code;
                        entry.timeLimited = timeLimited == 1 ? 1 : 0;

                        if(allowedFrom.length() > 0)
                        {
                            entry.allowedFromYear = allowedFromAr[0];
                            entry.allowedFromMonth = allowedFromAr[1];
                            entry.allowedFromDay = allowedFromAr[2];
                            entry.allowedFromHour = allowedFromAr[3];
                            entry.allowedFromMin = allowedFromAr[4];
                            entry.allowedFromSec = allowedFromAr[5];
                        }

                        if(allowedUntil.length() > 0)
                        {
                            entry.allowedUntilYear = allowedUntilAr[0];
                            entry.allowedUntilMonth = allowedUntilAr[1];
                            entry.allowedUntilDay = allowedUntilAr[2];
                            entry.allowedUntilHour = allowedUntilAr[3];
                            entry.allowedUntilMin = allowedUntilAr[4];
                            entry.allowedUntilSec = allowedUntilAr[5];
                        }

                        entry.allowedWeekdays = allowedWeekdaysInt;

                        if(allowedFromTime.length() > 0)
                        {
                            entry.allowedFromTimeHour = allowedFromTimeAr[0];
                            entry.allowedFromTimeMin = allowedFromTimeAr[1];
                        }

                        if(allowedUntilTime.length() > 0)
                        {
                            entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                            entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                        }

                        result = d._nukiRetryHandler->retryComm([&]()
                        {
                            return d.nukiDevice().addKeypadEntry(entry);
                        });
                        Log->print("Add keypad code: ");
                        TTraits::printCommandResult(result);
                    }
                    else if (strcmp(action, "update") == 0)
                    {
                        if(!codeId)
                        {
                            d._network->publishKeypadJsonCommandResult("noCodeIdSet");
                            return;
                        }

                        if(!idExists)
                        {
                            d._network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                            return;
                        }

                        std::list<typename TTraits::KeypadEntry> entries;
                        d.nukiDevice().getKeypadEntries(&entries);
                        bool cachedEntry = false;
                        for(const auto& entry : entries)
                        {
                            if(entry.codeId == codeId)
                            {
                                cachedEntry = true;
                                break;
                            }
                        }
                        Nuki::CmdResult resultKp = Nuki::CmdResult::Success;
                        if(!cachedEntry)
                        {
                            resultKp = d._nukiRetryHandler->retryComm([&]()
                            {
                                return d.nukiDevice().retrieveKeypadEntries(0, d._preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
                            });
                        }
                        bool foundExisting = false;

                        if(resultKp == Nuki::CmdResult::Success)
                        {
                            if(!cachedEntry)
                            {
                                d.nukiDevice().getKeypadEntries(&entries);
                            }

                            for(const auto& entry : entries)
                            {
                                if (codeId != entry.codeId)
                                {
                                    continue;
                                }
                                else
                                {
                                    foundExisting = true;
                                }

                                if(name.length() < 1)
                                {
                                    memset(oldName, 0, sizeof(oldName));
                                    memcpy(oldName, entry.name, sizeof(entry.name));
                                }
                                if(code == 12)
                                {
                                    code = entry.code;
                                }
                                if(enabled == 2)
                                {
                                    enabled = entry.enabled;
                                }
                                if(timeLimited == 2)
                                {
                                    timeLimited = entry.timeLimited;
                                }
                                if(allowedFrom.length() < 1)
                                {
                                    allowedFrom = "old";
                                    allowedFromAr[0] = entry.allowedFromYear;
                                    allowedFromAr[1] = entry.allowedFromMonth;
                                    allowedFromAr[2] = entry.allowedFromDay;
                                    allowedFromAr[3] = entry.allowedFromHour;
                                    allowedFromAr[4] = entry.allowedFromMin;
                                    allowedFromAr[5] = entry.allowedFromSec;
                                }
                                if(allowedUntil.length() < 1)
                                {
                                    allowedUntil = "old";
                                    allowedUntilAr[0] = entry.allowedUntilYear;
                                    allowedUntilAr[1] = entry.allowedUntilMonth;
                                    allowedUntilAr[2] = entry.allowedUntilDay;
                                    allowedUntilAr[3] = entry.allowedUntilHour;
                                    allowedUntilAr[4] = entry.allowedUntilMin;
                                    allowedUntilAr[5] = entry.allowedUntilSec;
                                }
                                if(allowedWeekdays.length() < 1)
                                {
                                    allowedWeekdaysInt = entry.allowedWeekdays;
                                }
                                if(allowedFromTime.length() < 1)
                                {
                                    allowedFromTime = "old";
                                    allowedFromTimeAr[0] = entry.allowedFromTimeHour;
                                    allowedFromTimeAr[1] = entry.allowedFromTimeMin;
                                }

                                if(allowedUntilTime.length() < 1)
                                {
                                    allowedUntilTime = "old";
                                    allowedUntilTimeAr[0] = entry.allowedUntilTimeHour;
                                    allowedUntilTimeAr[1] = entry.allowedUntilTimeMin;
                                }

                            }

                            if(!foundExisting)
                            {
                                d._network->publishKeypadJsonCommandResult("failedToRetrieveExistingKeypadEntry");
                                return;
                            }
                        }
                        else
                        {
                            d._network->publishKeypadJsonCommandResult("failedToRetrieveExistingKeypadEntry");
                            return;
                        }

                        typename TTraits::UpdatedKeypadEntry entry;

                        memset(&entry, 0, sizeof(entry));
                        entry.codeId = codeId;
                        entry.code = code;

                        if(name.length() < 1)
                        {
                            size_t nameLen = strlen(oldName);
                            memcpy(&entry.name, oldName, nameLen > 20 ? 20 : nameLen);
                        }
                        else
                        {
                            size_t nameLen = name.length();
                            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                        }
                        entry.enabled = enabled;
                        entry.timeLimited = timeLimited;

                        if(timeLimited == 1)
                        {
                            if(allowedFrom.length() > 0)
                            {
                                entry.allowedFromYear = allowedFromAr[0];
                                entry.allowedFromMonth = allowedFromAr[1];
                                entry.allowedFromDay = allowedFromAr[2];
                                entry.allowedFromHour = allowedFromAr[3];
                                entry.allowedFromMin = allowedFromAr[4];
                                entry.allowedFromSec = allowedFromAr[5];
                            }

                            if(allowedUntil.length() > 0)
                            {
                                entry.allowedUntilYear = allowedUntilAr[0];
                                entry.allowedUntilMonth = allowedUntilAr[1];
                                entry.allowedUntilDay = allowedUntilAr[2];
                                entry.allowedUntilHour = allowedUntilAr[3];
                                entry.allowedUntilMin = allowedUntilAr[4];
                                entry.allowedUntilSec = allowedUntilAr[5];
                            }

                            entry.allowedWeekdays = allowedWeekdaysInt;

                            if(allowedFromTime.length() > 0)
                            {
                                entry.allowedFromTimeHour = allowedFromTimeAr[0];
                                entry.allowedFromTimeMin = allowedFromTimeAr[1];
                            }

                            if(allowedUntilTime.length() > 0)
                            {
                                entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                                entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                            }
                        }

                        result = d._nukiRetryHandler->retryComm([&]()
                        {
                            return d.nukiDevice().updateKeypadEntry(entry);
                        });
                        if(result == Nuki::CmdResult::Success)
                        {
                            for(auto& e : d._keypadEntriesCache)
                            {
                                if(e.codeId != entry.codeId)
                                {
                                    continue;
                                }
                                e.code = entry.code;
                                e.enabled = entry.enabled;
                                e.timeLimited = entry.timeLimited;
                                e.allowedFromYear = entry.allowedFromYear;
                                e.allowedFromMonth = entry.allowedFromMonth;
                                e.allowedFromDay = entry.allowedFromDay;
                                e.allowedFromHour = entry.allowedFromHour;
                                e.allowedFromMin = entry.allowedFromMin;
                                e.allowedFromSec = entry.allowedFromSec;
                                e.allowedUntilYear = entry.allowedUntilYear;
                                e.allowedUntilMonth = entry.allowedUntilMonth;
                                e.allowedUntilDay = entry.allowedUntilDay;
                                e.allowedUntilHour = entry.allowedUntilHour;
                                e.allowedUntilMin = entry.allowedUntilMin;
                                e.allowedUntilSec = entry.allowedUntilSec;
                                e.allowedWeekdays = entry.allowedWeekdays;
                                e.allowedFromTimeHour = entry.allowedFromTimeHour;
                                e.allowedFromTimeMin = entry.allowedFromTimeMin;
                                e.allowedUntilTimeHour = entry.allowedUntilTimeHour;
                                e.allowedUntilTimeMin = entry.allowedUntilTimeMin;
                                memcpy(e.name, entry.name, sizeof(e.name));
                                d._keypadCacheDirty = true;
                            }
                        }
                        Log->print("Update keypad code: ");
                        TTraits::printCommandResult(result);
                    }
                }
                else
                {
                    d._network->publishKeypadJsonCommandResult("invalidAction");
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
                d._network->publishKeypadJsonCommandResult(resultStr);
            }
            d._nextKeypadUpdateTs = espMillis() + 3000;
        }
    }
    else
    {
        d._network->publishKeypadJsonCommandResult("noActionSet");
        return;
    }
}
template<typename TDerived, typename TTraits>
void NukiCommandHandler<TDerived, TTraits>::executeAuthCommand(const char *value)
{
    TDerived& d = static_cast<TDerived&>(*this);

    if(!d._nukiConfigValid)
    {
        d._network->publishAuthCommandResult("configNotReady");
        return;
    }

    if(!d.isPinValid())
    {
        d._network->publishAuthCommandResult("noValidPinSet");
        return;
    }

    if(!d._preferences->getBool(preference_auth_control_enabled))
    {
        d._network->publishAuthCommandResult("keypadControlDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        d._network->publishAuthCommandResult("invalidJson");
        return;
    }

    char oldName[33];
    const char *action = json["action"].as<const char*>();
    uint32_t authId = json["authId"].as<unsigned int>();
    //uint8_t idType = json["idType"].as<unsigned int>();
    //unsigned char secretKeyK[32] = {0x00};
    uint8_t remoteAllowed;
    uint8_t enabled;
    uint8_t timeLimited;
    String name;
    //String sharedKey;
    String allowedFrom;
    String allowedUntil;
    String allowedWeekdays;
    String allowedFromTime;
    String allowedUntilTime;

    if(json["remoteAllowed"].is<JsonVariant>())
    {
        remoteAllowed = json["remoteAllowed"].as<unsigned int>();
    }
    else
    {
        remoteAllowed = 2;
    }

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["timeLimited"].is<JsonVariant>())
    {
        timeLimited = json["timeLimited"].as<unsigned int>();
    }
    else
    {
        timeLimited = 2;
    }

    if(json["name"].is<JsonVariant>())
    {
        name = json["name"].as<String>();
    }
    //if(json["sharedKey"].is<JsonVariant>()) sharedKey = json["sharedKey"].as<String>();
    if(json["allowedFrom"].is<JsonVariant>())
    {
        allowedFrom = json["allowedFrom"].as<String>();
    }
    if(json["allowedUntil"].is<JsonVariant>())
    {
        allowedUntil = json["allowedUntil"].as<String>();
    }
    if(json["allowedWeekdays"].is<JsonVariant>())
    {
        allowedWeekdays = json["allowedWeekdays"].as<String>();
    }
    if(json["allowedFromTime"].is<JsonVariant>())
    {
        allowedFromTime = json["allowedFromTime"].as<String>();
    }
    if(json["allowedUntilTime"].is<JsonVariant>())
    {
        allowedUntilTime = json["allowedUntilTime"].as<String>();
    }

    if(action)
    {
        bool idExists = false;

        if(authId)
        {
            idExists = std::find(d._authIds.begin(), d._authIds.end(), authId) != d._authIds.end();
        }

        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while(retryCount < d._nrOfRetries)
        {
            if(strcmp(action, "delete") == 0)
            {
                if(idExists)
                {
                    result = d.nukiDevice().deleteAuthorizationEntry(authId);
                    espDelayAck(250);
                    Log->print("Delete authorization: ");
                    TTraits::printCommandResult(result);
                }
                else
                {
                    d._network->publishAuthCommandResult("noExistingAuthIdSet");
                    return;
                }
            }
            else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
            {
                if(name.length() < 1)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        d._network->publishAuthCommandResult("noNameSet");
                        return;
                    }
                }

                /*
                if(sharedKey.length() != 64)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        d._network->publishAuthCommandResult("noSharedKeySet");
                        return;
                    }
                }
                else
                {
                    for(int i=0; i<sharedKey.length();i+=2) secretKeyK[(i/2)] = std::stoi(sharedKey.substring(i, i+2).c_str(), nullptr, 16);
                }
                */

                unsigned int allowedFromAr[6];
                unsigned int allowedUntilAr[6];
                unsigned int allowedFromTimeAr[2];
                unsigned int allowedUntilTimeAr[2];
                uint8_t allowedWeekdaysInt = 0;

                if(timeLimited == 1)
                {
                    if(allowedFrom.length() > 0)
                    {
                        if(allowedFrom.length() == 19)
                        {
                            allowedFromAr[0] = (uint16_t)allowedFrom.substring(0, 4).toInt();
                            allowedFromAr[1] = (uint8_t)allowedFrom.substring(5, 7).toInt();
                            allowedFromAr[2] = (uint8_t)allowedFrom.substring(8, 10).toInt();
                            allowedFromAr[3] = (uint8_t)allowedFrom.substring(11, 13).toInt();
                            allowedFromAr[4] = (uint8_t)allowedFrom.substring(14, 16).toInt();
                            allowedFromAr[5] = (uint8_t)allowedFrom.substring(17, 19).toInt();

                            if(allowedFromAr[0] < 2000 || allowedFromAr[0] > 3000 || allowedFromAr[1] < 1 || allowedFromAr[1] > 12 || allowedFromAr[2] < 1 || allowedFromAr[2] > 31 || allowedFromAr[3] < 0 || allowedFromAr[3] > 23 || allowedFromAr[4] < 0 || allowedFromAr[4] > 59 || allowedFromAr[5] < 0 || allowedFromAr[5] > 59)
                            {
                                d._network->publishAuthCommandResult("invalidAllowedFrom");
                                return;
                            }
                        }
                        else
                        {
                            d._network->publishAuthCommandResult("invalidAllowedFrom");
                            return;
                        }
                    }

                    if(allowedUntil.length() > 0)
                    {
                        if(allowedUntil.length() == 19)
                        {
                            allowedUntilAr[0] = (uint16_t)allowedUntil.substring(0, 4).toInt();
                            allowedUntilAr[1] = (uint8_t)allowedUntil.substring(5, 7).toInt();
                            allowedUntilAr[2] = (uint8_t)allowedUntil.substring(8, 10).toInt();
                            allowedUntilAr[3] = (uint8_t)allowedUntil.substring(11, 13).toInt();
                            allowedUntilAr[4] = (uint8_t)allowedUntil.substring(14, 16).toInt();
                            allowedUntilAr[5] = (uint8_t)allowedUntil.substring(17, 19).toInt();

                            if(allowedUntilAr[0] < 2000 || allowedUntilAr[0] > 3000 || allowedUntilAr[1] < 1 || allowedUntilAr[1] > 12 || allowedUntilAr[2] < 1 || allowedUntilAr[2] > 31 || allowedUntilAr[3] < 0 || allowedUntilAr[3] > 23 || allowedUntilAr[4] < 0 || allowedUntilAr[4] > 59 || allowedUntilAr[5] < 0 || allowedUntilAr[5] > 59)
                            {
                                d._network->publishAuthCommandResult("invalidAllowedUntil");
                                return;
                            }
                        }
                        else
                        {
                            d._network->publishAuthCommandResult("invalidAllowedUntil");
                            return;
                        }
                    }

                    if(allowedFromTime.length() > 0)
                    {
                        if(allowedFromTime.length() == 5)
                        {
                            allowedFromTimeAr[0] = (uint8_t)allowedFromTime.substring(0, 2).toInt();
                            allowedFromTimeAr[1] = (uint8_t)allowedFromTime.substring(3, 5).toInt();

                            if(allowedFromTimeAr[0] < 0 || allowedFromTimeAr[0] > 23 || allowedFromTimeAr[1] < 0 || allowedFromTimeAr[1] > 59)
                            {
                                d._network->publishAuthCommandResult("invalidAllowedFromTime");
                                return;
                            }
                        }
                        else
                        {
                            d._network->publishAuthCommandResult("invalidAllowedFromTime");
                            return;
                        }
                    }

                    if(allowedUntilTime.length() > 0)
                    {
                        if(allowedUntilTime.length() == 5)
                        {
                            allowedUntilTimeAr[0] = (uint8_t)allowedUntilTime.substring(0, 2).toInt();
                            allowedUntilTimeAr[1] = (uint8_t)allowedUntilTime.substring(3, 5).toInt();

                            if(allowedUntilTimeAr[0] < 0 || allowedUntilTimeAr[0] > 23 || allowedUntilTimeAr[1] < 0 || allowedUntilTimeAr[1] > 59)
                            {
                                d._network->publishAuthCommandResult("invalidAllowedUntilTime");
                                return;
                            }
                        }
                        else
                        {
                            d._network->publishAuthCommandResult("invalidAllowedUntilTime");
                            return;
                        }
                    }

                    if(allowedWeekdays.indexOf("mon") >= 0)
                    {
                        allowedWeekdaysInt += 64;
                    }
                    if(allowedWeekdays.indexOf("tue") >= 0)
                    {
                        allowedWeekdaysInt += 32;
                    }
                    if(allowedWeekdays.indexOf("wed") >= 0)
                    {
                        allowedWeekdaysInt += 16;
                    }
                    if(allowedWeekdays.indexOf("thu") >= 0)
                    {
                        allowedWeekdaysInt += 8;
                    }
                    if(allowedWeekdays.indexOf("fri") >= 0)
                    {
                        allowedWeekdaysInt += 4;
                    }
                    if(allowedWeekdays.indexOf("sat") >= 0)
                    {
                        allowedWeekdaysInt += 2;
                    }
                    if(allowedWeekdays.indexOf("sun") >= 0)
                    {
                        allowedWeekdaysInt += 1;
                    }
                }

                if(strcmp(action, "add") == 0)
                {
                    d._network->publishAuthCommandResult("addActionNotSupported");
                    return;

                    typename TTraits::NewAuthorizationEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    size_t nameLen = name.length();
                    memcpy(&entry.name, name.c_str(), nameLen > 32 ? 32 : nameLen);
                    /*
                    memcpy(&entry.sharedKey, secretKeyK, 32);

                    if(idType != 1)
                    {
                        d._network->publishAuthCommandResult("invalidIdType");
                        return;
                    }

                    entry.idType = idType;
                    */
                    entry.remoteAllowed = remoteAllowed == 1 ? 1 : 0;
                    entry.timeLimited = timeLimited == 1 ? 1 : 0;

                    if(allowedFrom.length() > 0)
                    {
                        entry.allowedFromYear = allowedFromAr[0];
                        entry.allowedFromMonth = allowedFromAr[1];
                        entry.allowedFromDay = allowedFromAr[2];
                        entry.allowedFromHour = allowedFromAr[3];
                        entry.allowedFromMinute = allowedFromAr[4];
                        entry.allowedFromSecond = allowedFromAr[5];
                    }

                    if(allowedUntil.length() > 0)
                    {
                        entry.allowedUntilYear = allowedUntilAr[0];
                        entry.allowedUntilMonth = allowedUntilAr[1];
                        entry.allowedUntilDay = allowedUntilAr[2];
                        entry.allowedUntilHour = allowedUntilAr[3];
                        entry.allowedUntilMinute = allowedUntilAr[4];
                        entry.allowedUntilSecond = allowedUntilAr[5];
                    }

                    entry.allowedWeekdays = allowedWeekdaysInt;

                    if(allowedFromTime.length() > 0)
                    {
                        entry.allowedFromTimeHour = allowedFromTimeAr[0];
                        entry.allowedFromTimeMin = allowedFromTimeAr[1];
                    }

                    if(allowedUntilTime.length() > 0)
                    {
                        entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                        entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                    }

                    result = d.nukiDevice().addAuthorizationEntry(entry);
                    espDelayAck(250);
                    Log->print("Add authorization: ");
                    TTraits::printCommandResult(result);
                }
                else if (strcmp(action, "update") == 0)
                {
                    if(!authId)
                    {
                        d._network->publishAuthCommandResult("noAuthIdSet");
                        return;
                    }

                    if(!idExists)
                    {
                        d._network->publishAuthCommandResult("noExistingAuthIdSet");
                        return;
                    }

                    Nuki::CmdResult resultAuth = d.nukiDevice().retrieveAuthorizationEntries(0, d._preferences->getInt(preference_auth_max_entries, MAX_AUTH));
                    bool foundExisting = false;

                    if(resultAuth == Nuki::CmdResult::Success)
                    {
                        espDelayAck(5000);
                        std::list<typename TTraits::AuthorizationEntry> entries;
                        d.nukiDevice().getAuthorizationEntries(&entries);

                        for(const auto& entry : entries)
                        {
                            if (authId != entry.authId)
                            {
                                continue;
                            }
                            else
                            {
                                foundExisting = true;
                            }

                            if(name.length() < 1)
                            {
                                memset(oldName, 0, sizeof(oldName));
                                memcpy(oldName, entry.name, sizeof(entry.name));
                            }
                            if(remoteAllowed == 2)
                            {
                                remoteAllowed = entry.remoteAllowed;
                            }
                            if(enabled == 2)
                            {
                                enabled = entry.enabled;
                            }
                            if(timeLimited == 2)
                            {
                                timeLimited = entry.timeLimited;
                            }
                            if(allowedFrom.length() < 1)
                            {
                                allowedFrom = "old";
                                allowedFromAr[0] = entry.allowedFromYear;
                                allowedFromAr[1] = entry.allowedFromMonth;
                                allowedFromAr[2] = entry.allowedFromDay;
                                allowedFromAr[3] = entry.allowedFromHour;
                                allowedFromAr[4] = entry.allowedFromMinute;
                                allowedFromAr[5] = entry.allowedFromSecond;
                            }
                            if(allowedUntil.length() < 1)
                            {
                                allowedUntil = "old";
                                allowedUntilAr[0] = entry.allowedUntilYear;
                                allowedUntilAr[1] = entry.allowedUntilMonth;
                                allowedUntilAr[2] = entry.allowedUntilDay;
                                allowedUntilAr[3] = entry.allowedUntilHour;
                                allowedUntilAr[4] = entry.allowedUntilMinute;
                                allowedUntilAr[5] = entry.allowedUntilSecond;
                            }
                            if(allowedWeekdays.length() < 1)
                            {
                                allowedWeekdaysInt = entry.allowedWeekdays;
                            }
                            if(allowedFromTime.length() < 1)
                            {
                                allowedFromTime = "old";
                                allowedFromTimeAr[0] = entry.allowedFromTimeHour;
                                allowedFromTimeAr[1] = entry.allowedFromTimeMin;
                            }

                            if(allowedUntilTime.length() < 1)
                            {
                                allowedUntilTime = "old";
                                allowedUntilTimeAr[0] = entry.allowedUntilTimeHour;
                                allowedUntilTimeAr[1] = entry.allowedUntilTimeMin;
                            }
                        }

                        if(!foundExisting)
                        {
                            d._network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
                            return;
                        }
                    }
                    else
                    {
                        d._network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
                        return;
                    }

                    typename TTraits::UpdatedAuthorizationEntry entry;

                    memset(&entry, 0, sizeof(entry));
                    entry.authId = authId;

                    if(name.length() < 1)
                    {
                        size_t nameLen = strlen(oldName);
                        memcpy(&entry.name, oldName, nameLen > 20 ? 20 : nameLen);
                    }
                    else
                    {
                        size_t nameLen = name.length();
                        memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                    }
                    entry.remoteAllowed = remoteAllowed;
                    entry.enabled = enabled;
                    entry.timeLimited = timeLimited;

                    if(timeLimited == 1)
                    {
                        if(allowedFrom.length() > 0)
                        {
                            entry.allowedFromYear = allowedFromAr[0];
                            entry.allowedFromMonth = allowedFromAr[1];
                            entry.allowedFromDay = allowedFromAr[2];
                            entry.allowedFromHour = allowedFromAr[3];
                            entry.allowedFromMinute = allowedFromAr[4];
                            entry.allowedFromSecond = allowedFromAr[5];
                        }

                        if(allowedUntil.length() > 0)
                        {
                            entry.allowedUntilYear = allowedUntilAr[0];
                            entry.allowedUntilMonth = allowedUntilAr[1];
                            entry.allowedUntilDay = allowedUntilAr[2];
                            entry.allowedUntilHour = allowedUntilAr[3];
                            entry.allowedUntilMinute = allowedUntilAr[4];
                            entry.allowedUntilSecond = allowedUntilAr[5];
                        }

                        entry.allowedWeekdays = allowedWeekdaysInt;

                        if(allowedFromTime.length() > 0)
                        {
                            entry.allowedFromTimeHour = allowedFromTimeAr[0];
                            entry.allowedFromTimeMin = allowedFromTimeAr[1];
                        }

                        if(allowedUntilTime.length() > 0)
                        {
                            entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                            entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                        }
                    }

                    result = d.nukiDevice().updateAuthorizationEntry(entry);
                    espDelay(250);
                    Log->print("Update authorization: ");
                    TTraits::printCommandResult(result);
                }
            }
            else
            {
                d._network->publishAuthCommandResult("invalidAction");
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

        d.updateAuth(false);

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            TTraits::cmdResultToString(result, resultStr);
            d._network->publishAuthCommandResult(resultStr);
        }
    }
    else
    {
        d._network->publishAuthCommandResult("noActionSet");
        return;
    }
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

                    Nuki::CmdResult resultTc = d.nukiDevice().retrieveTimeControlEntries();
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
    TDerived& d = static_cast<TDerived&>(*this);

    if(d._disableNonJSON)
    {
        return;
    }

    if(!d._preferences->getBool(preference_keypad_control_enabled))
    {
        d._network->publishKeypadCommandResult("KeypadControlDisabled");
        return;
    }

    if(!d.hasKeypad())
    {
        if(d._nukiConfigValid)
        {
            d._network->publishKeypadCommandResult("KeypadNotAvailable");
        }
        return;
    }
    if(!d._keypadEnabled)
    {
        return;
    }

    bool idExists = std::find(d._keypadCodeIds.begin(), d._keypadCodeIds.end(), id) != d._keypadCodeIds.end();
    int codeInt = code.toInt();
    bool codeValid = codeInt > 100000 && codeInt < 1000000 && (code.indexOf('0') == -1);
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while(retryCount < d._nrOfRetries + 1)
    {
        if(strcmp(command, "add") == 0)
        {
            if(name == "" || name == "--")
            {
                d._network->publishKeypadCommandResult("MissingParameterName");
                return;
            }
            if(codeInt == 0)
            {
                d._network->publishKeypadCommandResult("MissingParameterCode");
                return;
            }
            if(!codeValid)
            {
                d._network->publishKeypadCommandResult("CodeInvalid");
                return;
            }

            typename TTraits::NewKeypadEntry entry;
            memset(&entry, 0, sizeof(entry));
            size_t nameLen = name.length();
            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
            entry.code = codeInt;
            result = d._nukiRetryHandler->retryComm([&]()
            {
                return d.nukiDevice().addKeypadEntry(entry);
            });
            Log->print("Add keypad code: ");
            TTraits::printCommandResult(result);
            d.updateKeypad(false);
        }
        else if(strcmp(command, "delete") == 0)
        {
            if(!idExists)
            {
                d._network->publishKeypadCommandResult("UnknownId");
                return;
            }

            result = d._nukiRetryHandler->retryComm([&]()
            {
                return d.nukiDevice().deleteKeypadEntry(id);
            });
            Log->print("Delete keypad code: ");
            TTraits::printCommandResult(result);
            d.updateKeypad(false);
        }
        else if(strcmp(command, "update") == 0)
        {
            if(name == "" || name == "--")
            {
                d._network->publishKeypadCommandResult("MissingParameterName");
                return;
            }
            if(codeInt == 0)
            {
                d._network->publishKeypadCommandResult("MissingParameterCode");
                return;
            }
            if(!codeValid)
            {
                d._network->publishKeypadCommandResult("CodeInvalid");
                return;
            }
            if(!idExists)
            {
                d._network->publishKeypadCommandResult("UnknownId");
                return;
            }

            typename TTraits::UpdatedKeypadEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.codeId = id;
            size_t nameLen = name.length();
            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
            entry.code = codeInt;
            entry.enabled = enabled == 0 ? 0 : 1;
            result = d._nukiRetryHandler->retryComm([&]()
            {
                return d.nukiDevice().updateKeypadEntry(entry);
            });
            Log->print("Update keypad code: ");
            TTraits::printCommandResult(result);
            d.updateKeypad(false);
        }
        else if(strcmp(command, "--") == 0)
        {
            return;
        }
        else
        {
            d._network->publishKeypadCommandResult("UnknownCommand");
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
        d._network->publishKeypadCommandResult(resultStr);
    }
}
