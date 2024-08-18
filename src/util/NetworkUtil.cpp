#include "NetworkUtil.h"
#include <src/Logger.h>

NetworkDeviceType NetworkUtil::GetDeviceTypeFromPreference(int hardwareDetect, int customPhy)
{
    switch (hardwareDetect)
    {
        case 1:
            return NetworkDeviceType::WiFi;
            break;
        case 2:
            return NetworkDeviceType::W5500;
            break;
        case 3:
            return NetworkDeviceType::W5500M5;
            break;
        case 4:
            return NetworkDeviceType::Olimex_LAN8720;
            break;
        case 5:
            return NetworkDeviceType::WT32_LAN8720;
            break;
        case 6:
            return NetworkDeviceType::M5STACK_PoESP32_Unit;
            break;
        case 7:
            return NetworkDeviceType::LilyGO_T_ETH_POE;
            break;
        case 8:
            return NetworkDeviceType::GL_S10;
            break;
        case 9:
            return NetworkDeviceType::ETH01_Evo;
            break;
        case 10:
            return NetworkDeviceType::W5500M5S3;
            break;
        case 11:
            if(customPhy> 0)
            {
                return NetworkDeviceType::CUSTOM;
            }
            else
            {
                return NetworkDeviceType::WiFi;
            }
            break;
        default:
            Log->println(F("Unknown hardware selected, falling back to Wi-Fi."));
            return NetworkDeviceType::WiFi;
            break;
    }
}
