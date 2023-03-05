#pragma once

#include <Preferences.h>

class IPConfiguration
{
public:
    explicit IPConfiguration(Preferences* preferences);

    bool dhcpEnabled() const;
    const IPAddress ipAddress() const;
    const IPAddress subnet() const;
    const IPAddress defaultGateway() const;
    const IPAddress dnsServer() const;

private:
    Preferences* _preferences = nullptr;

    IPAddress _ipAddress;
    IPAddress _subnet;
    IPAddress _gateway;
    IPAddress _dnsServer;
};

