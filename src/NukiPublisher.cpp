#include "NukiPublisher.h"


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

bool NukiPublisher::publishString(const char *topic, const String &value, bool retain)
{
    char str[value.length() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.begin(), value.length());
    return publishString(topic, str, retain);
}

bool NukiPublisher::publishString(const char *topic, const std::string &value, bool retain)
{
    char str[value.size() + 1];
    memset(str, 0, sizeof(str));
    memcpy(str, value.data(), value.length());
    return publishString(topic, str, retain);
}

bool NukiPublisher::publishString(const char *topic, const char *value, bool retain)
{
    return _network->publishString(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishULong(const char *topic, const unsigned long value, bool retain)
{
    return _network->publishULong(_mqttPath, topic, value, retain);
}

void NukiPublisher::publishLongLong(const char *topic, int64_t value, bool retain)
{
    return _network->publishLongLong(_mqttPath, topic, value, retain);
}
