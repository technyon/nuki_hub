#include "EthernetDevice.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#include "../RestartReason.h"

EthernetDevice::EthernetDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration, const std::string& deviceName, uint8_t phy_addr, int power, int mdc, int mdio, eth_phy_type_t ethtype, eth_clock_mode_t clock_mode)
: NetworkDevice(hostname, ipConfiguration),
  _deviceName(deviceName),
  _phy_addr(phy_addr),
  _power(power),
  _mdc(mdc),
  _mdio(mdio),
  _type(ethtype),
  _clock_mode(clock_mode),
  _useSpi(false),
  _preferences(preferences)
{
    init();
}

EthernetDevice::EthernetDevice(const String &hostname,
                                   Preferences *preferences,
                                   const IPConfiguration *ipConfiguration,
                                   const std::string &deviceName,
                                   uint8_t phy_addr,
                                   int cs,
                                   int irq,
                                   int rst,
                                   int spi_sck,
                                   int spi_miso,
                                   int spi_mosi,
                                   eth_phy_type_t ethtype)
        : NetworkDevice(hostname, ipConfiguration),
          _deviceName(deviceName),
          _phy_addr(phy_addr),
          _cs(cs),
          _irq(irq),
          _rst(rst),
          _spi_sck(spi_sck),
          _spi_miso(spi_miso),
          _spi_mosi(spi_mosi),
          _type(ethtype),
          _useSpi(true),
          _preferences(preferences)
{
    init();
}

const String EthernetDevice::deviceName() const
{
    return _deviceName.c_str();
}

void EthernetDevice::initialize()
{
    delay(250);

    Log->println(F("Init Ethernet"));

    if(_useSpi)
    {
        Log->println(F("Use SPI"));
        SPI.begin(_spi_sck, _spi_miso, _spi_mosi);
        _hardwareInitialized = ETH.begin(_type, _phy_addr, _cs, _irq, _rst, SPI);
    }
    #ifdef CONFIG_IDF_TARGET_ESP32
    else
    {
        Log->println(F("Use RMII"));
        _hardwareInitialized = ETH.begin(_type, _phy_addr, _mdc, _mdio, _power, _clock_mode);
        if(!_ipConfiguration->dhcpEnabled())
        {
            _checkIpTs = (esp_timer_get_time() / 1000) + 2000;
        }
    }
    #endif

    if(_hardwareInitialized)
    {
        Log->println(F("Ethernet hardware Initialized"));

        if(_useSpi && !_ipConfiguration->dhcpEnabled())
        {
            ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
        }

        Network.onEvent([&](arduino_event_id_t event, arduino_event_info_t info)
        {
            onNetworkEvent(event, info);
        });
    }
    else
    {
        Log->println(F("Failed to initialize ethernet hardware"));
    }
}

void EthernetDevice::update()
{
    if(_checkIpTs != -1)
    {
        if(_ipConfiguration->ipAddress() != ETH.localIP())
        {
            Log->println(F("ETH Set static IP"));
            ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
            _checkIpTs = (esp_timer_get_time() / 1000) + 2000;
        }
        else
        {
            _checkIpTs = -1;
        }
    }
}


void EthernetDevice::onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Log->println("ETH Started");
            ETH.setHostname(_hostname.c_str());
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Log->println("ETH Connected");
            if(!localIP().equals("0.0.0.0"))
            {
                _connected = true;
            }
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Log->printf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif));
            Log->println(ETH);

            // For RMII devices, this check is handled in the update() method.
            if(_useSpi && !_ipConfiguration->dhcpEnabled() && _ipConfiguration->ipAddress() != ETH.localIP())
            {
                Log->printf("Static IP not used, retrying to set static IP");
                ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
                ETH.begin(_type, _phy_addr, _cs, _irq, _rst, SPI);
            }

            _connected = true;
            if(_preferences->getBool(preference_ntw_reconfigure, false))
            {
                _preferences->putBool(preference_ntw_reconfigure, false);
            }
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            Log->println("ETH Lost IP");
            _connected = false;
            onDisconnected();
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Log->println("ETH Disconnected");
            _connected = false;
            onDisconnected();
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Log->println("ETH Stopped");
            _connected = false;
            onDisconnected();
            break;
        default:
            Log->print("ETH Event: ");
            Log->println(event);
            break;
    }
}

void EthernetDevice::reconfigure()
{
    delay(200);
    restartEsp(RestartReason::ReconfigureETH);
}

bool EthernetDevice::isConnected()
{
    return _connected;
}

ReconnectStatus EthernetDevice::reconnect(bool force)
{
    if(!_hardwareInitialized)
    {
        return ReconnectStatus::CriticalFailure;
    }
    delay(200);
    return isConnected() ? ReconnectStatus::Success : ReconnectStatus::Failure;
}

void EthernetDevice::onDisconnected()
{
    if(_preferences->getBool(preference_restart_on_disconnect, false) && ((esp_timer_get_time() / 1000) > 60000)) restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    reconnect();
}

int8_t EthernetDevice::signalStrength()
{
    return -1;
}

String EthernetDevice::localIP()
{
    return ETH.localIP().toString();
}

String EthernetDevice::BSSIDstr()
{
    return "";
}
