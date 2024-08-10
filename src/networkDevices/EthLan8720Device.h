#pragma once

#include "LAN8720Definitions.h"
#include "DM9051Definitions.h"
#include <WiFiClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#ifndef NUKI_HUB_UPDATER
#include "espMqttClient.h"
#endif
#include <ETH.h>
#include <cstdint>
#include <hal/spi_types.h>
#include <SPI.h>

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

    EthLan8720Device(const String& hostname,
                     Preferences* preferences,
                     const IPConfiguration* ipConfiguration,
                     const std::string& deviceName,
                     uint8_t phy_addr,
                     int cs, //  ETH_PHY_CS_ETH01EVO
                     int irq, // ETH_PHY_IRQ_ETH01EVO
                     int rst, // ETH_PHY_RST_ETH01EVO
                     spi_host_device_t spi_host, // ETH_PHY_SPI_HOST_ETH01EVO
                     int spi_sck, // ETH_PHY_SPI_SCK_ETH01EVO
                     int spi_miso, // ETH_PHY_SPI_MISO_ETH01EVO
                     int spi_mosi, // ETH_PHY_SPI_MOSI_ETH01EVO
                     uint8_t spi_freq_mhz = ETH_PHY_SPI_FREQ_MHZ,
                     eth_phy_type_t ethtype = ETH_PHY_TYPE_DM9051);

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
    void init(Preferences* preferences);
    void onDisconnected();
    void waitForIpAddressWithTimeout();

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

    // DM9051
    int _cs;
    int _irq;
    int _rst;
    spi_host_device_t _spi_host;
    int _spi_sck;
    int _spi_miso;
    int _spi_mosi;
    uint8_t _spi_freq_mhz;

    eth_phy_type_t _type;
    eth_clock_mode_t _clock_mode;
    bool _use_mac_from_efuse;

    SPIClass* spi = nullptr;

    #ifndef NUKI_HUB_UPDATER
    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
    #endif
};