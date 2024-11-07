#pragma once

#ifndef NUKI_HUB_UPDATER
#include "espMqttClient.h"
#endif
#include "IPConfiguration.h"

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
    virtual bool mqttConnect();
    virtual bool mqttDisconnect(bool force);
    virtual void mqttDisable();
    virtual bool mqttConnected() const;

    virtual uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const char* payload);
    virtual uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const uint8_t* payload, size_t length);
    virtual uint16_t mqttSubscribe(const char* topic, uint8_t qos);
    
    virtual void mqttSetServer(const char* host, uint16_t port);
    virtual void mqttSetClientId(const char* clientId);
    virtual void mqttSetCleanSession(bool cleanSession);
    virtual void mqttSetKeepAlive(uint16_t keepAlive);
    virtual void mqttSetWill(const char* topic, uint8_t qos, bool retain, const char* payload);
    virtual void mqttSetCredentials(const char* username, const char* password);
    
    virtual void mqttOnMessage(espMqttClientTypes::OnMessageCallback callback);
    virtual void mqttOnConnect(espMqttClientTypes::OnConnectCallback callback);
    virtual void mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback);
    #endif

protected:
    const IPConfiguration* _ipConfiguration = nullptr;
    Preferences* _preferences = nullptr;
    #ifndef NUKI_HUB_UPDATER
    espMqttClient *_mqttClient = nullptr;
    espMqttClientSecure *_mqttClientSecure = nullptr;

    void init();
    
    MqttClient *getMqttClient() const;

    bool _useEncryption = false;
    bool _mqttEnabled = true;
    char* _path;
    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
    #endif
    
    const String _hostname;
};