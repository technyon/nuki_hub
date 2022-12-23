#include <Arduino.h>
#include <WiFi.h>
#include "W5500Device.h"
#include "../Pins.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#include "../MqttTopics.h"

W5500Device::W5500Device(const String &hostname, Preferences* preferences)
: NetworkDevice(hostname),
  _preferences(preferences)
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
}

W5500Device::~W5500Device()
{}

void W5500Device::initialize()
{
    WiFi.mode(WIFI_OFF);

    resetDevice();

    Ethernet.init(ETHERNET_CS_PIN);
    _ethClient = new EthernetClient();
    _mqttClient = new PubSubClient(*_ethClient);
    _mqttClient->setBufferSize(_mqttMaxBufferSize);

    _path = new char[200];
    memset(_path, 0, sizeof(_path));

    String pathStr = _preferences->getString(preference_mqtt_lock_path);
    pathStr.concat(mqtt_topic_log);
    strcpy(_path, pathStr.c_str());
    Log = new MqttLogger(*_mqttClient, _path);

    reconnect();
}


bool W5500Device::reconnect()
{
    _hasDHCPAddress = false;

    // start the Ethernet connection:
    Log->println(F("Initialize Ethernet with DHCP:"));

    int dhcpRetryCnt = 0;

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
            }
            if (Ethernet.linkStatus() == LinkOFF)
            {
                Log->println(F("Ethernet cable is not connected."));
            }

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
            _hasDHCPAddress = true;
            dhcpRetryCnt = 1000;
            Log->print(F("  DHCP assigned IP "));
            Log->println(Ethernet.localIP());
        }
    }

    return _hasDHCPAddress;
}


void W5500Device::reconfigure()
{
    Log->println(F("Reconfigure W5500 not implemented."));
}

void W5500Device::resetDevice()
{
    Log->println(F("Resetting network hardware."));
    pinMode(ETHERNET_RESET_PIN, OUTPUT);
    digitalWrite(ETHERNET_RESET_PIN, HIGH);
    delay(250);
    digitalWrite(ETHERNET_RESET_PIN, LOW);
    delay(50);
    digitalWrite(ETHERNET_RESET_PIN, HIGH);
    delay(1500);
}


void W5500Device::printError()
{
    Log->print(F("Free Heap: "));
    Log->println(ESP.getFreeHeap());
}

PubSubClient *W5500Device::mqttClient()
{
    return _mqttClient;
}

bool W5500Device::isConnected()
{
    return Ethernet.linkStatus() == EthernetLinkStatus::LinkON && _maintainResult == 0 && _hasDHCPAddress;
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
}

int8_t W5500Device::signalStrength()
{
    return 127;
}
