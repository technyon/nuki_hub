#pragma once

#include <src/enums/NetworkDeviceType.h>

class NetworkUtil
{
public:
    static NetworkDeviceType GetDeviceTypeFromPreference(int hardwareDetect, int customPhy);
};