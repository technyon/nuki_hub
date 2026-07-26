#include "NtpWrapper.h"
#include <functional>
#include "Logger.h"
using namespace std::placeholders;

extern bool timeSynced;

void NtpWrapper::initialize(String timeserver, NetworkDeviceType networkDeviceType)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(timeserver.c_str());
    config.start = false;
    config.server_from_dhcp = true;
    config.renew_servers_after_new_IP = true;
    config.index_of_first_server = 1;

    if (networkDeviceType == NetworkDeviceType::WiFi)
    {
        config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    }
    else
    {
        config.ip_event_to_renew = IP_EVENT_ETH_GOT_IP;
    }
    config.sync_cb = cbSyncTime;
    esp_netif_sntp_init(&config);
}

void NtpWrapper::enable()
{
    esp_netif_sntp_start();
}

void NtpWrapper::cbSyncTime(struct timeval* tv)
{
    Log->println("NTP time synced");
    timeSynced = true;
}
