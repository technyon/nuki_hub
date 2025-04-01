#include "NetworkUtil.h"
#include "../Logger.h"
#include "../networkDevices/LAN8720Definitions.h"

NetworkDeviceType NetworkUtil::GetDeviceTypeFromPreference(int hardwareDetect, int customPhy)
{
    switch (hardwareDetect)
    {
    case 1:
        return NetworkDeviceType::WiFi;
    case 2:
        return NetworkDeviceType::W5500;
    case 3:
        return NetworkDeviceType::W5500M5;
    case 4:
        return NetworkDeviceType::Olimex_LAN8720;
    case 5:
        return NetworkDeviceType::WT32_LAN8720;
    case 6:
        return NetworkDeviceType::M5STACK_PoESP32_Unit;
    case 7:
        return NetworkDeviceType::LilyGO_T_ETH_POE;
    case 8:
        return NetworkDeviceType::GL_S10;
    case 9:
        return NetworkDeviceType::ETH01_Evo;
    case 10:
        return NetworkDeviceType::W5500M5S3;
    case 11:
        if(customPhy> 0)
        {
            return NetworkDeviceType::CUSTOM;
        }
        return NetworkDeviceType::WiFi;
    case 12:
        return NetworkDeviceType::LilyGO_T_ETH_ELite;
    case 13:
        return NetworkDeviceType::Waveshare_ESP32_S3_ETH;
    case 14:
        return NetworkDeviceType::LilyGO_T_ETH_Lite_S3;
    default:
        Log->println("Unknown hardware selected, falling back to Wi-Fi.");
        return NetworkDeviceType::WiFi;
    }
}

std::string NetworkUtil::GetCustomEthernetDeviceName(int custPHY)
{
    switch(custPHY)
    {
    case 4:
        return "Custom (LAN8720)";
    case 5:
        return"Custom (RTL8201)";
    case 6:
        return "Custom (TLK110/IP101)";
    case 7:
        return "Custom (DP83848)";
    case 8:
        return "Custom (KSZ8041)";
    case 9:
        return "Custom (KSZ8081)";
    default:
        return"Custom (LAN8720)";
    }
}

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
eth_phy_type_t NetworkUtil::GetCustomEthernetType(int custPHY)
{
    switch(custPHY)
    {
    case 4:
        return ETH_PHY_TYPE_LAN8720;
        break;
    case 5:
        return ETH_PHY_RTL8201;
        break;
    case 6:
        return ETH_PHY_TLK110;
        break;
    case 7:
        return ETH_PHY_DP83848;
        break;
    case 8:
        return ETH_PHY_KSZ8041;
        break;
    case 9:
        return ETH_PHY_KSZ8081;
        break;
    default:
        return ETH_PHY_TYPE_LAN8720;
        break;
    }
}

eth_clock_mode_t NetworkUtil::GetCustomClock(int custCLKpref)
{
    switch(custCLKpref)
    {
    case 0:
        return ETH_CLOCK_GPIO0_IN;
        break;
    case 1:        
        return ETH_CLOCK_GPIO0_OUT;
        break;
    case 2:
        return ETH_CLOCK_GPIO16_OUT;
        break;
    case 3:
        return ETH_CLOCK_GPIO17_OUT;
        break;
    default:
        return ETH_CLOCK_GPIO17_OUT;
        break;
    }
}
#endif