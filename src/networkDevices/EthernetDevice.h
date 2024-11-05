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
                     eth_clock_mode_t clock_mode = ETH_CLK_MODE_LAN8720);

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
                     eth_phy_type_t ethtype);

    const String deviceName() const override;

    virtual void initialize();
    virtual void reconfigure();
    virtual void update();

    virtual void scan(bool passive = false, bool async = true);
    virtual bool isConnected();
    virtual bool isApOpen();

    int8_t signalStrength() override;

    String localIP() override;
    String BSSIDstr() override;

private:
    Preferences* _preferences;

    void onDisconnected();
    void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);

    bool _connected = false;
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

    int64_t _checkIpTs = -1;

    eth_phy_type_t _type;
    eth_clock_mode_t _clock_mode;
    bool _useSpi = false;
};