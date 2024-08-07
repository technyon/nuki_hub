#include <Arduino.h>
#include <WiFi.h>
#include "W5500Device.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#ifndef NUKI_HUB_UPDATER
#include "../MqttTopics.h"
#endif
#include "sdkconfig.h"

W5500Device::W5500Device(const String &hostname, Preferences* preferences, const IPConfiguration* ipConfiguration, int variant)
: NetworkDevice(hostname, ipConfiguration),
  _preferences(preferences),
  _variant((W5500Variant)variant)
{
    initializeMacAddress(_mac);

    Log->print("MAC Adress: ");
    for(int i=0; i < 6; i++)
    {
        if(_mac[i] < 10)
        {
            Log->print(F("0"));
        }
        Log->print(_mac[i], 16);
        if(i < 5)
        {
            Log->print(F(":"));
        }
    }
    Log->println();
    #ifndef NUKI_HUB_UPDATER
    _mqttClient = new espMqttClientW5500();
    #endif
}

W5500Device::~W5500Device()
{}

const String W5500Device::deviceName() const
{
    return "Wiznet W5500";
}

void W5500Device::initialize()
{
    WiFi.mode(WIFI_STA);

    resetDevice();

    switch(_variant)
    {
        case W5500Variant::M5StackAtomPoe:
            _resetPin = -1;
            #if defined(CONFIG_IDF_TARGET_ESP32S3)
            Ethernet.init(6, 5, 7, 8);
            #else
            Ethernet.init(19, 22, 23, 33);
            #endif
            break;
        default:
            _resetPin = -1;
            Ethernet.init(5);
            break;
    }

    #ifndef NUKI_HUB_UPDATER
    if(_preferences->getBool(preference_mqtt_log_enabled) || _preferences->getBool(preference_webserial_enabled))
    {
        MqttLoggerMode mode;
      
        if(_preferences->getBool(preference_mqtt_log_enabled) && _preferences->getBool(preference_webserial_enabled)) mode = MqttLoggerMode::MqttAndSerialAndWeb;
        else if (_preferences->getBool(preference_webserial_enabled)) mode = MqttLoggerMode::SerialAndWeb;
        else mode = MqttLoggerMode::MqttAndSerial;
        
        String pathStr = _preferences->getString(preference_mqtt_lock_path);
        pathStr.concat(mqtt_topic_log);
        _path = new char[pathStr.length() + 1];
        memset(_path, 0, sizeof(_path));
        strcpy(_path, pathStr.c_str());
        Log = new MqttLogger(*getMqttClient(), _path, mode);
    }
    #endif

    reconnect();
}

ReconnectStatus W5500Device::reconnect(bool force)
{
    _hasDHCPAddress = false;

    // start the Ethernet connection:
    Log->println(F("Initialize Ethernet with DHCP:"));

    int dhcpRetryCnt = 0;
    bool hardwareFound = false;

    while(dhcpRetryCnt < 3)
    {
        Log->print(F("DHCP connect try #"));
        Log->print(dhcpRetryCnt);
        Log->println();
        dhcpRetryCnt++;

        if (Ethernet.begin(_mac, 1000, 1000) == 0)
        {
            Log->println(F("Failed to configure Ethernet using DHCP"));
            // Check for Ethernet hardware present
            if (Ethernet.hardwareStatus() == EthernetNoHardware)
            {
                Log->println(F("Ethernet module not found"));
                continue;
            }
            if (Ethernet.linkStatus() == LinkOFF)
            {
                Log->println(F("Ethernet cable is not connected."));
            }

            hardwareFound = true;

            IPAddress ip;
            ip.fromString("192.168.4.1");

            IPAddress subnet;
            subnet.fromString("255.255.255.0");

            // try to congifure using IP address instead of DHCP:
            Ethernet.begin(_mac, ip);
            Ethernet.setSubnetMask(subnet);

            delay(1000);
        }
        else
        {
            hardwareFound = true;
            _hasDHCPAddress = true;
            dhcpRetryCnt = 1000;
            if(_ipConfiguration->dhcpEnabled())
            {
                Log->print(F("  DHCP assigned IP "));
                Log->println(Ethernet.localIP());
            }
        }

        if(!_ipConfiguration->dhcpEnabled())
        {
            Ethernet.setLocalIP(_ipConfiguration->ipAddress());
            Ethernet.setSubnetMask(_ipConfiguration->subnet());
            Ethernet.setGatewayIP(_ipConfiguration->defaultGateway());
            Ethernet.setDnsServerIP(_ipConfiguration->dnsServer());
        }
    }

    if(!hardwareFound)
    {
        return ReconnectStatus::CriticalFailure;
    }

    return _hasDHCPAddress ? ReconnectStatus::Success : ReconnectStatus::Failure;
}


void W5500Device::reconfigure()
{
    Log->println(F("Reconfigure W5500 not implemented."));
}

void W5500Device::resetDevice()
{
    if(_resetPin == -1) return;

    Log->println(F("Resetting network hardware."));
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, HIGH);
    delay(50);
    digitalWrite(_resetPin, LOW);
    delay(50);
    digitalWrite(_resetPin, HIGH);
    delay(50);
}

bool W5500Device::supportsEncryption()
{
    return false;
}

bool W5500Device::isConnected()
{
    return (Ethernet.linkStatus() == EthernetLinkStatus::LinkON && _maintainResult == 0 && _hasDHCPAddress);
}

void W5500Device::initializeMacAddress(byte *mac)
{
    memset(mac, 0, 6);

    mac[0] = 0x00;  // wiznet prefix
    mac[1] = 0x08;  // wiznet prefix
    mac[2] = 0xDC;  // wiznet prefix

    if(_preferences->getBool(preference_has_mac_saved))
    {
        mac[3] = _preferences->getChar(preference_has_mac_byte_0);
        mac[4] = _preferences->getChar(preference_has_mac_byte_1);
        mac[5] = _preferences->getChar(preference_has_mac_byte_2);
    }
    else
    {
        mac[3] = random(0,255);
        mac[4] = random(0,255);
        mac[5] = random(0,255);

        _preferences->putChar(preference_has_mac_byte_0, mac[3]);
        _preferences->putChar(preference_has_mac_byte_1, mac[4]);
        _preferences->putChar(preference_has_mac_byte_2, mac[5]);
        _preferences->putBool(preference_has_mac_saved, true);
    }
}

void W5500Device::update()
{
    _maintainResult = Ethernet.maintain();
    NetworkDevice::update();
}

int8_t W5500Device::signalStrength()
{
    return 127;
}

String W5500Device::localIP()
{
    return Ethernet.localIP().toString();
}

String W5500Device::BSSIDstr()
{
    return "";
}