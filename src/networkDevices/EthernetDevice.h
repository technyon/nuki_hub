#pragma once

#include <ETH.h>
#include <cstdint>
#include <hal/spi_types.h>
#include <SPI.h>
#include "LAN8720Definitions.h"
#include "DM9051Definitions.h"
#include "W5500Definitions.h"
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#ifndef NUKI_HUB_UPDATER
#include "espMqttClient.h"
#endif

class EthernetDevice : public NetworkDevice
{

public:
    EthernetDevice(const String& hostname,
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

    EthernetDevice(const String& hostname,
                     Preferences* preferences,
                     const IPConfiguration* ipConfiguration,
                     const std::string& deviceName,
                     uint8_t phy_addr,
                     int cs,
                     int irq,
                     int rst,
                     int spi_sck,
                     int spi_miso,
                     int spi_mosi,
                     uint8_t spi_freq_mhz,
                     eth_phy_type_t ethtype);

    const String deviceName() const override;

    virtual void initialize();
    virtual void reconfigure();
    virtual void update();

    virtual ReconnectStatus reconnect(bool force = false);
    bool supportsEncryption() override;

    virtual bool isConnected();

    int8_t signalStrength() override;
    
    String localIP() override;
    String BSSIDstr() override;

private:
    Preferences* _preferences;
    
    void init();
    void onDisconnected();
    void waitForIpAddressWithTimeout();

    bool _connected = false;
    bool _restartOnDisconnect = false;
    bool _startAp = false;
    char* _path;
    bool _hardwareInitialized = false;

    const std::string _deviceName;
    uint8_t _phy_addr;

    // LAN8720
    int _power;
    int _mdc;
    int _mdio;

    // W55000 and DM9051
    int _cs;
    int _irq;
    int _rst;
    int _spi_sck;
    int _spi_miso;
    int _spi_mosi;
    uint8_t _spi_freq_mhz;

    int64_t _checkIpTs = -1;

    eth_phy_type_t _type;
    eth_clock_mode_t _clock_mode;
    bool _use_mac_from_efuse;
    bool _useSpi = false;

    #ifndef NUKI_HUB_UPDATER
    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
    #endif
};