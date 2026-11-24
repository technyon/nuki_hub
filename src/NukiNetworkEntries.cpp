#include <algorithm>
#include "NukiNetworkEntries.h"
#include "NukiJson.h"
#include "PsramJsonAllocator.h"
#include "Arduino.h"
#include "Config.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <ArduinoJson.h>
#include <ctype.h>
#include "hal/wdt_hal.h"
#include "util/NukiHelper.h"
#include "util/StringUtil.h"

extern bool& forceEnableWebServer;
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

NukiNetworkEntries::NukiNetworkEntries(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize, const char* idPreference, const char* topicSuffix, const char* deviceType)
    : _network(network),
      _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize),
      _idPreference(idPreference),
      _topicSuffix(topicSuffix),
      _deviceType(deviceType)
{
}

void NukiNetworkEntries::publishKeypad(const std::list<Nuki::KeypadEntry>& entries, uint maxKeypadCodeCount)
{
    bool publishCode = _preferences->getBool(preference_keypad_publish_code, false);
    bool topicPerEntry = _preferences->getBool(preference_keypad_topic_per_entry, false);
    bool haByCodeId = _preferences->getBool(preference_keypad_ha_by_code_id, false);
    std::vector<std::string> haSlots;
    uint index = 0;
    char uidString[20];
    itoa(_preferences->getUInt(_idPreference, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_lock_path);
    baseTopic.concat(_topicSuffix);
    PsramJsonAllocator allocator;
    JsonDocument json(&allocator);
    json.to<JsonArray>();

    for(const auto& entry : entries)
    {
        String basePath = mqtt_topic_keypad;
        basePath.concat("/code_");
        basePath.concat(std::to_string(index).c_str());
        publishKeypadEntry(basePath, entry);

        auto jsonEntry = json.add<JsonVariant>();
        NukiJson::keypadEntry(entry, jsonEntry, publishCode);

        if(topicPerEntry)
        {
            std::string slot = haByCodeId ? std::to_string(entry.codeId) : std::to_string(index);
            haSlots.push_back(slot);
            basePath = mqtt_topic_keypad;
            basePath.concat("/codes/");
            basePath.concat(slot.c_str());
            jsonEntry["name_ha"] = entry.name;
            jsonEntry["index"] = index;
            serializeJson(jsonEntry, _buffer, _bufferSize);
            _nukiPublisher->publishString(basePath.c_str(), _buffer, true);

            String basePathPrefix = "~";
            basePathPrefix.concat(basePath);

            std::string baseCommand = std::string("{ \"action\": \"update\", \"codeId\": \"") + std::to_string(entry.codeId);
            std::string enaCommand = baseCommand + (char*)"\", \"enabled\": \"1\" }";
            std::string disCommand = baseCommand + (char*)"\", \"enabled\": \"0\" }";
            std::string mqttDeviceName = std::string("keypad_") + slot;
            std::string uidStringPostfix = std::string("_") + mqttDeviceName;
            char codeName[33];
            memcpy(codeName, entry.name, sizeof(entry.name));
            codeName[sizeof(entry.name)] = '\0';
            std::string displayName = std::string("Keypad - ") + std::string((char*)codeName) + " - " + std::to_string(entry.codeId);

            _network->publishHassTopic("switch",
                                       mqttDeviceName.c_str(),
                                       uidString,
                                       uidStringPostfix.c_str(),
                                       displayName.c_str(),
                                       _nukiName,
                                       baseTopic.c_str(),
                                       String("~") + basePath.c_str(),
                                       (char*)"SmartLock",
                                       "",
                                       "",
                                       "diagnostic",
                                       String("~") + mqtt_topic_keypad_json_action,
            {
                { (char*)"json_attr_t", (char*)basePathPrefix.c_str() },
                { (char*)"pl_on", (char*)enaCommand.c_str() },
                { (char*)"pl_off", (char*)disCommand.c_str() },
                { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                { (char*)"stat_on", (char*)"1" },
                { (char*)"stat_off", (char*)"0" }
            });
        }

        ++index;
    }

    _nukiPublisher->publishJson(mqtt_topic_keypad_json, json, _buffer, _bufferSize, true);

    if(!_disableNonJSON)
    {
        if(_clearNonJsonKeypad || index < _publishedKeypadCount)
        {
            _clearNonJsonKeypad = false;
            while(index < maxKeypadCodeCount)
            {
                Nuki::KeypadEntry entry;
                memset(&entry, 0, sizeof(entry));
                String basePath = mqtt_topic_keypad;
                basePath.concat("/code_");
                basePath.concat(std::to_string(index).c_str());
                publishKeypadEntry(basePath, entry);

                ++index;
            }

            if(!publishCode)
            {
                for(int i=0; i<maxKeypadCodeCount; i++)
                {
                    String codeTopic = _mqttPath;
                    codeTopic.concat(mqtt_topic_keypad);
                    codeTopic.concat("/code_");
                    codeTopic.concat(std::to_string(i).c_str());
                    codeTopic.concat("/");
                    _network->removeTopic(codeTopic, "code");
                }
            }
        }
    }
    else
    {
        if (_clearNonJsonKeypad)
        {
            _clearNonJsonKeypad = false;
            for(int i=0; i<maxKeypadCodeCount; i++)
            {
                String codeTopic = _mqttPath;
                codeTopic.concat(mqtt_topic_keypad);
                codeTopic.concat("/code_");
                codeTopic.concat(std::to_string(i).c_str());
                codeTopic.concat("/");
                _network->removeTopic(codeTopic, "id");
                _network->removeTopic(codeTopic, "enabled");
                _network->removeTopic(codeTopic, "code");
                _network->removeTopic(codeTopic, "name");
                _network->removeTopic(codeTopic, "createdYear");
                _network->removeTopic(codeTopic, "createdMonth");
                _network->removeTopic(codeTopic, "createdDay");
                _network->removeTopic(codeTopic, "createdHour");
                _network->removeTopic(codeTopic, "createdMin");
                _network->removeTopic(codeTopic, "createdSec");
                _network->removeTopic(codeTopic, "lockCount");
            }
        }
    }

    if(topicPerEntry)
    {
        for(const auto& slot : _keypadHaSlots)
        {
            if(std::find(haSlots.begin(), haSlots.end(), slot) == haSlots.end())
            {
                String codesTopic = _mqttPath;
                codesTopic.concat(mqtt_topic_keypad_codes);
                codesTopic.concat("/");
                _network->removeTopic(codesTopic, slot.c_str());
                std::string mqttDeviceName = std::string("keypad_") + slot;
                _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
            }
        }
        _keypadHaSlots = haSlots;

        if(_clearKeypadSlotEntities || entries.size() < _publishedKeypadCount)
        {
            _clearKeypadSlotEntities = false;
            for(int j = haByCodeId ? 0 : entries.size(); j<maxKeypadCodeCount; j++)
            {
                String codesTopic = _mqttPath;
                codesTopic.concat(mqtt_topic_keypad_codes);
                codesTopic.concat("/");
                _network->removeTopic(codesTopic, (char*)std::to_string(j).c_str());
                std::string mqttDeviceName = std::string("keypad_") + std::to_string(j);
                _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
            }
        }
    }

    _publishedKeypadCount = entries.size();
}

void NukiNetworkEntries::publishAuth(const std::list<Nuki::AuthorizationEntry>& authEntries, uint maxAuthEntryCount)
{
    bool topicPerEntry = _preferences->getBool(preference_auth_topic_per_entry, false);
    bool haByAuthId = _preferences->getBool(preference_auth_ha_by_auth_id, false);
    std::vector<std::string> haSlots;
    uint index = 0;
    char str[50];
    char uidString[20];
    itoa(_preferences->getUInt(_idPreference, 0), uidString, 16);
    String baseTopic = _preferences->getString(preference_mqtt_lock_path);
    baseTopic.concat(_topicSuffix);
    PsramJsonAllocator allocator;
    JsonDocument json(&allocator);

    for(const auto& entry : authEntries)
    {
        auto jsonEntry = json.add<JsonVariant>();
        NukiJson::authEntry(entry, jsonEntry);
        _authEntries[entry.authId] = jsonEntry["name"].as<String>();

        if(topicPerEntry)
        {
            std::string slot = haByAuthId ? std::to_string(entry.authId) : std::to_string(index);
            haSlots.push_back(slot);
            String basePath = mqtt_topic_auth;
            basePath.concat("/entries/");
            basePath.concat(slot.c_str());
            jsonEntry["index"] = index;
            serializeJson(jsonEntry, _buffer, _bufferSize);
            _nukiPublisher->publishString(basePath.c_str(), _buffer, true);

            String basePathPrefix = "~";
            basePathPrefix.concat(basePath);
            const char *basePathPrefixChr = basePathPrefix.c_str();

            std::string baseCommand = std::string("{ \"action\": \"update\", \"authId\": \"") + std::to_string(entry.authId);
            std::string enaCommand = baseCommand + (char*)"\", \"enabled\": \"1\" }";
            std::string disCommand = baseCommand + (char*)"\", \"enabled\": \"0\" }";
            std::string mqttDeviceName = std::string("auth_") + slot;
            std::string uidStringPostfix = std::string("_") + mqttDeviceName;
            char authName[sizeof(entry.name) + 1];
            memset(authName, 0, sizeof(authName));
            memcpy(authName, entry.name, sizeof(entry.name));
            std::string displayName = std::string("Authorization - ") + std::string(authName) + " - " + std::to_string(entry.authId);

            _network->publishHassTopic("switch",
                                       mqttDeviceName.c_str(),
                                       uidString,
                                       uidStringPostfix.c_str(),
                                       displayName.c_str(),
                                       _nukiName,
                                       baseTopic.c_str(),
                                       String("~") + basePath.c_str(),
                                       (char*)_deviceType,
                                       "",
                                       "",
                                       "diagnostic",
                                       String("~") + mqtt_topic_auth_action,
            {
                { (char*)"json_attr_t", (char*)basePathPrefixChr },
                { (char*)"pl_on", (char*)enaCommand.c_str() },
                { (char*)"pl_off", (char*)disCommand.c_str() },
                { (char*)"val_tpl", (char*)"{{value_json.enabled}}" },
                { (char*)"stat_on", (char*)"1" },
                { (char*)"stat_off", (char*)"0" }
            });
        }

        ++index;
    }

    _nukiPublisher->publishJson(mqtt_topic_auth_json, json, _buffer, _bufferSize, true);

    if(topicPerEntry)
    {
        for(const auto& slot : _authHaSlots)
        {
            bool stillThere = false;
            for(const auto& current : haSlots)
            {
                if(current == slot)
                {
                    stillThere = true;
                    break;
                }
            }
            if(!stillThere)
            {
                String entriesTopic = _mqttPath;
                entriesTopic.concat(mqtt_topic_auth_entries);
                entriesTopic.concat("/");
                _network->removeTopic(entriesTopic, slot.c_str());
                std::string mqttDeviceName = std::string("auth_") + slot;
                _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
            }
        }
        _authHaSlots = haSlots;
    }

    // Remove list-position entries left over (all of them once after switching to ids)
    uint firstStale = (haByAuthId && topicPerEntry) ? (_clearAuthSlotEntities ? 0 : maxAuthEntryCount) : authEntries.size();
    _clearAuthSlotEntities = false;
    for(int j=firstStale; j<maxAuthEntryCount; j++)
    {
        bool inUse = false;
        for(const auto& current : haSlots)
        {
            if(current == std::to_string(j))
            {
                inUse = true;
                break;
            }
        }
        if(inUse)
        {
            continue;
        }
        String entriesTopic = _mqttPath;
        entriesTopic.concat(mqtt_topic_auth_entries);
        entriesTopic.concat("/");
        _network->removeTopic(entriesTopic, (char*)std::to_string(j).c_str());
        std::string mqttDeviceName = std::string("auth_") + std::to_string(j);
        _network->removeHassTopic((char*)"switch", (char*)mqttDeviceName.c_str(), uidString);
    }
}

void NukiNetworkEntries::publishKeypadEntry(const String topic, Nuki::KeypadEntry entry)
{
    if(_disableNonJSON)
    {
        return;
    }

    char codeName[sizeof(entry.name) + 1];
    memset(codeName, 0, sizeof(codeName));
    memcpy(codeName, entry.name, sizeof(entry.name));

    _nukiPublisher->publishInt(concat(topic, "/id").c_str(), entry.codeId, true);
    _nukiPublisher->publishBool(concat(topic, "/enabled").c_str(), entry.enabled, true);
    _nukiPublisher->publishString(concat(topic, "/name").c_str(), codeName, true);

    if(_preferences->getBool(preference_keypad_publish_code, false))
    {
        _nukiPublisher->publishInt(concat(topic, "/code").c_str(), entry.code, true);
    }

    _nukiPublisher->publishInt(concat(topic, "/createdYear").c_str(), entry.dateCreatedYear, true);
    _nukiPublisher->publishInt(concat(topic, "/createdMonth").c_str(), entry.dateCreatedMonth, true);
    _nukiPublisher->publishInt(concat(topic, "/createdDay").c_str(), entry.dateCreatedDay, true);
    _nukiPublisher->publishInt(concat(topic, "/createdHour").c_str(), entry.dateCreatedHour, true);
    _nukiPublisher->publishInt(concat(topic, "/createdMin").c_str(), entry.dateCreatedMin, true);
    _nukiPublisher->publishInt(concat(topic, "/createdSec").c_str(), entry.dateCreatedSec, true);
    _nukiPublisher->publishInt(concat(topic, "/lockCount").c_str(), entry.lockCount, true);
}

String NukiNetworkEntries::concat(String a, String b)
{
    String c = a;
    c.concat(b);
    return c;
}
