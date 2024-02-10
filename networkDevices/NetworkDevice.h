#pragma once

#include "espMqttClient.h"
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
    virtual void printError();
    virtual bool supportsEncryption() = 0;

    virtual void update();

    virtual bool isConnected() = 0;
    virtual int8_t signalStrength() = 0;
    
    virtual String localIP() = 0;

    virtual void mqttSetClientId(const char* clientId);
    virtual void mqttSetCleanSession(bool cleanSession);
    virtual uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const char* payload);
    virtual uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const uint8_t* payload, size_t length);
    virtual bool mqttConnected() const;
    virtual void mqttSetServer(const char* host, uint16_t port);
    virtual bool mqttConnect();
    virtual bool mqttDisconnect(bool force);
    virtual void setWill(const char* topic, uint8_t qos, bool retain, const char* payload);
    virtual void mqttSetCredentials(const char* username, const char* password);
    virtual void mqttOnMessage(espMqttClientTypes::OnMessageCallback callback);
    virtual void mqttOnConnect(espMqttClientTypes::OnConnectCallback callback);
    virtual void mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback);
    virtual void disableMqtt();

    virtual uint16_t mqttSubscribe(const char* topic, uint8_t qos);

protected:
    espMqttClient *_mqttClient = nullptr;
    espMqttClientSecure *_mqttClientSecure = nullptr;

    bool _useEncryption = false;
    bool _mqttEnabled = true;

    const String _hostname;
    const IPConfiguration* _ipConfiguration = nullptr;

    MqttClient *getMqttClient() const;
};