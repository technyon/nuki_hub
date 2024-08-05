//#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
//#define ETH_PHY_POWER 12

#include <WiFi.h>
#include <ETH.h>
#include "EthLan8720Device.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#ifndef NUKI_HUB_UPDATER
#include "../MqttTopics.h"
#include "espMqttClient.h"
#endif
#include "../RestartReason.h"

EthLan8720Device::EthLan8720Device(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration, const std::string& deviceName, uint8_t phy_addr, int power, int mdc, int mdio, eth_phy_type_t ethtype, eth_clock_mode_t clock_mode, bool use_mac_from_efuse)
: NetworkDevice(hostname, ipConfiguration),
  _deviceName(deviceName),
  _phy_addr(phy_addr),
  _power(power),
  _mdc(mdc),
  _mdio(mdio),
  _type(ethtype),
  _clock_mode(clock_mode),
  _use_mac_from_efuse(use_mac_from_efuse)
{
    _restartOnDisconnect = preferences->getBool(preference_restart_on_disconnect);

    #ifndef NUKI_HUB_UPDATER
    size_t caLength = preferences->getString(preference_mqtt_ca, _ca, TLS_CA_MAX_SIZE);
    size_t crtLength = preferences->getString(preference_mqtt_crt, _cert, TLS_CERT_MAX_SIZE);
    size_t keyLength = preferences->getString(preference_mqtt_key, _key, TLS_KEY_MAX_SIZE);

    _useEncryption = caLength > 1;  // length is 1 when empty

    if(_useEncryption)
    {
        Log->println(F("MQTT over TLS."));
        Log->println(_ca);
        _mqttClientSecure = new espMqttClientSecure(espMqttClientTypes::UseInternalTask::NO);
        _mqttClientSecure->setCACert(_ca);
        if(crtLength > 1 && keyLength > 1) // length is 1 when empty
        {
            Log->println(F("MQTT with client certificate."));
            Log->println(_cert);
            Log->println(_key);
            _mqttClientSecure->setCertificate(_cert);
            _mqttClientSecure->setPrivateKey(_key);
        }
    } else
    {
        Log->println(F("MQTT without TLS."));
        _mqttClient = new espMqttClient(espMqttClientTypes::UseInternalTask::NO);
    }

    if(preferences->getBool(preference_mqtt_log_enabled, false) || preferences->getBool(preference_webserial_enabled, false))
    {
        MqttLoggerMode mode;
      
        if(preferences->getBool(preference_mqtt_log_enabled, false) && preferences->getBool(preference_webserial_enabled, false)) mode = MqttLoggerMode::MqttAndSerialAndWeb;
        else if (preferences->getBool(preference_webserial_enabled, false)) mode = MqttLoggerMode::SerialAndWeb;
        else mode = MqttLoggerMode::MqttAndSerial;
        
        _path = new char[200];
        memset(_path, 0, sizeof(_path));

        String pathStr = preferences->getString(preference_mqtt_lock_path);
        pathStr.concat(mqtt_topic_log);
        strcpy(_path, pathStr.c_str());
        Log = new MqttLogger(*getMqttClient(), _path, mode);
    }
    #endif
}

const String EthLan8720Device::deviceName() const
{
    return _deviceName.c_str();
}

void EthLan8720Device::initialize()
{
    delay(250);

    WiFi.setHostname(_hostname.c_str());

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
    _hardwareInitialized = ETH.begin(_phy_addr, _power, _mdc, _mdio, _type, _clock_mode, _use_mac_from_efuse);
#elif CONFIG_IDF_TARGET_ESP32
    _hardwareInitialized = ETH.begin(_type, _phy_addr, _mdc, _mdio, _power, _clock_mode);
#else
    _hardwareInitialized = false;
#endif

    ETH.setHostname(_hostname.c_str());
    if(!_ipConfiguration->dhcpEnabled())
    {
        ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
    }

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
    {
        if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            onDisconnected();
        }
    });

    if(_ipConfiguration->dhcpEnabled())
    {
        waitForIpAddressWithTimeout();
    }
}

void EthLan8720Device::waitForIpAddressWithTimeout()
{
    int count = 100;

    Log->print(F("LAN8720: Obtaining IP address via DHCP"));

    while(count > 0 && localIP().equals("0.0.0.0"))
    {
        Log->print(F("."));
        count--;
        delay(100);
    }

    if(localIP().equals("0.0.0.0"))
    {
        Log->println(F(" Failed"));
    }
    else
    {
        Log->println(localIP());
    }
}

void EthLan8720Device::reconfigure()
{
    delay(200);
    restartEsp(RestartReason::ReconfigureLAN8720);
}

bool EthLan8720Device::supportsEncryption()
{
    return true;
}

bool EthLan8720Device::isConnected()
{
    return ETH.linkUp();
}

ReconnectStatus EthLan8720Device::reconnect(bool force)
{
    if(!_hardwareInitialized)
    {
        return ReconnectStatus::CriticalFailure;
    }
    delay(200);
    return isConnected() ? ReconnectStatus::Success : ReconnectStatus::Failure;
}

void EthLan8720Device::onDisconnected()
{
    if(_restartOnDisconnect && ((esp_timer_get_time() / 1000) > 60000)) restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    reconnect();
}

int8_t EthLan8720Device::signalStrength()
{
    return -1;
}

String EthLan8720Device::localIP()
{
    return ETH.localIP().toString();
}

String EthLan8720Device::BSSIDstr()
{
    return "";
}