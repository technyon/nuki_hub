#include <cstring>
#include "NukiPublisher.h"
#include "Logger.h"
#include "esp_heap_caps.h"


NukiPublisher::NukiPublisher(NukiNetwork *network, const char* mqttPath)
    : _network(network),
      _mqttPath(mqttPath)
{
}

void NukiPublisher::publishFloat(const char *topic, const float value, bool retain, const uint8_t precision)
{
    _network->publishFloat(_mqttPath, topic, value, retain, precision);
}

void NukiPublisher::publishInt(const char *topic, const int value, bool retain)
{
    _network->publishInt(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishUInt(const char *topic, const unsigned int value, bool retain)
{
    _network->publishUInt(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishBool(const char *topic, const bool value, bool retain)
{
    _network->publishBool(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishString(const char *topic, const String &value, bool retain)
{
    publishString(topic, value.c_str(), retain);
}

void NukiPublisher::publishString(const char *topic, const std::string &value, bool retain)
{
    publishString(topic, value.c_str(), retain);
}

void NukiPublisher::publishString(const char *topic, const char *value, bool retain)
{
    _network->publishString(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishULong(const char *topic, const unsigned long value, bool retain)
{
    _network->publishULong(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishLongLong(const char *topic, int64_t value, bool retain)
{
    _network->publishLongLong(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishJson(const char *topic, JsonDocument &json, char *buffer, size_t bufferSize, bool retain)
{
    size_t jsonSize = measureJson(json) + 1;

    if(jsonSize <= bufferSize)
    {
        serializeJson(json, buffer, bufferSize);
        publishString(topic, buffer, retain);
        return;
    }

    if(jsonSize >= heap_caps_get_largest_free_block(MALLOC_CAP_8BIT))
    {
        Log->print("Not enough free heap to publish ");
        Log->print(topic);
        Log->print(", needs ");
        Log->println(jsonSize);
        return;
    }

    std::string jsonString;
    jsonString.reserve(jsonSize);
    serializeJson(json, jsonString);
    publishString(topic, jsonString, retain);
}
