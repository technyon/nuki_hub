#pragma once

#include <string>
#include <ETH.h>
#include "../enums/NetworkDeviceType.h"

class NetworkUtil
{
public:
    static NetworkDeviceType GetDeviceTypeFromPreference(int hardwareDetect, int customPhy);
    static std::string GetCustomEthernetDeviceName(int custPHY);
    static eth_phy_type_t GetCustomEthernetType(int custPHY);
    static eth_clock_mode_t GetCustomClock(int custCLKpref);
};