#pragma once

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#include "WiFiManager.h"
#include "espMqttClient.h"
#include "IPConfiguration.h"

class WifiDevice : public NetworkDevice
{
public:
    WifiDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration);

    const String deviceName() const override;

    virtual void initialize();
    virtual void reconfigure();
    virtual ReconnectStatus reconnect();
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

    bool mqttDisconnect(bool force) override;

    void setWill(const char *topic, uint8_t qos, bool retain, const char *payload) override;

    void mqttSetCredentials(const char *username, const char *password) override;

    void mqttOnMessage(espMqttClientTypes::OnMessageCallback callback) override;

    void mqttOnConnect(espMqttClientTypes::OnConnectCallback callback) override;

    void mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback) override;

    uint16_t mqttSubscribe(const char *topic, uint8_t qos) override;

    void disableMqtt() override;

private:
    static void clearRtcInitVar(WiFiManager*);

    void onDisconnected();
    MqttClient *getMqttClient() const;

    WiFiManager _wm;
    espMqttClient* _mqttClient = nullptr;
    espMqttClientSecure* _mqttClientSecure = nullptr;
    Preferences* _preferences = nullptr;

    bool _restartOnDisconnect = false;
    bool _startAp = false;
    char* _path;
    bool _useEncryption = false;
    bool _mqttEnabled = true;

    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
};
