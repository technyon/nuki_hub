#pragma once

#include <Preferences.h>

class IPConfiguration
{
public:
    explicit IPConfiguration(Preferences* preferences, const bool& firstStart);

    bool dhcpEnabled() const;
    String ipAddress() const;
    String subnet() const;
    String defaultGateway() const;
    String dnsServer() const;

private:
    Preferences* _preferences = nullptr;
};

