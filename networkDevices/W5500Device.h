#pragma once

#include "NetworkDevice.h"
#include "espMqttClient.h"
#include "espMqttClientEthernet.h"
#include <Ethernet.h>
#include <Preferences.h>

class W5500Device : public NetworkDevice
{
public:
    explicit W5500Device(const String& hostname, Preferences* _preferences);
    ~W5500Device();

    virtual void initialize();
    virtual ReconnectStatus reconnect();
    virtual void reconfigure();
    virtual void printError();

    bool supportsEncryption() override;

    virtual void update();

    virtual bool isConnected();

    int8_t signalStrength() override;

    void mqttSetClientId(const char *clientId) override;

    void mqttSetCleanSession(bool cleanSession) override;

    uint16_t mqttPublish(const char *topic, uint8_t qos, bool retain, const char *payload) override;

    uint16_t mqttPublish(const char *topic, uint8_t qos, bool retain, const uint8_t *payload, size_t length) override;

    bool mqttConnected() const override;

    void mqttSetServer(const char *host, uint16_t port) override;

    bool mqttConnect() override;

    bool mqttDisonnect(bool force) override;

    void mqttSetCredentials(const char *username, const char *password) override;

    void mqttOnMessage(espMqttClientTypes::OnMessageCallback callback) override;

    void mqttOnConnect(espMqttClientTypes::OnConnectCallback callback) override;

    void mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback) override;

    uint16_t mqttSubscribe(const char *topic, uint8_t qos) override;

private:
    void resetDevice();
    void initializeMacAddress(byte* mac);

    espMqttClientEthernet _mqttClient;
    Preferences* _preferences = nullptr;

    int _maintainResult = 0;
    bool _hasDHCPAddress = false;
    char* _path;

    byte _mac[6];
};