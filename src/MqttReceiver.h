#pragma once

#include <Arduino.h>

class MqttReceiver
{
public:
    virtual void onMqttDataReceived(char* topic, int topic_len, char* data, int data_len) = 0;
};