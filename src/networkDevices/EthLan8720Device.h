#pragma once

#include "LAN8720Definitions.h"
#include <WiFiClient.h>
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
#include <WiFiClientSecure.h>
#else
#include <NetworkClientSecure.h>
#endif
#include <Preferences.h>
#include "NetworkDevice.h"
#ifndef NUKI_HUB_UPDATER
#include "espMqttClient.h"
#endif
#include <ETH.h>

class EthLan8720Device : public NetworkDevice
{

public:
    EthLan8720Device(const String& hostname,
                     Preferences* preferences,
                     const IPConfiguration* ipConfiguration,
                     const std::string& deviceName,
                     uint8_t phy_addr = ETH_PHY_ADDR_LAN8720,
                     int power = ETH_PHY_POWER_LAN8720,
                     int mdc = ETH_PHY_MDC_LAN8720,
                     int mdio = ETH_PHY_MDIO_LAN8720,
                     eth_phy_type_t ethtype = ETH_PHY_TYPE_LAN8720,
                     eth_clock_mode_t clock_mode = ETH_CLK_MODE_LAN8720,
                     bool use_mac_from_efuse = false);

    const String deviceName() const override;

    virtual void initialize();
    virtual void reconfigure();
    virtual ReconnectStatus reconnect(bool force = false);
    bool supportsEncryption() override;

    virtual bool isConnected();

    int8_t signalStrength() override;
    
    String localIP() override;
    String BSSIDstr() override;

private:
    void onDisconnected();
    void waitForIpAddressWithTimeout();

    bool _restartOnDisconnect = false;
    bool _startAp = false;
    char* _path;
    bool _hardwareInitialized = false;

    const std::string _deviceName;
    uint8_t _phy_addr;
    int _power;
    int _mdc;
    int _mdio;
    eth_phy_type_t _type;
    eth_clock_mode_t _clock_mode;
    bool _use_mac_from_efuse;
    #ifndef NUKI_HUB_UPDATER
    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
    #endif
};