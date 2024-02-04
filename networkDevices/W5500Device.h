#pragma once

#include "NetworkDevice.h"
#include "espMqttClient.h"
#include "espMqttClientW5500.h"
#include <Ethernet.h>
#include <Preferences.h>

enum class W5500Variant
{
    Generic = 2,
    M5StackAtomPoe = 3
};

class W5500Device : public NetworkDevice
{
public:
    explicit W5500Device(const String& hostname, Preferences* _preferences, const IPConfiguration* ipConfiguration, int variant);
    ~W5500Device();

    const String deviceName() const override;

    virtual void initialize();
    virtual ReconnectStatus reconnect();
    virtual void reconfigure();
    virtual void printError();

    bool supportsEncryption() override;

    virtual void update();

    virtual bool isConnected();

    int8_t signalStrength() override;
    
    String localIP() override;

    void mqttSetClientId(const char *clientId) override;

    void mqttSetCleanSession(bool cleanSession) override;

    uint16_t mqttPublish(const char *topic, uint8_t qos, bool retain, const char *payload) override;

    uint16_t mqttPublish(const char *topic, uint8_t qos, bool retain, const uint8_t *payload, size_t length) override;

    bool mqttConnected() const override;

    void mqttSetServer(const char *host, uint16_t port) override;

    bool mqttConnect() override;

    bool mqttDisconnect(bool force) override;

    void setWill(const char *topic, uint8_t qos, bool retain, const char *payload) override;

    void mqttSetCredentials(const char *username, const char *password) override;

    void mqttOnMessage(espMqttClientTypes::OnMessageCallback callback) override;

    void mqttOnConnect(espMqttClientTypes::OnConnectCallback callback) override;

    void mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback) override;

    uint16_t mqttSubscribe(const char *topic, uint8_t qos) override;

    void disableMqtt() override;

private:
    void resetDevice();
    void initializeMacAddress(byte* mac);

    espMqttClientW5500 _mqttClient;
    Preferences* _preferences = nullptr;

    int _maintainResult = 0;
    int _resetPin = -1;
    bool _hasDHCPAddress = false;
    char* _path;
    W5500Variant _variant;
    bool _lastConnected = false;
    bool _mqttEnabled = true;

    byte _mac[6];
};