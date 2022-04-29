#include "W5500Device.h"
#include "../Pins.h"
#include "../PreferencesKeys.h"

W5500Device::W5500Device(const String &hostname, Preferences* preferences)
: NetworkDevice(hostname),
  _preferences(preferences)
{
    initializeMacAddress(_mac);

    Serial.print("MAC Adress: ");
    for(int i=0; i < 6; i++)
    {
        if(_mac[i] < 10)
        {
            Serial.print(F("0"));
        }
        Serial.print(_mac[i], 16);
        if(i < 5)
        {
            Serial.print(F(":"));
        }
    }
    Serial.println();
}

W5500Device::~W5500Device()
{}

void W5500Device::initialize()
{
    resetDevice();

    Ethernet.init(ETHERNET_CS_PIN);
    _ethClient = new EthernetClient();
    _mqttClient = new PubSubClient(*_ethClient);

    // start the Ethernet connection:
    Serial.println(F("Initialize Ethernet with DHCP:"));

    int dhcpRetryCnt = 0;

    while(dhcpRetryCnt < 3)
    {
        Serial.print(F("DHCP connect try #"));
        Serial.print(dhcpRetryCnt);
        Serial.println();
        dhcpRetryCnt++;

        if (Ethernet.begin(_mac, 1000, 1000) == 0)
        {
            Serial.println(F("Failed to configure Ethernet using DHCP"));
            // Check for Ethernet hardware present
            if (Ethernet.hardwareStatus() == EthernetNoHardware)
            {
                Serial.println(F("Ethernet module not found"));
                delay(10000);
                ESP.restart();
            }
            if (Ethernet.linkStatus() == LinkOFF)
            {
                Serial.println(F("Ethernet cable is not connected."));
            }

            IPAddress ip;
            ip.fromString("192.168.4.1");

            IPAddress subnet;
            subnet.fromString("255.255.255.0");

            // try to congifure using IP address instead of DHCP:
            Ethernet.begin(_mac, ip);
            Ethernet.setSubnetMask(subnet);

            delay(2000);
        }
        else
        {
            dhcpRetryCnt = 1000;
            Serial.print(F("  DHCP assigned IP "));
            Serial.println(Ethernet.localIP());
        }
    }
}

void W5500Device::reconfigure()
{
    Serial.println(F("Reconfigure W5500 not implemented."));
}

void W5500Device::resetDevice()
{
    Serial.println(F("Resetting network hardware."));
    pinMode(ETHERNET_RESET_PIN, OUTPUT);
    digitalWrite(ETHERNET_RESET_PIN, HIGH);
    delay(250);
    digitalWrite(ETHERNET_RESET_PIN, LOW);
    delay(50);
    digitalWrite(ETHERNET_RESET_PIN, HIGH);
    delay(1500);
}

PubSubClient *W5500Device::mqttClient()
{
    return _mqttClient;
}

bool W5500Device::isConnected()
{
    return _ethClient->connected();
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
