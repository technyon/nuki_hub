#include "IPConfiguration.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"

IPConfiguration::IPConfiguration(Preferences *preferences)
: _preferences(preferences)
{
    if(!dhcpEnabled() && _preferences->getString(preference_ip_address, "").length() <= 0)
    {
        Log->println("IP address empty, falling back to DHCP.");
        _preferences->putBool(preference_ip_dhcp_enabled, true);
    }

    _ipAddress.fromString(_preferences->getString(preference_ip_address, ""));
    _subnet.fromString(_preferences->getString(preference_ip_subnet, ""));
    _gateway.fromString(_preferences->getString(preference_ip_gateway, ""));
    _dnsServer.fromString(_preferences->getString(preference_ip_dns_server, ""));

    Log->print(F("IP configuration: "));
    if(dhcpEnabled())
    {
        Log->println(F("DHCP"));
    }
    else
    {
        Log->print(F("IP address: ")); Log->print(ipAddress());
        Log->print(F(", Subnet: ")); Log->print(subnet());
        Log->print(F(", Gateway: ")); Log->print(defaultGateway());
        Log->print(F(", DNS: ")); Log->println(dnsServer());
    }
}

bool IPConfiguration::dhcpEnabled() const
{
    return _preferences->getBool(preference_ip_dhcp_enabled, true);
}

const IPAddress IPConfiguration::ipAddress() const
{
    return _ipAddress;
}

const IPAddress IPConfiguration::subnet() const
{
    return _subnet;
}

const IPAddress IPConfiguration::defaultGateway() const
{
    return _gateway;
}

const IPAddress IPConfiguration::dnsServer() const
{
    return _dnsServer;
}
