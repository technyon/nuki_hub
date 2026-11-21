#include "HomeAssistantDiscovery.h"
#include "Config.h"
#include "EspMillis.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "esp_rom_crc.h"
#include "esp_heap_caps.h"
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_HOSTED_ENABLED)
#include "WiFi.h"
#else
#include "esp_mac.h"
#endif

#define HASS_MAX_QUEUED 6
#define HASS_QUEUE_WAIT_MS 3000
#define HASS_DEVICE_BUCKETS_HUB 4
#define HASS_DEVICE_BUCKETS_NUKI 16
#define HASS_SENT_CACHE_SIZE 128

extern TaskHandle_t networkTaskHandle;

namespace
{
struct HassLock
{
    explicit HassLock(SemaphoreHandle_t mutex) : _mutex(mutex)
    {
        xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }
    ~HassLock()
    {
        xSemaphoreGiveRecursive(_mutex);
    }
    SemaphoreHandle_t _mutex;
};

uint32_t hassCrc(uint32_t crc, const char* value)
{
    return esp_rom_crc32_le(crc, (const uint8_t*)value, strlen(value));
}

bool splitHassPath(const String& path, String& component, String& uid, String& name)
{
    int end = path.lastIndexOf('/');
    int nameStart = path.lastIndexOf('/', end - 1);
    int uidStart = path.lastIndexOf('/', nameStart - 1);
    int componentStart = path.lastIndexOf('/', uidStart - 1);
    if(end < 0 || nameStart < 0 || uidStart < 0 || componentStart < 0)
    {
        return false;
    }
    component = path.substring(componentStart + 1, uidStart);
    uid = path.substring(uidStart + 1, nameStart);
    name = path.substring(nameStart + 1, end);
    return true;
}
}

HomeAssistantDiscovery::HomeAssistantDiscovery(NetworkDevice* device, Preferences *preferences, char* buffer, size_t bufferSize)
    : _device(device),
      _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize)
{
    _baseTopic = _preferences->getString(preference_mqtt_lock_path);
    _hostname = _preferences->getString(preference_hostname, "");
    _discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery, "");
    _discoveryTopicOrDefault = _preferences->getString(preference_mqtt_hass_discovery, "homeassistant");
    _mutex = xSemaphoreCreateRecursiveMutex();
}

bool HomeAssistantDiscovery::tryBegin()
{
    return xSemaphoreTakeRecursive(_mutex, 0) == pdTRUE;
}

void HomeAssistantDiscovery::end()
{
    xSemaphoreGiveRecursive(_mutex);
}

void HomeAssistantDiscovery::requestRefresh()
{
    HassLock lock(_mutex);
    _refreshRequested[0] = true;
    _refreshRequested[1] = true;
    _refreshRequested[2] = true;
    _sent.clear();
}

bool HomeAssistantDiscovery::runDiscovery(int type, const char* uidString, const std::function<void()>& generate)
{
    char signatureKey[] = "hasig0";
    char bucketsKey[] = "habkt0";
    signatureKey[5] = '0' + type;
    bucketsKey[5] = '0' + type;

    if(!_device->mqttConnected())
    {
        return false;
    }

    int buckets = 0;
    if(_discoveryTopic != "" && _preferences->getBool(preference_hass_device_discovery, false))
    {
        buckets = type == 0 ? HASS_DEVICE_BUCKETS_HUB : HASS_DEVICE_BUCKETS_NUKI;
    }
    int lastBuckets = _preferences->getInt(bucketsKey, 0);

    _runUid = uidString;
    _buckets = buckets;
    _run = HassRun::Hash;
    _crc = buckets;
    generate();
    uint32_t signature = _crc;

    bool changed = _preferences->getUInt(signatureKey, 0) != signature || lastBuckets != buckets;
    bool refresh = _refreshRequested[type] || !_publishedSinceBoot[type];

    if(!changed && !refresh)
    {
        _run = HassRun::Direct;
        Log->println("HomeAssistant discovery is unchanged, nothing published.");
        return true;
    }

    _sendFailed = false;

    if(lastBuckets > 0 && buckets == 0 && _discoveryTopic != "")
    {
        for(int i = 0; i < lastBuckets; i++)
        {
            send(chunkTopic(uidString, i).c_str(), "{\"migrate_discovery\":true}", false);
        }
    }
    else if(lastBuckets == 0 && buckets > 0)
    {
        _run = HassRun::Migrate;
        generate();
    }

    if(buckets == 0)
    {
        _run = changed ? HassRun::Publish : HassRun::Refresh;
        generate();
    }
    else
    {
        for(int i = 0; i < buckets; i++)
        {
            _run = HassRun::Collect;
            _bucket = i;
            _chunk.clear();
            generate();
            flushChunk(uidString, i);
        }
    }

    if(lastBuckets > 0 && buckets == 0 && _discoveryTopic != "")
    {
        for(int i = 0; i < lastBuckets; i++)
        {
            send(chunkTopic(uidString, i).c_str(), "", true);
        }
    }
    else if(lastBuckets == 0 && buckets > 0)
    {
        _run = HassRun::Cleanup;
        generate();
    }

    _run = HassRun::Direct;
    _chunk.clear();

    if(_sendFailed)
    {
        Log->println("HomeAssistant discovery was not published completely, it will be repeated.");
        return false;
    }

    _refreshRequested[type] = false;
    _publishedSinceBoot[type] = true;
    if(changed)
    {
        _preferences->putUInt(signatureKey, signature);
        _preferences->putInt(bucketsKey, buckets);
    }
    return true;
}

bool HomeAssistantDiscovery::needsPayload(const String& path)
{
    if(_run == HassRun::Migrate || _run == HassRun::Cleanup)
    {
        return false;
    }
    if(_run == HassRun::Collect)
    {
        String component, uid, name;
        if(!splitHassPath(path, component, uid, name))
        {
            return false;
        }
        if(uid != _runUid)
        {
            return _bucket == 0;
        }
        return (int)(hassCrc(0, (component + "_" + name).c_str()) % _buckets) == _bucket;
    }
    return true;
}

void HomeAssistantDiscovery::publishConfig(const String& path, JsonDocument& json)
{
    if(_run == HassRun::Migrate)
    {
        if(!foreignPath(path))
        {
            send(path.c_str(), "{\"migrate_discovery\":true}", false);
        }
        return;
    }
    if(_run == HassRun::Cleanup)
    {
        if(!foreignPath(path))
        {
            send(path.c_str(), "", true);
        }
        return;
    }
    if(_run == HassRun::Collect)
    {
        String component, uid, name;
        if(!needsPayload(path) || !splitHassPath(path, component, uid, name))
        {
            return;
        }
        if(uid == _runUid)
        {
            JsonObject entry = _chunk["cmps"][component + "_" + name].to<JsonObject>();
            entry.set(json.as<JsonObjectConst>());
            entry.remove("dev");
            entry["p"] = component;
            return;
        }
    }

    if(_run == HassRun::Hash)
    {
        serializeJson(json, _buffer, _bufferSize);
        String component, uid, name;
        if(json["dev"]["mf"].is<const char*>() && splitHassPath(path, component, uid, name))
        {
            _deviceInfo[uid] = json["dev"];
        }
        _crc = hassCrc(hassCrc(_crc, path.c_str()), _buffer);
        return;
    }

    uint32_t key = 0;
    uint32_t value = 0;
    if(_run == HassRun::Direct)
    {
        serializeJson(json, _buffer, _bufferSize);
        key = hassCrc(0, path.c_str());
        value = hassCrc(1, _buffer);
        auto it = _sent.find(key);
        if(it != _sent.end() && it->second == value)
        {
            return;
        }
    }

    if(!waitForSlot())
    {
        return;
    }
    serializeJson(json, _buffer, _bufferSize);
    if(publishNow(path.c_str(), _buffer, true) && _run == HassRun::Direct)
    {
        if(_sent.size() >= HASS_SENT_CACHE_SIZE)
        {
            _sent.clear();
        }
        _sent[key] = value;
    }
}

void HomeAssistantDiscovery::removeConfig(const String& path)
{
    if(_run == HassRun::Hash)
    {
        _crc = hassCrc(hassCrc(_crc, path.c_str()), "-");
        return;
    }
    if(_run == HassRun::Collect)
    {
        String component, uid, name;
        if(needsPayload(path) && splitHassPath(path, component, uid, name) && uid == _runUid)
        {
            _chunk["cmps"][component + "_" + name].to<JsonObject>()["p"] = component;
        }
        return;
    }
    if(_run == HassRun::Migrate || _run == HassRun::Refresh)
    {
        return;
    }
    if(_run == HassRun::Cleanup && foreignPath(path))
    {
        return;
    }
    if(_run == HassRun::Direct)
    {
        uint32_t key = hassCrc(0, path.c_str());
        auto it = _sent.find(key);
        if(it != _sent.end() && it->second == 0)
        {
            return;
        }
        if(send(path.c_str(), "", true))
        {
            if(_sent.size() >= HASS_SENT_CACHE_SIZE)
            {
                _sent.clear();
            }
            _sent[key] = 0;
        }
        return;
    }

    send(path.c_str(), "", true);
}

bool HomeAssistantDiscovery::foreignPath(const String& path)
{
    String component, uid, name;
    return !splitHassPath(path, component, uid, name) || uid != _runUid;
}

bool HomeAssistantDiscovery::waitForSlot()
{
    if(_run != HassRun::Direct && _sendFailed)
    {
        return false;
    }

    int64_t start = espMillis();
    while(_device->mqttConnected() && _device->mqttQueueSize() >= HASS_MAX_QUEUED && espMillis() - start < HASS_QUEUE_WAIT_MS)
    {
        if(xTaskGetCurrentTaskHandle() == networkTaskHandle)
        {
            _device->update();
        }
        espDelayAck(5);
    }

    if(!_device->mqttConnected() || _device->mqttQueueSize() >= HASS_MAX_QUEUED)
    {
        _sendFailed = true;
        return false;
    }
    return true;
}

bool HomeAssistantDiscovery::publishNow(const char* path, const char* payload, bool retain)
{
    if(_device->mqttPublish(path, MQTT_QOS_LEVEL, retain, payload) == 0)
    {
        _sendFailed = true;
        return false;
    }
    return true;
}

bool HomeAssistantDiscovery::send(const char* path, const char* payload, bool retain)
{
    return waitForSlot() && publishNow(path, payload, retain);
}

String HomeAssistantDiscovery::chunkTopic(const char* uidString, int bucket)
{
    String path = _discoveryTopic;
    path.concat("/device/");
    path.concat(uidString);
    path.concat("/part");
    path.concat(bucket);
    path.concat("/config");
    return path;
}

void HomeAssistantDiscovery::flushChunk(const char* uidString, int bucket)
{
    String topic = chunkTopic(uidString, bucket);

    if(_chunk["cmps"].size() == 0)
    {
        send(topic.c_str(), "", true);
        return;
    }

    if(_deviceInfo[uidString].is<JsonObject>())
    {
        _chunk["dev"] = _deviceInfo[uidString];
    }
    else
    {
        _chunk["dev"]["ids"][0] = String("nuki_") + uidString;
    }
    _chunk["o"]["name"] = "Nuki Hub";
    _chunk["o"]["sw"] = NUKI_HUB_VERSION;
    _chunk["o"]["url"] = "https://github.com/technyon/nuki_hub";

    size_t size = measureJson(_chunk) + 1;

    if(size <= _bufferSize)
    {
        if(waitForSlot())
        {
            serializeJson(_chunk, _buffer, _bufferSize);
            publishNow(topic.c_str(), _buffer, true);
        }
    }
    else if(size >= heap_caps_get_largest_free_block(MALLOC_CAP_8BIT))
    {
        Log->print("Not enough free heap to publish ");
        Log->print(topic);
        Log->print(", needs ");
        Log->println(size);
        _sendFailed = true;
    }
    else
    {
        std::string payload;
        payload.reserve(size);
        serializeJson(_chunk, payload);
        send(topic.c_str(), payload.c_str(), true);
    }
    _chunk.clear();
}

bool HomeAssistantDiscovery::setupHASS(int type, uint32_t nukiId, char* nukiName, const char* firmwareVersion, const char* hardwareVersion, bool hasDoorSensor, bool hasKeypad)
{
    bool published = true;
    HassLock lock(_mutex);
    _discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery, "");
    _discoveryTopicOrDefault = _preferences->getString(preference_mqtt_hass_discovery, "homeassistant");

    uint64_t savedDevId = _preferences->getULong64(preference_nukihub_id, 0);
    uint64_t curDevId;

    #if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_HOSTED_ENABLED)
    curDevId = 0;
    std::string macString = std::string(WiFi.macAddress().c_str());
    std::string::iterator i;
    std::string::iterator end = macString.end();

    for(i = macString.begin(); i != end; ++i) {
        char let = *i;
        if (let >= '0' && let <= '9') {
            curDevId = curDevId*0xf + (let-'0');
        } else if (let >= 'a' && let <= 'f') {
            curDevId = curDevId*0xf + (let-'a'+10);
        } else if (let >= 'A' && let <= 'F') {
            curDevId = curDevId*0xf + (let-'A'+10);
        }
    }
    #else
    uint8_t mac[8] = {0};
    esp_efuse_mac_get_default(mac);
    memcpy(&curDevId, &mac, 8);
    #endif

    if (savedDevId == 0)
    {
        _preferences->putULong64(preference_nukihub_id, curDevId);
        char uidString[20];
        itoa(_preferences->getUInt(preference_device_id_lock, 0), uidString, 10);
        removeHASSConfig(uidString);
        espDelayAck(3000);
    }
    else if(savedDevId != curDevId)
    {
        _preferences->putULong64(preference_nukihub_id, curDevId);
        Log->print("Efuse ID: ");
        Log->println(curDevId);
        Log->print("Saved ID: ");
        Log->println(savedDevId);
        Log->println("Efuse ID and Nuki Hub device ID do not match, removing HomeAssistant setup for incorrect Nuki Hub device ID.");
        char uidString[21];
        itoa(_preferences->getUInt(preference_device_id_lock, 0), uidString, 10);
        removeHASSConfig(uidString);
        espDelayAck(3000);
        // Same format as _nukiHubUidString: the ID is 64 bit, itoa() truncated it to int.
        snprintf(uidString, sizeof(uidString), "%" PRIu64, savedDevId);
        removeHASSConfig(uidString);
        espDelayAck(3000);
    }

    sprintf(_nukiHubUidString, "%" PRIu64, curDevId);
    Log->print("HA UID: ");
    Log->println(_nukiHubUidString);

    char uidString[20];
    itoa(nukiId, uidString, 16);
    bool publishAuthData = _preferences->getBool(preference_publish_authdata, false);

    if(type == 0)
    {
        published = runDiscovery(0, _nukiHubUidString, [&]()
        {
            publishHASSNukiHubConfig();
        });
        if(published)
        {
            Log->println("HomeAssistant setup for Nuki Hub completed.");
        }
    }
    else if(type == 1)
    {
        if(_preferences->getUInt(preference_nuki_id_lock, 0) != nukiId)
        {
            return true;
        }
        String lockTopic = _baseTopic;
        lockTopic.concat("/lock");
        published = runDiscovery(1, uidString, [&]()
        {
            publishHASSConfig((char*)"SmartLock", lockTopic.c_str(), nukiName, uidString, firmwareVersion, hardwareVersion, hasDoorSensor, hasKeypad, publishAuthData, (char*)"lock", (char*)"unlock", (char*)"unlatch");
        });
        if(published)
        {
            Log->println("HomeAssistant setup for lock completed.");
        }
    }
    else if(type == 2)
    {
        if(_preferences->getUInt(preference_nuki_id_opener, 0) != nukiId)
        {
            return true;
        }
        String openerTopic = _baseTopic;
        openerTopic.concat("/opener");
        bool continuousMode = _preferences->getBool(preference_opener_continuous_mode, false);
        published = runDiscovery(2, uidString, [&]()
        {
            publishHASSConfig((char*)"Opener", openerTopic.c_str(), nukiName, uidString, firmwareVersion, hardwareVersion, hasDoorSensor, hasKeypad, publishAuthData, continuousMode ? (char*)"deactivateCM" : (char*)"deactivateRTO", continuousMode ? (char*)"activateCM" : (char*)"activateRTO", (char*)"electricStrikeActuation");
        });

        if(published)
        {
            Log->println("HomeAssistant setup for opener completed.");
        }
    }
    return published;
}

void HomeAssistantDiscovery::disableHASS()
{
    HassLock lock(_mutex);
    _preferences->remove("hasig0");
    _preferences->remove("hasig1");
    _preferences->remove("hasig2");
    _sent.clear();
    removeHASSConfig(_nukiHubUidString);
    espDelayAck(3000);

    char uidString[20];

    if(_preferences->getUInt(preference_nuki_id_lock, 0) != 0)
    {
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        removeHASSConfig(uidString);
        espDelayAck(3000);
    }
    if(_preferences->getUInt(preference_nuki_id_opener, 0) != 0)
    {
        itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
        removeHASSConfig(uidString);
        espDelayAck(3000);
    }
}

static const char* const lockFobOptions[] = {"No Action", "Unlock", "Lock", "Lock n Go", "Intelligent", nullptr};
static const char* const openerFobOptions[] = {"No Action", "Toggle RTO", "Activate RTO", "Deactivate RTO", "Open", "Ring", nullptr};
static const char* const advertisingModeOptions[] = {"Automatic", "Normal", "Slow", "Slowest", nullptr};
static const char* const timezoneOptions[] = {"Africa/Cairo", "Africa/Lagos", "Africa/Maputo", "Africa/Nairobi", "America/Anchorage", "America/Argentina/Buenos_Aires", "America/Chicago", "America/Denver", "America/Halifax", "America/Los_Angeles", "America/Manaus", "America/Mexico_City", "America/New_York", "America/Phoenix", "America/Regina", "America/Santiago", "America/Sao_Paulo", "America/St_Johns", "Asia/Bangkok", "Asia/Dubai", "Asia/Hong_Kong", "Asia/Jerusalem", "Asia/Karachi", "Asia/Kathmandu", "Asia/Kolkata", "Asia/Riyadh", "Asia/Seoul", "Asia/Shanghai", "Asia/Tehran", "Asia/Tokyo", "Asia/Yangon", "Australia/Adelaide", "Australia/Brisbane", "Australia/Darwin", "Australia/Hobart", "Australia/Perth", "Australia/Sydney", "Europe/Berlin", "Europe/Helsinki", "Europe/Istanbul", "Europe/London", "Europe/Moscow", "Pacific/Auckland", "Pacific/Guam", "Pacific/Honolulu", "Pacific/Pago_Pago", "None", nullptr};
static const char* const lockButtonOptions[] = {"No Action", "Intelligent", "Unlock", "Lock", "Unlatch", "Lock n Go", "Show Status", nullptr};
static const char* const openerButtonOptions[] = {"No Action", "Toggle RTO", "Activate RTO", "Deactivate RTO", "Toggle CM", "Activate CM", "Deactivate CM", "Open", nullptr};
static const char* const batteryTypeOptions[] = {"Alkali", "Accumulators", "Lithium", "No Warnings", nullptr};
static const char* const motorSpeedOptions[] = {"Standard", "Insane", "Gentle", nullptr};
static const char* const operatingModeOptions[] = {"Generic door opener", "Analogue intercom", "Digital intercom", "Siedle", "TCS", "Bticino", "Siedle HTS", "STR", "Ritto", "Fermax", "Comelit", "Urmet BiBus", "Urmet 2Voice", "Golmar", "SKS", "Spare", nullptr};
static const char* const doorbellSuppressionOptions[] = {"Off", "CM", "RTO", "CM & RTO", "Ring", "CM & Ring", "RTO & Ring", "CM & RTO & Ring", nullptr};
static const char* const soundOptions[] = {"No Sound", "Sound 1", "Sound 2", "Sound 3", nullptr};

static const HassEntity lockEntities[] =
{
    {"button", "unlatch", "Open", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 2, -1, 0, {{"en", "false"}, {"pl_prs", "unlatch"}}},
    {"button", "lockngo", "Lock 'n' Go", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 3, -1, 0, {{"en", "false"}, {"pl_prs", "lockNgo"}}},
    {"binary_sensor", "lock_n_go_active", "Lock 'n' Go active", mqtt_topic_lock_state, "running", "", "", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "1"}, {"pl_off", "0"}, {"val_tpl", "{{1 if value == 'unlockedLnga' else 0}}"}}},
    {"button", "lockngounlatch", "Lock 'n' Go with unlatch", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 4, -1, 0, {{"en", "false"}, {"pl_prs", "lockNgoUnlatch"}}},
    {"button", "query_battery", "Query battery", nullptr, "", "", "diagnostic", mqtt_topic_query_battery, HassAcl::None, 0, -1, 0, {{"en", "false"}, {"pl_prs", "1"}}},
    {"switch", "led_enabled", "LED enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 6, -1, 0, {{"en", "true"}, {"ic", "mdi:led-variant-on"}, {"pl_on", "{ \"ledEnabled\": \"1\"}"}, {"pl_off", "{ \"ledEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.ledEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "button_enabled", "Button enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 5, -1, 0, {{"en", "true"}, {"ic", "mdi:radiobox-marked"}, {"pl_on", "{ \"buttonEnabled\": \"1\"}"}, {"pl_off", "{ \"buttonEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.buttonEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "auto_lock", "Auto lock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 19, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoLockEnabled\": \"1\"}"}, {"pl_off", "{ \"autoLockEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.autoLockEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "auto_unlock", "Auto unlock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 12, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoUnLockDisabled\": \"0\"}"}, {"pl_off", "{ \"autoUnLockDisabled\": \"1\"}"}, {"val_tpl", "{{value_json.autoUnLockDisabled}}"}, {"stat_on", "0"}, {"stat_off", "1"}}},
    {"switch", "double_lock", "Double lock", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 13, -1, 0, {{"en", "true"}, {"pl_on", "{ \"singleLock\": \"0\"}"}, {"pl_off", "{ \"singleLock\": \"1\"}"}, {"val_tpl", "{{value_json.singleLock}}"}, {"stat_on", "0"}, {"stat_off", "1"}}},
    {"sensor", "battery_level", "Battery level", mqtt_topic_battery_basic_json, "battery", "measurement", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"unit_of_meas", "%"}, {"val_tpl", "{{value_json.level}}"}}},
    {"number", "led_brightness", "LED brightness", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 7, -1, 0, {{"en", "true"}, {"ic", "mdi:brightness-6"}, {"cmd_tpl", "{ \"ledBrightness\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.ledBrightness}}"}, {"min", "0"}, {"max", "5"}}},
    {"switch", "auto_unlatch", "Auto unlatch", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 3, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoUnlatch\": \"1\"}"}, {"pl_off", "{ \"autoUnlatch\": \"0\"}"}, {"val_tpl", "{{value_json.autoUnlatch}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "pairing_enabled", "Pairing enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 4, -1, 0, {{"en", "true"}, {"pl_on", "{ \"pairingEnabled\": \"1\"}"}, {"pl_off", "{ \"pairingEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.pairingEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "timezone_offset", "Timezone offset", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 8, -1, 0, {{"en", "true"}, {"ic", "mdi:timer-cog-outline"}, {"cmd_tpl", "{ \"timeZoneOffset\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.timeZoneOffset}}"}, {"min", "0"}, {"max", "60"}}},
    {"switch", "dst_mode", "DST mode European", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 9, -1, 0, {{"en", "true"}, {"pl_on", "{ \"dstMode\": \"1\"}"}, {"pl_off", "{ \"dstMode\": \"0\"}"}, {"val_tpl", "{{value_json.dstMode}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "unlocked_position_offset_degrees", "Unlocked position offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 0, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"unlockedPositionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.unlockedPositionOffsetDegrees}}"}, {"min", "-90"}, {"max", "180"}}},
    {"number", "locked_position_offset_degrees", "Locked position offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 1, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"lockedPositionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.lockedPositionOffsetDegrees}}"}, {"min", "-180"}, {"max", "90"}}},
    {"number", "single_locked_position_offset_degrees", "Single locked position offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 2, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"singleLockedPositionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.singleLockedPositionOffsetDegrees}}"}, {"min", "-180"}, {"max", "180"}}},
    {"number", "unlocked_locked_transition_offset_degrees", "Unlocked to locked transition offset degrees", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 3, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"unlockedToLockedTransitionOffsetDegrees\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.unlockedToLockedTransitionOffsetDegrees}}"}, {"min", "-180"}, {"max", "180"}}},
    {"number", "lockngo_timeout", "Lock n Go timeout", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 4, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"lockNgoTimeout\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.lockNgoTimeout}}"}, {"min", "5"}, {"max", "60"}}},
    {"switch", "detached_cylinder", "Detached cylinder", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 7, -1, 0, {{"en", "true"}, {"pl_on", "{ \"detachedCylinder\": \"1\"}"}, {"pl_off", "{ \"detachedCylinder\": \"0\"}"}, {"val_tpl", "{{value_json.detachedCylinder}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "automatic_battery_type_detection", "Automatic battery type detection", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 9, -1, -1, {{"en", "true"}, {"pl_on", "{ \"automaticBatteryTypeDetection\": \"1\"}"}, {"pl_off", "{ \"automaticBatteryTypeDetection\": \"0\"}"}, {"val_tpl", "{{value_json.automaticBatteryTypeDetection}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "unlatch_duration", "Unlatch duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 10, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"unlatchDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.unlatchDuration}}"}, {"min", "1"}, {"max", "30"}}},
    {"number", "auto_lock_timeout", "Auto lock timeout", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 11, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"autoLockTimeOut\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.autoLockTimeOut}}"}, {"min", "30"}, {"max", "1800"}}},
    {"switch", "nightmode_enabled", "Nightmode enabled", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 13, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeEnabled\": \"1\"}"}, {"pl_off", "{ \"nightModeEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.nightModeEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"text", "nightmode_start_time", "Nightmode start time", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 14, -1, 0, {{"en", "true"}, {"pattern", "([0-1][0-9]|2[0-3]):[0-5][0-9]"}, {"cmd_tpl", "{ \"nightModeStartTime\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.nightModeStartTime}}"}, {"min", "5"}, {"max", "5"}}},
    {"text", "nightmode_end_time", "Nightmode end time", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 15, -1, 0, {{"en", "true"}, {"pattern", "([0-1][0-9]|2[0-3]):[0-5][0-9]"}, {"cmd_tpl", "{ \"nightModeEndTime\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.nightModeEndTime}}"}, {"min", "5"}, {"max", "5"}}},
    {"switch", "nightmode_auto_lock", "Nightmode auto lock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 16, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeAutoLockEnabled\": \"1\"}"}, {"pl_off", "{ \"nightModeAutoLockEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.nightModeAutoLockEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "nightmode_auto_unlock", "Nightmode auto unlock", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 17, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeAutoUnlockDisabled\": \"0\"}"}, {"pl_off", "{ \"nightModeAutoUnlockDisabled\": \"1\"}"}, {"val_tpl", "{{value_json.nightModeAutoUnlockDisabled}}"}, {"stat_on", "0"}, {"stat_off", "1"}}},
    {"switch", "nightmode_immediate_lock_start", "Nightmode immediate lock on start", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 18, -1, 0, {{"en", "true"}, {"pl_on", "{ \"nightModeImmediateLockOnStart\": \"1\"}"}, {"pl_off", "{ \"nightModeImmediateLockOnStart\": \"0\"}"}, {"val_tpl", "{{value_json.nightModeImmediateLockOnStart}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "immediate_auto_lock_enabled", "Immediate auto lock enabled", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 20, -1, 0, {{"en", "true"}, {"pl_on", "{ \"immediateAutoLockEnabled\": \"1\"}"}, {"pl_off", "{ \"immediateAutoLockEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.immediateAutoLockEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "auto_update_enabled", "Auto update enabled", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 21, -1, 0, {{"en", "true"}, {"pl_on", "{ \"autoUpdateEnabled\": \"1\"}"}, {"pl_off", "{ \"autoUpdateEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.autoUpdateEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "enable_slow_speed_during_nightmode", "Enable slow speed during nightmode", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 24, -1, 1, {{"en", "true"}, {"pl_on", "{ \"enableSlowSpeedDuringNightMode\": \"1\"}"}, {"pl_off", "{ \"enableSlowSpeedDuringNightMode\": \"0\"}"}, {"val_tpl", "{{value_json.enableSlowSpeedDuringNightMode}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"button", "reboot_nuki", "Reboot Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 22, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"rebootNuki\": \"1\"}"}}},
    {"button", "recalibrate_nuki", "Recalibrate Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 25, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"recalibrateNuki\": \"1\"}"}}},
};

static const HassEntity openerEntities[] =
{
    {"button", "unlatch", "Open", nullptr, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 11, -1, 0, {{"en", "false"}, {"pl_prs", "electricStrikeActuation"}}},
    {"binary_sensor", "continuous_mode", "Continuous mode", mqtt_topic_lock_continuous_mode, "lock", "", "", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "on"}, {"pl_off", "off"}}},
    {"switch", "continuous_mode", "Continuous mode", mqtt_topic_lock_continuous_mode, "", "", "", mqtt_topic_lock_action, HassAcl::Action, 12, 13, 0, {{"en", "true"}, {"stat_on", "on"}, {"stat_off", "off"}, {"pl_on", "activateCM"}, {"pl_off", "deactivateCM"}}},
    {"binary_sensor", "ring_detect", "Ring detect", mqtt_topic_lock_binary_ring, "sound", "", "", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "ring"}, {"pl_off", "standby"}}},
    {"switch", "led_enabled", "LED enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 5, -1, 0, {{"en", "true"}, {"ic", "mdi:led-variant-on"}, {"pl_on", "{ \"ledFlashEnabled\": \"1\"}"}, {"pl_off", "{ \"ledFlashEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.ledFlashEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "button_enabled", "Button enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 4, -1, 0, {{"en", "true"}, {"ic", "mdi:radiobox-marked"}, {"pl_on", "{ \"buttonEnabled\": \"1\"}"}, {"pl_off", "{ \"buttonEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.buttonEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "sound_level", "Sound level", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 15, -1, 0, {{"en", "true"}, {"ic", "mdi:volume-source"}, {"cmd_tpl", "{ \"soundLevel\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.soundLevel}}"}, {"min", "0"}, {"max", "255"}, {"mode", "slider"}, {"step", "25.5"}}},
    {"switch", "pairing_enabled", "Pairing enabled", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 3, -1, 0, {{"en", "true"}, {"pl_on", "{ \"pairingEnabled\": \"1\"}"}, {"pl_off", "{ \"pairingEnabled\": \"0\"}"}, {"val_tpl", "{{value_json.pairingEnabled}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "timezone_offset", "Timezone offset", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 6, -1, 0, {{"en", "true"}, {"ic", "mdi:timer-cog-outline"}, {"cmd_tpl", "{ \"timeZoneOffset\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.timeZoneOffset}}"}, {"min", "0"}, {"max", "60"}}},
    {"switch", "dst_mode", "DST mode European", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 7, -1, 0, {{"en", "true"}, {"pl_on", "{ \"dstMode\": \"1\"}"}, {"pl_off", "{ \"dstMode\": \"0\"}"}, {"val_tpl", "{{value_json.dstMode}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "bus_mode_switch", "BUS mode switch analogue", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 1, -1, 0, {{"en", "true"}, {"pl_on", "{ \"busModeSwitch\": \"1\"}"}, {"pl_off", "{ \"busModeSwitch\": \"0\"}"}, {"val_tpl", "{{value_json.busModeSwitch}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "short_circuit_duration", "Short circuit duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 2, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"shortCircuitDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.shortCircuitDuration}}"}, {"min", "0"}}},
    {"number", "electric_strike_delay", "Electric strike delay", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 3, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"electricStrikeDelay\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.electricStrikeDelay}}"}, {"min", "0"}, {"max", "30000"}, {"step", "3000"}}},
    {"switch", "random_electric_strike_delay", "Random electric strike delay", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 4, -1, 0, {{"en", "true"}, {"pl_on", "{ \"randomElectricStrikeDelay\": \"1\"}"}, {"pl_off", "{ \"randomElectricStrikeDelay\": \"0\"}"}, {"val_tpl", "{{value_json.randomElectricStrikeDelay}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "electric_strike_duration", "Electric strike duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 5, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"electricStrikeDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.electricStrikeDuration}}"}, {"min", "1000"}, {"max", "30000"}, {"step", "3000"}}},
    {"switch", "disable_rto_after_ring", "Disable RTO after ring", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 6, -1, 0, {{"en", "true"}, {"pl_on", "{ \"disableRtoAfterRing\": \"1\"}"}, {"pl_off", "{ \"disableRtoAfterRing\": \"0\"}"}, {"val_tpl", "{{value_json.disableRtoAfterRing}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"number", "rto_timeout", "RTO timeout", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 7, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"rtoTimeout\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.rtoTimeout}}"}, {"min", "5"}, {"max", "60"}}},
    {"number", "doorbell_suppression_duration", "Doorbell suppression duration", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 9, -1, 0, {{"en", "true"}, {"cmd_tpl", "{ \"doorbellSuppressionDuration\": \"{{ value }}\" }"}, {"val_tpl", "{{value_json.doorbellSuppressionDuration}}"}, {"min", "500"}, {"max", "10000"}, {"step", "1000"}}},
    {"switch", "sound_confirmation", "Sound confirmation", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 14, -1, 0, {{"en", "true"}, {"pl_on", "{ \"soundConfirmation\": \"1\"}"}, {"pl_off", "{ \"soundConfirmation\": \"0\"}"}, {"val_tpl", "{{value_json.soundConfirmation}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"switch", "automatic_battery_type_detection", "Automatic battery type detection", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 19, -1, 0, {{"en", "true"}, {"pl_on", "{ \"automaticBatteryTypeDetection\": \"1\"}"}, {"pl_off", "{ \"automaticBatteryTypeDetection\": \"0\"}"}, {"val_tpl", "{{value_json.automaticBatteryTypeDetection}}"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"button", "reboot_nuki", "Reboot Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 20, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"rebootNuki\": \"1\"}"}}},
    {"button", "recalibrate_nuki", "Recalibrate Nuki", nullptr, "", "", "diagnostic", mqtt_topic_config_action, HassAcl::Advanced, 21, -1, 0, {{"en", "true"}, {"pl_prs", "{ \"recalibrateNuki\": \"1\"}"}}},
};

static const HassEntity lockSelects[] =
{
    {"select", "fob_action_1", "Fob action 1", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 10, -1, 0, {{"val_tpl", "{{value_json.fobAction1}}"}, {"en", "true"}, {"cmd_tpl", "{ \"fobAction1\": \"{{ value }}\" }"}}, lockFobOptions},
    {"select", "fob_action_2", "Fob action 2", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 11, -1, 0, {{"val_tpl", "{{value_json.fobAction2}}"}, {"en", "true"}, {"cmd_tpl", "{ \"fobAction2\": \"{{ value }}\" }"}}, lockFobOptions},
    {"select", "fob_action_3", "Fob action 3", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 12, -1, 0, {{"val_tpl", "{{value_json.fobAction3}}"}, {"en", "true"}, {"cmd_tpl", "{ \"fobAction3\": \"{{ value }}\" }"}}, lockFobOptions},
    {"select", "advertising_mode", "Advertising mode", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 14, -1, 0, {{"val_tpl", "{{value_json.advertisingMode}}"}, {"en", "true"}, {"cmd_tpl", "{ \"advertisingMode\": \"{{ value }}\" }"}}, advertisingModeOptions},
    {"select", "timezone", "Timezone", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 15, -1, 0, {{"val_tpl", "{{value_json.timeZone}}"}, {"en", "true"}, {"cmd_tpl", "{ \"timeZone\": \"{{ value }}\" }"}}, timezoneOptions},
    {"select", "single_button_press_action", "Single button press action", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 5, -1, 0, {{"val_tpl", "{{value_json.singleButtonPressAction}}"}, {"en", "true"}, {"cmd_tpl", "{ \"singleButtonPressAction\": \"{{ value }}\" }"}}, lockButtonOptions},
    {"select", "double_button_press_action", "Double button press action", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 6, -1, 0, {{"val_tpl", "{{value_json.doubleButtonPressAction}}"}, {"en", "true"}, {"cmd_tpl", "{ \"doubleButtonPressAction\": \"{{ value }}\" }"}}, lockButtonOptions},
    {"select", "battery_type", "Battery type", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 8, -1, -1, {{"val_tpl", "{{value_json.batteryType}}"}, {"en", "true"}, {"cmd_tpl", "{ \"batteryType\": \"{{ value }}\" }"}}, batteryTypeOptions},
    {"select", "motor_speed", "Motor speed", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 23, -1, 1, {{"val_tpl", "{{value_json.motorSpeed}}"}, {"en", "true"}, {"cmd_tpl", "{ \"motorSpeed\": \"{{ value }}\" }"}}, motorSpeedOptions},
};

static const HassEntity openerSelects[] =
{
    {"select", "fob_action_1", "Fob action 1", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 8, -1, 0, {{"val_tpl", "{{value_json.fobAction1}}"}, {"en", "true"}, {"cmd_tpl", "{ \"fobAction1\": \"{{ value }}\" }"}}, openerFobOptions},
    {"select", "fob_action_2", "Fob action 2", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 9, -1, 0, {{"val_tpl", "{{value_json.fobAction2}}"}, {"en", "true"}, {"cmd_tpl", "{ \"fobAction2\": \"{{ value }}\" }"}}, openerFobOptions},
    {"select", "fob_action_3", "Fob action 3", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 10, -1, 0, {{"val_tpl", "{{value_json.fobAction3}}"}, {"en", "true"}, {"cmd_tpl", "{ \"fobAction3\": \"{{ value }}\" }"}}, openerFobOptions},
    {"select", "advertising_mode", "Advertising mode", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 12, -1, 0, {{"val_tpl", "{{value_json.advertisingMode}}"}, {"en", "true"}, {"cmd_tpl", "{ \"advertisingMode\": \"{{ value }}\" }"}}, advertisingModeOptions},
    {"select", "timezone", "Timezone", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 13, -1, 0, {{"val_tpl", "{{value_json.timeZone}}"}, {"en", "true"}, {"cmd_tpl", "{ \"timeZone\": \"{{ value }}\" }"}}, timezoneOptions},
    {"select", "operating_mode", "Operating mode", mqtt_topic_config_basic_json, "", "", "config", mqtt_topic_config_action, HassAcl::Basic, 11, -1, 0, {{"val_tpl", "{{value_json.operatingMode}}"}, {"en", "true"}, {"cmd_tpl", "{ \"operatingMode\": \"{{ value }}\" }"}}, operatingModeOptions},
    {"select", "doorbell_suppression", "Doorbell suppression", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 8, -1, 0, {{"val_tpl", "{{value_json.doorbellSuppression}}"}, {"en", "true"}, {"cmd_tpl", "{ \"doorbellSuppression\": \"{{ value }}\" }"}}, doorbellSuppressionOptions},
    {"select", "sound_ring", "Sound ring", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 10, -1, 0, {{"val_tpl", "{{value_json.soundRing}}"}, {"en", "true"}, {"cmd_tpl", "{ \"soundRing\": \"{{ value }}\" }"}}, soundOptions},
    {"select", "sound_open", "Sound open", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 11, -1, 0, {{"val_tpl", "{{value_json.soundOpen}}"}, {"en", "true"}, {"cmd_tpl", "{ \"soundOpen\": \"{{ value }}\" }"}}, soundOptions},
    {"select", "sound_rto", "Sound RTO", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 12, -1, 0, {{"val_tpl", "{{value_json.soundRto}}"}, {"en", "true"}, {"cmd_tpl", "{ \"soundRto\": \"{{ value }}\" }"}}, soundOptions},
    {"select", "sound_cm", "Sound CM", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 13, -1, 0, {{"val_tpl", "{{value_json.soundCm}}"}, {"en", "true"}, {"cmd_tpl", "{ \"soundCm\": \"{{ value }}\" }"}}, soundOptions},
    {"select", "single_button_press_action", "Single button press action", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 16, -1, 0, {{"val_tpl", "{{value_json.singleButtonPressAction}}"}, {"en", "true"}, {"cmd_tpl", "{ \"singleButtonPressAction\": \"{{ value }}\" }"}}, openerButtonOptions},
    {"select", "double_button_press_action", "Double button press action", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 17, -1, 0, {{"val_tpl", "{{value_json.doubleButtonPressAction}}"}, {"en", "true"}, {"cmd_tpl", "{ \"doubleButtonPressAction\": \"{{ value }}\" }"}}, openerButtonOptions},
    {"select", "battery_type", "Battery type", mqtt_topic_config_advanced_json, "", "", "config", mqtt_topic_config_action, HassAcl::Advanced, 18, -1, 0, {{"val_tpl", "{{value_json.batteryType}}"}, {"en", "true"}, {"cmd_tpl", "{ \"batteryType\": \"{{ value }}\" }"}}, batteryTypeOptions},
};

static const HassEntity hubEntities[] =
{
#ifndef CONFIG_IDF_TARGET_ESP32H2
    {"sensor", "wifi_signal_strength", "WIFI signal strength", mqtt_topic_wifi_rssi, "signal_strength", "measurement", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"unit_of_meas", "dBm"}}},
#endif
    {"binary_sensor", "mqtt_connected", "MQTT connected", mqtt_topic_mqtt_connection_state, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "online"}, {"pl_off", "offline"}, {"ic", "mdi:lan-connect"}}},
    {"sensor", "network_device", "Network device", mqtt_topic_network_device, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}}},
    {"switch", "webserver", "Webserver enabled", mqtt_topic_webserver_state, "", "", "diagnostic", mqtt_topic_webserver_action, HassAcl::None, 0, -1, 0, {{"pl_on", "1"}, {"pl_off", "0"}, {"stat_on", "1"}, {"stat_off", "0"}}},
    {"sensor", "uptime", "Uptime", mqtt_topic_uptime, "duration", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}, {"unit_of_meas", "min"}}},
    {"sensor", "mqtt_log", "MQTT Log", mqtt_topic_log, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}}, nullptr, preference_mqtt_log_enabled},
    {"sensor", "nuki_hub_version", "Version", mqtt_topic_info_nuki_hub_version, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}, {"ic", "mdi:counter"}}},
    {"sensor", "nuki_hub_build", "Build", mqtt_topic_info_nuki_hub_build, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}, {"ic", "mdi:counter"}}},
    {"sensor", "nuki_hub_restart_reason", "Restart reason", mqtt_topic_restart_reason_fw, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}}},
    {"sensor", "nuki_hub_restart_reason_esp", "Restart reason ESP", mqtt_topic_restart_reason_esp, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}}},
    {"sensor", "nuki_hub_latest", "Latest Version", mqtt_topic_info_nuki_hub_latest, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}, {"ic", "mdi:counter"}}, nullptr, preference_check_updates},
};

static const HassEntity hubIpEntity[] =
{
    {"sensor", "nuki_hub_ip", "IP", mqtt_topic_info_nuki_hub_ip, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}, {"ic", "mdi:ip"}}},
};

static const HassEntity deviceEntities[] =
{
    {"sensor", "firmware_version", "Firmware version", mqtt_topic_info_firmware_version, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}, {"ic", "mdi:counter"}}},
    {"sensor", "hardware_version", "Hardware version", mqtt_topic_info_hardware_version, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}, {"ic", "mdi:counter"}}},
    {"binary_sensor", "battery_low", "Battery low", mqtt_topic_battery_basic_json, "battery", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "1"}, {"pl_off", "0"}, {"val_tpl", "{{value_json.critical}}"}}},
    {"binary_sensor", "battery_charging", "Battery charging", mqtt_topic_battery_basic_json, "battery", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "1"}, {"pl_off", "0"}, {"val_tpl", "{{value_json.charging}}"}}},
    {"sensor", "battery_voltage", "Battery voltage", mqtt_topic_battery_advanced_json, "voltage", "measurement", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"unit_of_meas", "V"}, {"val_tpl", "{{value_json.batteryVoltage}}"}}},
    {"sensor", "trigger", "Trigger", mqtt_topic_lock_trigger, "", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"en", "true"}}},
    {"button", "query_lockstate", "Query lock state", nullptr, "", "", "diagnostic", mqtt_topic_query_lockstate, HassAcl::None, 0, -1, 0, {{"en", "false"}, {"pl_prs", "1"}}},
    {"button", "query_config", "Query config", nullptr, "", "", "diagnostic", mqtt_topic_query_config, HassAcl::None, 0, -1, 0, {{"en", "false"}, {"pl_prs", "1"}}},
    {"button", "query_commandresult", "Query lock state command result", nullptr, "", "", "diagnostic", mqtt_topic_query_lockstate_command_result, HassAcl::None, 0, -1, 0, {{"en", "false"}, {"pl_prs", "1"}}},
    {"sensor", "bluetooth_signal_strength", "Bluetooth signal strength", mqtt_topic_lock_rssi, "signal_strength", "measurement", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"unit_of_meas", "dBm"}}},
};

static const HassEntity doorSensorEntity[] =
{
    {"binary_sensor", "door_sensor", "Door sensor", mqtt_topic_lock_door_sensor_state, "door", "", "", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "doorOpened"}, {"pl_off", "doorClosed"}, {"pl_not_avail", "unavailable"}}},
};

static const HassEntity keypadEntities[] =
{
    {"binary_sensor", "keypad_battery_low", "Keypad battery low", mqtt_topic_battery_basic_json, "battery", "", "diagnostic", nullptr, HassAcl::None, 0, -1, 0, {{"pl_on", "1"}, {"pl_off", "0"}, {"val_tpl", "{{value_json.keypadCritical}}"}}},
    {"button", "query_keypad", "Query keypad", nullptr, "", "", "diagnostic", mqtt_topic_query_keypad, HassAcl::None, 0, -1, 0, {{"en", "false"}, {"pl_prs", "1"}}},
};

static const char* const removedEntities[][2] =
{
    {"lock", "smartlock"},
    {"binary_sensor", "battery_low"},
    {"binary_sensor", "battery_charging"},
    {"binary_sensor", "keypad_battery_low"},
    {"sensor", "battery_voltage"},
    {"sensor", "trigger"},
    {"binary_sensor", "mqtt_connected"},
    {"switch", "reset"},
    {"sensor", "firmware_version"},
    {"sensor", "hardware_version"},
    {"sensor", "nuki_hub_version"},
    {"sensor", "nuki_hub_build"},
    {"sensor", "nuki_hub_latest"},
    {"update", "nuki_hub_update"},
    {"sensor", "nuki_hub_ip"},
    {"binary_sensor", "door_sensor"},
    {"sensor", "sound_level"},
    {"sensor", "last_action_authorization"},
    {"sensor", "keypad_status"},
    {"sensor", "rolling_log"},
    {"sensor", "wifi_signal_strength"},
    {"sensor", "bluetooth_signal_strength"},
    {"button", "query_lockstate"},
    {"button", "query_config"},
    {"button", "query_keypad"},
    {"button", "query_commandresult"},
    {"select", "battery_type"},
    {"select", "double_button_press_action"},
    {"select", "single_button_press_action"},
    {"select", "sound_cm"},
    {"select", "sound_rto"},
    {"select", "sound_open"},
    {"select", "sound_ring"},
    {"select", "doorbell_suppression"},
    {"select", "operating_mode"},
    {"select", "timezone"},
    {"select", "advertising_mode"},
    {"select", "fob_action_3"},
    {"select", "fob_action_2"},
    {"select", "fob_action_1"},
    {"sensor", "network_device"},
    {"switch", "webserver"},
    {"sensor", "uptime"},
    {"sensor", "mqtt_log"},
    {"binary_sensor", "hybrid_connected"},
    {"sensor", "nuki_hub_restart_reason"},
    {"sensor", "nuki_hub_restart_reason_esp"},
};

#define HASS_COUNT(table) (sizeof(table) / sizeof(table[0]))

void HomeAssistantDiscovery::publishHASSNukiHubConfig()
{
    JsonDocument json;
    json.clear();
    JsonObject dev = json["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(String("nuki_") + _nukiHubUidString);
    json["dev"]["mf"] = "Technyon";
    json["dev"]["mdl"] = "Nuki Hub";
    json["dev"]["name"] = _hostname.c_str();
    json["dev"]["sw"] = NUKI_HUB_VERSION;
    json["dev"]["hw"] = NUKI_HUB_HW;
    _uidToName[_nukiHubUidString] = _hostname.c_str();

    String cuUrl = _preferences->getString(preference_mqtt_hass_cu_url, "");

    if (cuUrl != "")
    {
        json["dev"]["cu"] = cuUrl;
    }
    else
    {
        json["dev"]["cu"] = "http://" + _device->localIP();
    }

    json["~"] = _baseTopic;
    json["name"] = "Restart Nuki Hub";
    json["unique_id"] = String(_nukiHubUidString) + "_reset";
    json["avty"][0]["t"] = String("~") + mqtt_topic_mqtt_connection_state;
    json["opt"] = "false";
    json["stat_t"] = String("~") + mqtt_topic_reset;
    json["ent_cat"] = "diagnostic";
    json["cmd_t"] = String("~") + mqtt_topic_reset;
    json["ic"] = "mdi:restart";
    json["pl_on"] = "1";
    json["pl_off"] = "0";
    json["stat_on"] = "1";
    json["stat_off"] = "0";

    String path = _discoveryTopicOrDefault;
    path.concat("/switch/");
    path.concat(_nukiHubUidString);
    path.concat("/reset/config");

    publishConfig(path, json);

    publishHassEntities(hubEntities, HASS_COUNT(hubEntities), "NukiHub", _baseTopic.c_str(), _hostname.c_str(), _nukiHubUidString);

    if(_preferences->getBool(preference_check_updates, false))
    {
        // Nuki Hub update
        char latest_version_topic[250];
        _baseTopic.toCharArray(latest_version_topic,_baseTopic.length() + 1);
        strcat(latest_version_topic, mqtt_topic_info_nuki_hub_latest);

        if(!_preferences->getBool(preference_update_from_mqtt, false))
        {
            publishHassTopic("update",
                             "nuki_hub_update",
                             _nukiHubUidString,
                             "_nuki_hub_update",
                             "Firmware update",
                             _hostname.c_str(),
                             _baseTopic.c_str(),
                             String("~") + mqtt_topic_info_nuki_hub_version,
                             "NukiHub",
                             "firmware",
                             "",
                             "diagnostic",
                             "",
            {
                { (char*)"en", (char*)"true" },
                { (char*)"ent_pic", (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/master/icon/favicon-32x32.png" },
                { (char*)"rel_u", (char*)GITHUB_LATEST_RELEASE_URL },
                { (char*)"l_ver_t", (char*)latest_version_topic }
            });
        }
        else
        {
            publishHassTopic("update",
                             "nuki_hub_update",
                             _nukiHubUidString,
                             "_nuki_hub_update",
                             "Firmware update",
                             _hostname.c_str(),
                             _baseTopic.c_str(),
                             String("~") + mqtt_topic_info_nuki_hub_version,
                             "NukiHub",
                             "firmware",
                             "",
                             "diagnostic",
                             String("~") + mqtt_topic_update,
            {
                { (char*)"en", (char*)"true" },
                { (char*)"pl_inst", (char*)"1" },
                { (char*)"ent_pic", (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/master/icon/favicon-32x32.png" },
                { (char*)"rel_u", (char*)GITHUB_LATEST_RELEASE_URL },
                { (char*)"l_ver_t", (char*)latest_version_topic }
            });
        }
    }
    else
    {
        removeHassTopic((char*)"update", (char*)"nuki_hub_update", _nukiHubUidString);
    }

    publishHassEntities(hubIpEntity, HASS_COUNT(hubIpEntity), "NukiHub", _baseTopic.c_str(), _hostname.c_str(), _nukiHubUidString);
}

void HomeAssistantDiscovery::publishHASSConfig(char *deviceType, const char *baseTopic, char *name, char *uidString, const char *softwareVersion, const char *hardwareVersion, const bool& hasDoorSensor, const bool& hasKeypad, const bool& publishAuthData, char *lockAction, char *unlockAction, char *openAction)
{
    String availabilityTopic = _baseTopic;
    availabilityTopic.concat(mqtt_topic_mqtt_connection_state);

    publishHASSDeviceConfig(deviceType, baseTopic, name, uidString, softwareVersion, hardwareVersion, availabilityTopic.c_str(), hasKeypad, lockAction, unlockAction, openAction);

    if(strcmp(deviceType, "SmartLock") == 0)
    {
        publishHASSConfigAdditionalLockEntities(deviceType, baseTopic, name, uidString);
    }
    else
    {
        publishHASSConfigAdditionalOpenerEntities(deviceType, baseTopic, name, uidString);
    }
    if(hasDoorSensor)
    {
        publishHASSConfigDoorSensor(deviceType, baseTopic, name, uidString);
    }
    else
    {
        removeHASSConfigTopic((char*)"binary_sensor", (char*)"door_sensor", uidString);
    }
    if(publishAuthData)
    {
        publishHASSConfigAccessLog(deviceType, baseTopic, name, uidString);
        removeHASSConfigTopic((char*)"sensor", (char*)"last_action_authorization", uidString);
    }
    else
    {
        removeHASSConfigTopic((char*)"sensor", (char*)"rolling_log", uidString);
    }
    if(hasKeypad)
    {
        publishHASSConfigKeypad(deviceType, baseTopic, name, uidString);
        removeHASSConfigTopic((char*)"sensor", (char*)"keypad_status", uidString);
    }
    else
    {
        removeHASSConfigTopic((char*)"binary_sensor", (char*)"keypad_battery_low", uidString);
    }
}

void HomeAssistantDiscovery::publishHASSDeviceConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const char *softwareVersion, const char *hardwareVersion, const char* availabilityTopic, const bool& hasKeypad, char* lockAction, char* unlockAction, char* openAction)
{
    JsonDocument json;
    json.clear();
    JsonObject dev = json["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(String("nuki_") + uidString);
    json["dev"]["mf"] = "Nuki";
    json["dev"]["mdl"] = deviceType;
    json["dev"]["name"] = name;
    json["dev"]["sw"] = softwareVersion;
    json["dev"]["hw"] = hardwareVersion;
    json["dev"]["via_device"] = String("nuki_") + _nukiHubUidString;
    _uidToName[uidString] = name;

    String cuUrl = _preferences->getString(preference_mqtt_hass_cu_url, "");

    if (cuUrl != "")
    {
        json["dev"]["cu"] = cuUrl;
    }
    else
    {
        json["dev"]["cu"] = "http://" + _device->localIP();
    }

    json["~"] = baseTopic;
    json["name"] = nullptr;
    json["unique_id"] = String(uidString) + "_lock";
    json["cmd_t"] = String("~") + String(mqtt_topic_lock_action);
    json["avty"][0]["t"] = availabilityTopic;
    json["avty"][1]["t"] = String("~") + String(mqtt_topic_lock_availability);
    json["avty_mode"] = "all";
    json["pl_lock"] = lockAction;
    json["pl_unlk"] = unlockAction;

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    if((strcmp(deviceType, "SmartLock") == 0 && (int)aclPrefs[2]) || (strcmp(deviceType, "SmartLock") != 0 && (int)aclPrefs[11]))
    {
        json["pl_open"] = openAction;
    }

    json["stat_t"] = String("~") + mqtt_topic_lock_ha_state;
    json["stat_jam"] = "jammed";
    json["stat_locked"] = "locked";
    json["stat_locking"] = "locking";
    json["stat_unlocked"] = "unlocked";
    json["stat_unlocking"] = "unlocking";
    json["stat_open"] = "open";
    json["stat_opening"] = "opening";
    json["opt"] = "false";

    String path = _discoveryTopicOrDefault;
    path.concat("/lock/");
    path.concat(uidString);
    path.concat("/smartlock/config");

    publishConfig(path, json);


    publishHassEntities(deviceEntities, 6, deviceType, baseTopic, name, uidString);

    if(_preferences->getBool(preference_official_hybrid_enabled, false) && strcmp(deviceType, "SmartLock") == 0)
    {
        // Hybrid connected
        String hybridPath = _baseTopic;
        hybridPath.concat("/lock");
        hybridPath.concat(mqtt_topic_hybrid_state);
        publishHassTopic("binary_sensor",
                         "hybrid_connected",
                         uidString,
                         "_hybrid_connected",
                         "Hybrid connected",
                         name,
                         baseTopic,
                         hybridPath,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
        {
            {(char*)"pl_on", (char*)"1"},
            {(char*)"pl_off", (char*)"0"},
            { (char*)"en", (char*)"true" }
        });
    }
    else
    {
        removeHassTopic((char*)"binary_sensor", (char*)"hybrid_connected", uidString);
    }

    publishHassEntities(deviceEntities + 6, HASS_COUNT(deviceEntities) - 6, deviceType, baseTopic, name, uidString);
}

void HomeAssistantDiscovery::publishHassEntities(const HassEntity* entities, size_t count, const char* deviceType, const char* baseTopic, const char* name, const char* uidString, const uint32_t* aclPrefs, const uint32_t* basicAclPrefs, const uint32_t* advancedAclPrefs)
{
    bool geminiEnabled = _preferences->getBool(preference_lock_gemini_enabled, false);

    for(size_t i = 0; i < count; i++)
    {
        const HassEntity& entity = entities[i];
        bool allowed = true;

        switch(entity.acl)
        {
        case HassAcl::Action:
            allowed = aclPrefs[entity.aclIndex] != 0 && (entity.aclIndex2 < 0 || aclPrefs[entity.aclIndex2] != 0);
            break;
        case HassAcl::Basic:
            allowed = basicAclPrefs[entity.aclIndex] == 1;
            break;
        case HassAcl::Advanced:
            allowed = advancedAclPrefs[entity.aclIndex] == 1;
            break;
        default:
            break;
        }

        if(entity.preference != nullptr)
        {
            allowed = allowed && _preferences->getBool(entity.preference, false);
        }

        if(entity.gemini > 0)
        {
            allowed = allowed && geminiEnabled;
        }
        else if(entity.gemini < 0)
        {
            allowed = allowed && !geminiEnabled;
        }

        if(!allowed)
        {
            removeHassTopic(entity.component, entity.name, uidString);
            continue;
        }

        publishHassEntity(entity, deviceType, baseTopic, name, uidString);
    }
}

void HomeAssistantDiscovery::publishHassEntity(const HassEntity& entity, const char* deviceType, const char* baseTopic, const char* name, const char* uidString)
{
    String uidStringPostfix = String("_") + entity.name;
    String stateTopic = entity.stateTopic != nullptr ? String("~") + entity.stateTopic : String("");
    String commandTopic = entity.commandTopic != nullptr ? String("~") + entity.commandTopic : String("");

    if(entity.options == nullptr)
    {
        publishHassTopic(entity.component, entity.name, uidString, uidStringPostfix, entity.displayName, name, baseTopic, stateTopic, deviceType, entity.deviceClass, entity.stateClass, entity.entityCategory, commandTopic, entity.additionalEntries);
        return;
    }

    if (_discoveryTopic != "")
    {
        HassLock lock(_mutex);
        String path = createHassTopicPath(entity.component, entity.name, uidString);
        JsonDocument json;
        if(needsPayload(path))
        {
            json = createHassJson(uidString, uidStringPostfix, entity.displayName, name, baseTopic, stateTopic, deviceType, entity.deviceClass, entity.stateClass, entity.entityCategory, commandTopic, entity.additionalEntries);
            for(size_t i = 0; entity.options[i] != nullptr; i++)
            {
                json["options"][i] = entity.options[i];
            }
        }
        publishConfig(path, json);
    }
}

void HomeAssistantDiscovery::publishHASSConfigAdditionalLockEntities(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedLockConfigAclPrefs[26] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if(_preferences->getBool(preference_conf_info_enabled, true))
    {
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
    }

    publishHassEntities(lockEntities, sizeof(lockEntities) / sizeof(lockEntities[0]), deviceType, baseTopic, name, uidString, aclPrefs, basicLockConfigAclPrefs, advancedLockConfigAclPrefs);

    publishHassEntities(lockSelects, HASS_COUNT(lockSelects), deviceType, baseTopic, name, uidString, aclPrefs, basicLockConfigAclPrefs, advancedLockConfigAclPrefs);
}

void HomeAssistantDiscovery::publishHASSConfigDoorSensor(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassEntities(doorSensorEntity, HASS_COUNT(doorSensorEntity), deviceType, baseTopic, name, uidString);
}

void HomeAssistantDiscovery::publishHASSConfigAdditionalOpenerEntities(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedOpenerConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if(_preferences->getBool(preference_conf_info_enabled, true))
    {
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));
    }

    publishHassEntities(openerEntities, sizeof(openerEntities) / sizeof(openerEntities[0]), deviceType, baseTopic, name, uidString, aclPrefs, basicOpenerConfigAclPrefs, advancedOpenerConfigAclPrefs);

    JsonDocument json;

    json = createHassJson(uidString, "_ring_event", "Ring", name, baseTopic, String("~") + mqtt_topic_lock_ring, deviceType, "doorbell", "", "", "", {{(char*)"val_tpl", (char*)"{ \"event_type\": \"{{ value }}\" }"}});

    json["event_types"][0] = "ring";

    json["event_types"][1] = "ringlocked";

    json["event_types"][2] = "standby";

    String path = createHassTopicPath("event", "ring", uidString);

    publishConfig(path, json);

    publishHassEntities(openerSelects, HASS_COUNT(openerSelects), deviceType, baseTopic, name, uidString, aclPrefs, basicOpenerConfigAclPrefs, advancedOpenerConfigAclPrefs);
}

void HomeAssistantDiscovery::publishHASSConfigAccessLog(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    String rollingSate = "~";
    rollingSate.concat(mqtt_topic_lock_log_rolling);

    publishHassTopic("sensor",
                     "rolling_log",
                     uidString,
                     "_rolling_log",
                     "Rolling authorization log",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_log_rolling,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
    {
        { (char*)"ic", (char*)"mdi:format-list-bulleted" },
        { (char*)"json_attr_t", (char*)rollingSate.c_str() },
        { (char*)"val_tpl", (char*)"{{value_json.index}}" }
    });
}

void HomeAssistantDiscovery::publishHASSConfigKeypad(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassEntities(keypadEntities, HASS_COUNT(keypadEntities), deviceType, baseTopic, name, uidString);
}

void HomeAssistantDiscovery::publishHassTopic(const String& mqttDeviceType,
        const String& mqttDeviceName,
        const String& uidString,
        const String& uidStringPostfix,
        const String& displayName,
        const String& name,
        const String& baseTopic,
        const String& stateTopic,
        const String& deviceType,
        const String& deviceClass,
        const String& stateClass,
        const String& entityCat,
        const String& commandTopic,
        std::initializer_list<HassKv> additionalEntries
                                             )
{
    if (_discoveryTopic != "")
    {
        HassLock lock(_mutex);
        String path = createHassTopicPath(mqttDeviceType, mqttDeviceName, uidString);
        JsonDocument json;
        if(needsPayload(path))
        {
            json = createHassJson(uidString, uidStringPostfix, displayName, name, baseTopic, stateTopic, deviceType, deviceClass, stateClass, entityCat, commandTopic, additionalEntries);
        }
        publishConfig(path, json);
    }
}

String HomeAssistantDiscovery::createHassTopicPath(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString)
{
    String path = _discoveryTopic;
    path.concat("/");
    path.concat(mqttDeviceType);
    path.concat("/");
    path.concat(uidString);
    path.concat("/");
    path.concat(mqttDeviceName);
    path.concat("/config");

    return path;
}

void HomeAssistantDiscovery::removeHassTopic(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString)
{
    if (_discoveryTopic != "")
    {
        HassLock lock(_mutex);
        String path = createHassTopicPath(mqttDeviceType, mqttDeviceName, uidString);
        removeConfig(path);
    }
}

void HomeAssistantDiscovery::removeHASSConfig(char* uidString)
{
    Log->println("Removing HASS entities with ID:");
    Log->println(uidString);

    for(const auto& entity : removedEntities)
    {
        removeHassTopic(entity[0], entity[1], uidString);
    }

    for(const HassEntity& entity : lockEntities)
    {
        removeHassTopic(entity.component, entity.name, uidString);
    }
    for(const HassEntity& entity : openerEntities)
    {
        removeHassTopic(entity.component, entity.name, uidString);
    }

    if (_discoveryTopic != "")
    {
        for(int i = 0; i < HASS_DEVICE_BUCKETS_NUKI; i++)
        {
            removeConfig(chunkTopic(uidString, i));
        }
    }
}

void HomeAssistantDiscovery::removeHASSConfigTopic(char *deviceType, char *name, char *uidString)
{
    removeHassTopic(deviceType, name, uidString);
}

JsonDocument HomeAssistantDiscovery::createHassJson(const String& uidString,
        const String& uidStringPostfix,
        const String& displayName,
        const String& name,
        const String& baseTopic,
        const String& stateTopic,
        const String& deviceType,
        const String& deviceClass,
        const String& stateClass,
        const String& entityCat,
        const String& commandTopic,
        std::initializer_list<HassKv> additionalEntries
                                                   )
{
    JsonDocument json;
    json.clear();
    JsonObject dev = json["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(String("nuki_") + uidString);

    json["~"] = baseTopic;
    json["name"] = displayName;
    json["unique_id"] = String(uidString) + uidStringPostfix;

    if(deviceClass != "")
    {
        json["dev_cla"] = deviceClass;
    }

    if(stateTopic != "")
    {
        json["stat_t"] = stateTopic;
    }

    if(stateClass != "")
    {
        json["stat_cla"] = stateClass;
    }

    if(entityCat != "")
    {
        json["ent_cat"] = entityCat;
    }

    if(commandTopic != "")
    {
        json["cmd_t"] = commandTopic;
    }

    json["dev"]["name"] = _uidToName[uidString];

    if (displayName != "Rolling authorization log")
    {
        json["avty"][0]["t"] = _baseTopic + mqtt_topic_mqtt_connection_state;

        if (displayName != "Query lock state" && uidString.length() < 10)
        {
            json["avty"][1]["t"] = String("~") + String(mqtt_topic_lock_availability);
            json["avty_mode"] = "all";
        }
    }

    for(const auto& entry : additionalEntries)
    {
        if(strcmp(entry.second, "true") == 0)
        {
            json[entry.first] = true;
        }
        else if(strcmp(entry.second, "false") == 0)
        {
            json[entry.first] = false;
        }
        else
        {
            json[entry.first] = entry.second;
        }
    }

    return json;
}
