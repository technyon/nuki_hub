#pragma once

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#include "espMqttClient.h"
#include <ETH.h>

class EthLan8720Device : public NetworkDevice
{

public:
    EthLan8720Device(const String& hostname,
                     Preferences* preferences,
                     const IPConfiguration* ipConfiguration,
                     const std::string& deviceName,
                     uint8_t phy_addr = ETH_PHY_ADDR,
                     int power = ETH_PHY_POWER,
                     int mdc = ETH_PHY_MDC,
                     int mdio = ETH_PHY_MDIO,
                     eth_phy_type_t ethtype = ETH_PHY_TYPE,
                     eth_clock_mode_t clock_mode = ETH_CLK_MODE,
                     bool use_mac_from_efuse = false);

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

    bool mqttDisonnect(bool force) override;

    void setWill(const char *topic, uint8_t qos, bool retain, const char *payload) override;

    void mqttSetCredentials(const char *username, const char *password) override;

    void mqttOnMessage(espMqttClientTypes::OnMessageCallback callback) override;

    void mqttOnConnect(espMqttClientTypes::OnConnectCallback callback) override;

    void mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback) override;

    uint16_t mqttSubscribe(const char *topic, uint8_t qos) override;

    void disableMqtt() override;

private:
    void onDisconnected();

    espMqttClient* _mqttClient = nullptr;
    espMqttClientSecure* _mqttClientSecure = nullptr;

    bool _restartOnDisconnect = false;
    bool _startAp = false;
    char* _path;
    bool _useEncryption = false;
    bool _hardwareInitialized = false;
    bool _lastConnected = false;

    const std::string _deviceName;
    uint8_t _phy_addr;
    int _power;
    int _mdc;
    int _mdio;
    eth_phy_type_t _type;
    eth_clock_mode_t _clock_mode;
    bool _use_mac_from_efuse;
    bool _mqttEnabled = true;

    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
};
