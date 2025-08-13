#include "esp_task_wdt.h"
#include "EthernetDevice.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#include "../RestartReason.h"
#include "../EspMillis.h"

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_private/esp_gpio_reserve.h"
#include <bootloader_common.h>
#include "esp_psram.h"
#include "esp32-hal.h"
#endif

extern bool ethCriticalFailure;
extern bool wifiFallback;

EthernetDevice::EthernetDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration, const std::string& deviceName, uint8_t phy_addr, int power, int mdc, int mdio, eth_phy_type_t ethtype, eth_clock_mode_t clock_mode)
    : NetworkDevice(hostname, preferences, ipConfiguration),
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
#ifndef NUKI_HUB_UPDATER
    NetworkDevice::init();
#endif
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
    : NetworkDevice(hostname, preferences, ipConfiguration),
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
#ifndef NUKI_HUB_UPDATER
    NetworkDevice::init();
#endif
}

const String EthernetDevice::deviceName() const
{
    return _deviceName.c_str();
}

void EthernetDevice::initialize()
{
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }
    vTaskDelay(250 / portTICK_PERIOD_MS);
    if(ethCriticalFailure)
    {
        ethCriticalFailure = false;
        Log->println("Failed to initialize ethernet hardware");
        Log->println("Network device has a critical failure, enable fallback to Wi-Fi and reboot.");
        wifiFallback = true;
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }

    Log->println("Init Ethernet");

    if(_useSpi)
    {
        Log->println("Use SPI");
        ethCriticalFailure = true;
        SPI.begin(_spi_sck, _spi_miso, _spi_mosi);
        _hardwareInitialized = ETH.begin(_type, _phy_addr, _cs, _irq, _rst, SPI);
        ethCriticalFailure = false;
    }
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
    else
    {
        Log->println("Use RMII");

        // Workaround for failing RMII initialization with pioarduino 3.1.0
        // Revoke all GPIO's some of them set by init PSRAM in IDF
        // sources:
        // https://github.com/arendst/Tasmota/commit/f8fbe153000591727e40b5007e0de78c33833131
        // https://github.com/arendst/Tasmota/commit/f8fbe153000591727e40b5007e0de78c33833131#diff-32fc0eefbf488dd507b3bef52189bbe37158737aba6f96fe98a8746dc5021955R417
        uint32_t pkg_version = bootloader_common_get_chip_ver_pkg();
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
        if(esp_psram_get_size() <= 0 && pkg_version <= 3)
#else
        if(pkg_version <= 3)
#endif
        {
            esp_gpio_revoke(0xFFFFFFFFFFFFFFFF);
        }

        ethCriticalFailure = true;
        _hardwareInitialized = ETH.begin(_type, _phy_addr, _mdc, _mdio, _power, _clock_mode);
        ethCriticalFailure = false;
        if(!_ipConfiguration->dhcpEnabled())
        {
            _checkIpTs = espMillis() + 2000;
        }
    }
#endif

    if(_hardwareInitialized)
    {
        Log->println("Ethernet hardware Initialized");
        wifiFallback = false;

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
        Log->println("Failed to initialize ethernet hardware");
        Log->println("Network device has a critical failure, enable fallback to Wi-Fi and reboot.");
        wifiFallback = true;
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }
}

void EthernetDevice::update()
{
    NetworkDevice::update();

    if(_checkIpTs != -1 && _checkIpTs < espMillis())
    {
        if(_ipConfiguration->ipAddress() != ETH.localIP())
        {
            Log->println("ETH Set static IP");
            ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
            _checkIpTs = espMillis() + 5000;
            return;
        }

        _checkIpTs = -1;
    }
}


void EthernetDevice::onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    switch (event)
    {
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
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
    restartEsp(RestartReason::ReconfigureETH);
}

void EthernetDevice::scan(bool passive, bool async)
{
}

bool EthernetDevice::isConnected()
{
    return _connected;
}

bool EthernetDevice::isApOpen()
{
    return false;
}

void EthernetDevice::onDisconnected()
{
    if(_preferences->getBool(preference_restart_on_disconnect, false) && (espMillis() > 60000))
    {
        restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    }
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
