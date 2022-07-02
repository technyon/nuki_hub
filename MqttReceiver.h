#pragma once

#include <Arduino.h>

class MqttReceiver
{
public:
    virtual void onMqttDataReceived(char*& topic, byte*& payload, unsigned int& length) = 0;
};