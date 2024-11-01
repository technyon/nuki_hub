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
