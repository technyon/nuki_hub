#include "IPConfiguration.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"

IPConfiguration::IPConfiguration(Preferences *preferences, const bool& firstStart)
: _preferences(preferences)
{
    if(firstStart)
    {
        _preferences->putBool(preference_ip_dhcp_enabled, true);
    }

    Log->print(F("IP configuration: "));
    if(dhcpEnabled())
    {
        Log->println(F("DHCP"));
    }
    else
    {
        Log->print(F("IP address: ")); Log->print(ipAddress());
        Log->print(F("Subnet: ")); Log->print(subnet());
        Log->print(F("Gateway: ")); Log->print(defaultGateway());
        Log->print(F("DNS: ")); Log->println(dnsServer());
    }
}

bool IPConfiguration::dhcpEnabled() const
{
    return _preferences->getBool(preference_ip_dhcp_enabled);
}

String IPConfiguration::ipAddress() const
{
    return _preferences->getString(preference_ip_address);
}

String IPConfiguration::subnet() const
{
    return _preferences->getString(preference_ip_subnet);
}

String IPConfiguration::defaultGateway() const
{
    return _preferences->getString(preference_ip_gateway);
}

String IPConfiguration::dnsServer() const
{
    return _preferences->getString(preference_ip_dns_server);
}
