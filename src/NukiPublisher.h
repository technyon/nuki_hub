#pragma once

#include <cstdint>
#include "NukiNetwork.h"

class NukiPublisher
{
public:
    NukiPublisher(NukiNetwork* _network, const char* mqttPath);

    void publishFloat(const char* topic, const float value, bool retain, const uint8_t precision = 2);
    void publishInt(const char* topic, const int value, bool retain);
    void publishUInt(const char* topic, const unsigned int value, bool retain);
    void publishULong(const char* topic, const unsigned long value, bool retain);
    void publishLongLong(const char* topic, int64_t value, bool retain);
    void publishBool(const char* topic, const bool value, bool retain);
    bool publishString(const char* topic, const String& value, bool retain);
    bool publishString(const char* topic, const std::string& value, bool retain);
    bool publishString(const char* topic, const char* value, bool retain);

private:
    NukiNetwork* _network;
    const char* _mqttPath;

};
