//#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
//#define ETH_PHY_POWER 12

#include <WiFi.h>
#include <ETH.h>
#include "EthLan8720Device.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#include "../MqttTopics.h"
#include "espMqttClient.h"
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

    size_t caLength = preferences->getString(preference_mqtt_ca, _ca, TLS_CA_MAX_SIZE);
    size_t crtLength = preferences->getString(preference_mqtt_crt, _cert, TLS_CERT_MAX_SIZE);
    size_t keyLength = preferences->getString(preference_mqtt_key, _key, TLS_KEY_MAX_SIZE);

    _useEncryption = caLength > 1;  // length is 1 when empty

    if(_useEncryption)
    {
        Log->println(F("MQTT over TLS."));
        Log->println(_ca);
        _mqttClientSecure = new espMqttClientSecure(false);
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
        _mqttClient = new espMqttClient(false);
    }

    if(preferences->getBool(preference_mqtt_log_enabled))
    {
        _path = new char[200];
        memset(_path, 0, sizeof(_path));

        String pathStr = preferences->getString(preference_mqtt_lock_path);
        pathStr.concat(mqtt_topic_log);
        strcpy(_path, pathStr.c_str());
        Log = new MqttLogger(this, _path, MqttLoggerMode::MqttAndSerial);
    }
}

const String EthLan8720Device::deviceName() const
{
    return _deviceName.c_str();
}

void EthLan8720Device::initialize()
{
    delay(250);

    WiFi.setHostname(_hostname.c_str());
    _hardwareInitialized = ETH.begin(_phy_addr, _power, _mdc, _mdio, _type, _clock_mode, _use_mac_from_efuse);
    ETH.setHostname(_hostname.c_str());
    if(!_ipConfiguration->dhcpEnabled())
    {
        ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
    }

    if(_restartOnDisconnect)
    {
        WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
        {
            if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
            {
                onDisconnected();
            }
        });
    }
}

void EthLan8720Device::reconfigure()
{
    delay(200);
    restartEsp(RestartReason::ReconfigureLAN8720);
}

void EthLan8720Device::printError()
{
    Log->print(F("Free Heap: "));
    Log->println(ESP.getFreeHeap());
}

bool EthLan8720Device::supportsEncryption()
{
    return true;
}

bool EthLan8720Device::isConnected()
{
    bool connected = ETH.linkUp();

    if(_lastConnected == false && connected == true)
    {
        Serial.print(F("Ethernet connected. IP address: "));
        Serial.println(ETH.localIP().toString());
    }

    _lastConnected = connected;

    return connected;
}

ReconnectStatus EthLan8720Device::reconnect()
{
    if(!_hardwareInitialized)
    {
        return ReconnectStatus::CriticalFailure;
    }
    delay(200);
    return isConnected() ? ReconnectStatus::Success : ReconnectStatus::Failure;
}

void EthLan8720Device::update()
{
    if(_mqttEnabled)
    {
        if (_useEncryption)
        {
            _mqttClientSecure->loop();
        } else
        {
            _mqttClient->loop();
        }
    }
}

void EthLan8720Device::onDisconnected()
{
    if(millis() > 60000)
    {
        restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    }
}

int8_t EthLan8720Device::signalStrength()
{
    return -1;
}

void EthLan8720Device::mqttSetClientId(const char *clientId)
{
    if(_useEncryption)
    {
        _mqttClientSecure->setClientId(clientId);
    }
    else
    {
        _mqttClient->setClientId(clientId);
    }
}

void EthLan8720Device::mqttSetCleanSession(bool cleanSession)
{
    if(_useEncryption)
    {
        _mqttClientSecure->setCleanSession(cleanSession);
    }
    else
    {
        _mqttClient->setCleanSession(cleanSession);
    }
}

uint16_t EthLan8720Device::mqttPublish(const char *topic, uint8_t qos, bool retain, const char *payload)
{
    if(_useEncryption)
    {
        return _mqttClientSecure->publish(topic, qos, retain, payload);
    }
    else
    {
        return _mqttClient->publish(topic, qos, retain, payload);
    }
}

uint16_t EthLan8720Device::mqttPublish(const char *topic, uint8_t qos, bool retain, const uint8_t *payload, size_t length)
{
    if(_useEncryption)
    {
        return _mqttClientSecure->publish(topic, qos, retain, payload, length);
    }
    else
    {
        return _mqttClient->publish(topic, qos, retain, payload, length);
    }
}

bool EthLan8720Device::mqttConnected() const
{
    if(_useEncryption)
    {
        return _mqttClientSecure->connected();
    }
    else
    {
        return _mqttClient->connected();
    }
}

void EthLan8720Device::mqttSetServer(const char *host, uint16_t port)
{
    if(_useEncryption)
    {
        _mqttClientSecure->setServer(host, port);
    }
    else
    {
        _mqttClient->setServer(host, port);
    }
}

bool EthLan8720Device::mqttConnect()
{
    if(_useEncryption)
    {
        return _mqttClientSecure->connect();
    }
    else
    {
        return _mqttClient->connect();
    }
}

bool EthLan8720Device::mqttDisonnect(bool force)
{
    if(_useEncryption)
    {
        return _mqttClientSecure->disconnect(force);
    }
    else
    {
        return _mqttClient->disconnect(force);
    }
}

void EthLan8720Device::mqttSetCredentials(const char *username, const char *password)
{
    if(_useEncryption)
    {
        _mqttClientSecure->setCredentials(username, password);
    }
    else
    {
        _mqttClient->setCredentials(username, password);
    }
}

void EthLan8720Device::mqttOnMessage(espMqttClientTypes::OnMessageCallback callback)
{
    if(_useEncryption)
    {
        _mqttClientSecure->onMessage(callback);
    }
    else
    {
        _mqttClient->onMessage(callback);
    }
}


void EthLan8720Device::mqttOnConnect(espMqttClientTypes::OnConnectCallback callback)
{
    if(_useEncryption)
    {
        _mqttClientSecure->onConnect(callback);
    }
    else
    {
        _mqttClient->onConnect(callback);
    }
}

void EthLan8720Device::mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback)
{
    if(_useEncryption)
    {
        _mqttClientSecure->onDisconnect(callback);
    }
    else
    {
        _mqttClient->onDisconnect(callback);
    }
}


uint16_t EthLan8720Device::mqttSubscribe(const char *topic, uint8_t qos)
{
    if(_useEncryption)
    {
        return _mqttClientSecure->subscribe(topic, qos);
    }
    else
    {
        return _mqttClient->subscribe(topic, qos);
    }
}

void EthLan8720Device::disableMqtt()
{
    if (_useEncryption)
    {
        _mqttClientSecure->disconnect();
    } else
    {
        _mqttClient->disconnect();
    }

    _mqttEnabled = false;
}
