#pragma once

#ifndef NUKI_HUB_UPDATER
#include "espMqttClient.h"
#include "MqttClientSetup.h"
#endif
#include "IPConfiguration.h"
#include "../EspMillis.h"

class NetworkDevice
{
public:
    explicit NetworkDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration)
    : _hostname(hostname),
      _preferences(preferences),
      _ipConfiguration(ipConfiguration)
    {}

    virtual const String deviceName() const = 0;

    virtual void initialize() = 0;
    virtual void reconfigure() = 0;

    virtual void update();
    virtual void scan(bool passive = false, bool async = true) = 0;
    virtual bool isConnected() = 0;
    virtual bool isApOpen() = 0;
    virtual int8_t signalStrength() = 0;

    virtual String localIP() = 0;
    virtual String BSSIDstr() = 0;

    #ifndef NUKI_HUB_UPDATER
    virtual void mqttSetClientId(const char* clientId);
    virtual void mqttSetCleanSession(bool cleanSession);
    virtual void mqttSetKeepAlive(uint16_t keepAlive);
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
    #endif

protected:
    #ifndef NUKI_HUB_UPDATER
    espMqttClient *_mqttClient = nullptr;
    espMqttClientSecure *_mqttClientSecure = nullptr;

    bool _useEncryption = false;
    bool _mqttEnabled = true;

    void init();
    
    MqttClient *getMqttClient() const;

    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
    #endif
    
    const String _hostname;
    const IPConfiguration* _ipConfiguration = nullptr;
};