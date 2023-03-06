#pragma once

#include "MqttClient.h"
#include "MqttClientSetup.h"
#include "IPConfiguration.h"

enum class ReconnectStatus
{
    Failure = 0,
    Success = 1,
    CriticalFailure = 2
};

class NetworkDevice
{
public:
    explicit NetworkDevice(const String& hostname, const IPConfiguration* ipConfiguration)
    : _hostname(hostname),
      _ipConfiguration(ipConfiguration)
    {}

    virtual const String deviceName() const = 0;

    virtual void initialize() = 0;
    virtual ReconnectStatus reconnect() = 0;
    virtual void reconfigure() = 0;
    virtual void printError() = 0;
    virtual bool supportsEncryption() = 0;

    virtual void update() = 0;

    virtual bool isConnected() = 0;
    virtual int8_t signalStrength() = 0;

    virtual void mqttSetClientId(const char* clientId) = 0;
    virtual void mqttSetCleanSession(bool cleanSession) = 0;
    virtual uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const char* payload) = 0;
    virtual uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const uint8_t* payload, size_t length) = 0;
    virtual bool mqttConnected() const = 0;
    virtual void mqttSetServer(const char* host, uint16_t port) = 0;
    virtual bool mqttConnect() = 0;
    virtual bool mqttDisonnect(bool force) = 0;
    virtual void mqttSetCredentials(const char* username, const char* password) = 0;
    virtual void mqttOnMessage(espMqttClientTypes::OnMessageCallback callback) = 0;
    virtual void mqttOnConnect(espMqttClientTypes::OnConnectCallback callback) = 0;
    virtual void mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback) = 0;
    virtual void disableMqtt() = 0;

    virtual uint16_t mqttSubscribe(const char* topic, uint8_t qos) = 0;

protected:
    const uint16_t _mqttMaxBufferSize = 6144;
    const String _hostname;
    const IPConfiguration* _ipConfiguration = nullptr;
};