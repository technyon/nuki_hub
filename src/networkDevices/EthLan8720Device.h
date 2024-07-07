#pragma once

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0, 0))
    #ifndef CONFIG_IDF_TARGET_ESP32
        typedef enum {
            ETH_CLOCK_GPIO0_IN = 0,
            ETH_CLOCK_GPIO16_OUT = 2,
            ETH_CLOCK_GPIO17_OUT = 3
        } eth_clock_mode_t;

        #define ETH_PHY_TYPE ETH_PHY_MAX
    #else
        #define ETH_PHY_TYPE        ETH_PHY_LAN8720
    #endif

#define ETH_CLK_MODE        ETH_CLOCK_GPIO0_IN

#define ETH_PHY_ADDR         0
#define ETH_PHY_MDC         23
#define ETH_PHY_MDIO        18
#define ETH_PHY_POWER       -1
#define ETH_RESET_PIN        1
#endif

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
    bool supportsEncryption() override;

    virtual bool isConnected();

    int8_t signalStrength() override;
    
    String localIP() override;
    String BSSIDstr() override;

private:
    void onDisconnected();

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