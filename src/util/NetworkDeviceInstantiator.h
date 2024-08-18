#pragma once

#include "../networkDevices/NetworkDevice.h"
#include "../enums/NetworkDeviceType.h"
#include <string>
#include <Preferences.h>

class NetworkDeviceInstantiator
{
public:
    static NetworkDevice* Create(NetworkDeviceType networkDeviceType, String hostname, Preferences* preferences, IPConfiguration* ipConfiguration);
};
