#include "W5500Device.h"
#include "../Pins.h"

W5500Device::W5500Device(const String &hostname)
: NetworkDevice(hostname)
{
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

        byte mac[] = {0xB0,0xCD,0xAE,0x0F,0xDE,0x10};

        if (Ethernet.begin(mac, 1000, 1000) == 0)
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
            Ethernet.begin(mac, ip);
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
