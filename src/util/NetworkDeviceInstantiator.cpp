#include "NetworkDeviceInstantiator.h"
#include "../networkDevices/EthernetDevice.h"
#ifndef CONFIG_IDF_TARGET_ESP32H2
#include "../networkDevices/WifiDevice.h"
#endif
#include "../PreferencesKeys.h"
#include "NetworkUtil.h"
#include "../networkDevices/LAN8720Definitions.h"
#include "../networkDevices/Tlk110Definitions.h"

NetworkDevice *NetworkDeviceInstantiator::Create(NetworkDeviceType networkDeviceType, String hostname, Preferences *preferences, IPConfiguration *ipConfiguration)
{
    NetworkDevice* device = nullptr;

    switch (networkDeviceType)
    {
        case NetworkDeviceType::W5500:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "Generic W5500",
                                        ETH_PHY_ADDR_W5500,
                                        ETH_PHY_CS_GENERIC_W5500,
                                        ETH_PHY_IRQ_GENERIC_W5500,
                                        ETH_PHY_RST_GENERIC_W5500,
                                        ETH_PHY_SPI_SCK_GENERIC_W5500,
                                        ETH_PHY_SPI_MISO_GENERIC_W5500,
                                        ETH_PHY_SPI_MOSI_GENERIC_W5500,
                                        ETH_PHY_W5500);
            break;
        case NetworkDeviceType::W5500M5:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "M5Stack Atom POE",
                                        ETH_PHY_ADDR_W5500,
                                        ETH_PHY_CS_M5_W5500,
                                        ETH_PHY_IRQ_M5_W5500,
                                        ETH_PHY_RST_M5_W5500,
                                        ETH_PHY_SPI_SCK_M5_W5500,
                                        ETH_PHY_SPI_MISO_M5_W5500,
                                        ETH_PHY_SPI_MOSI_M5_W5500,
                                        ETH_PHY_W5500);
            break;
        case NetworkDeviceType::W5500M5S3:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "M5Stack Atom POE S3",
                                        ETH_PHY_ADDR_W5500,
                                        ETH_PHY_CS_M5_W5500_S3,
                                        ETH_PHY_IRQ_M5_W5500,
                                        ETH_PHY_RST_M5_W5500,
                                        ETH_PHY_SPI_SCK_M5_W5500_S3,
                                        ETH_PHY_SPI_MISO_M5_W5500_S3,
                                        ETH_PHY_SPI_MOSI_M5_W5500_S3,
                                        ETH_PHY_W5500);
            break;
        case NetworkDeviceType::Waveshare_ESP32_S3_ETH:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "Waveshare ESP32-S3-ETH / ESP32-S3-ETH-POE",
                                        ETH_ADDR_WAVESHARE_ESP32_S3_ETH,
                                        ETH_PHY_SPI_CS_WAVESHARE_ESP32_S3_ETH,
                                        ETH_PHY_SPI_IRQ_WAVESHARE_ESP32_S3_ETH,
                                        ETH_PHY_SPI_RST_WAVESHARE_ESP32_S3_ETH,
                                        ETH_PHY_SPI_SCK_WAVESHARE_ESP32_S3_ETH,
                                        ETH_PHY_SPI_MISO_WAVESHARE_ESP32_S3_ETH,
                                        ETH_PHY_SPI_MOSI_WAVESHARE_ESP32_S3_ETH,
                                        ETH_PHY_W5500);
            break;
        case NetworkDeviceType::ETH01_Evo:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "ETH01-Evo",
                                        ETH_PHY_ADDR_ETH01EVO,
                                        ETH_PHY_CS_ETH01EVO,
                                        ETH_PHY_IRQ_ETH01EVO,
                                        ETH_PHY_RST_ETH01EVO,
                                        ETH_PHY_SPI_SCK_ETH01EVO,
                                        ETH_PHY_SPI_MISO_ETH01EVO,
                                        ETH_PHY_SPI_MOSI_ETH01EVO,
                                        ETH_PHY_TYPE_DM9051);
            break;
        case NetworkDeviceType::LilyGO_T_ETH_ELite:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "LilyGO T-ETH ELite",
                                        ETH_PHY_ADDR_W5500,
                                        ETH_PHY_CS_ELITE_W5500,
                                        ETH_PHY_IRQ_ELITE_W5500,
                                        ETH_PHY_RST_ELITE_W5500,
                                        ETH_PHY_SPI_SCK_ELITE_W5500,
                                        ETH_PHY_SPI_MISO_ELITE_W5500,
                                        ETH_PHY_SPI_MOSI_ELITE_W5500,
                                        ETH_PHY_W5500);
            break;
        case NetworkDeviceType::LilyGO_T_ETH_Lite_S3:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "LilyGO T-ETH-Lite-ESP32S3",
                                        ETH_PHY_ADDR_W5500,
                                        ETH_PHY_CS_ETHLITES3_W5500,
                                        ETH_PHY_IRQ_ETHLITES3_W5500,
                                        ETH_PHY_RST_ETHLITES3_W5500,
                                        ETH_PHY_SPI_SCK_ETHLITES3_W5500,
                                        ETH_PHY_SPI_MISO_ETHLITES3_W5500,
                                        ETH_PHY_SPI_MOSI_ETHLITES3_W5500,
                                        ETH_PHY_W5500);
            break;

        case NetworkDeviceType::CUSTOM:
        {
            int custPHY = preferences->getInt(preference_network_custom_phy, 0);

            if(custPHY >= 1 && custPHY <= 3)
            {
                std::string custName;
                eth_phy_type_t custEthtype;

                switch(custPHY)
                {
                    case 1:
                        custName = "Custom (W5500)";
                        custEthtype = ETH_PHY_W5500;
                        break;
                    case 2:
                        custName = "Custom (DN9051)";
                        custEthtype = ETH_PHY_DM9051;
                        break;
                    case 3:
                        custName = "Custom (KSZ8851SNL)";
                        custEthtype = ETH_PHY_KSZ8851;
                        break;
                    default:
                        custName = "Custom (W5500)";
                        custEthtype = ETH_PHY_W5500;
                        break;
                }

                device = new EthernetDevice(hostname, preferences, ipConfiguration, custName,
                                            preferences->getInt(preference_network_custom_addr, -1),
                                            preferences->getInt(preference_network_custom_cs, -1),
                                            preferences->getInt(preference_network_custom_irq, -1),
                                            preferences->getInt(preference_network_custom_rst, -1),
                                            preferences->getInt(preference_network_custom_sck, -1),
                                            preferences->getInt(preference_network_custom_miso, -1),
                                            preferences->getInt(preference_network_custom_mosi, -1),
                                            custEthtype);
            }
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
            else if(custPHY >= 4 && custPHY <= 9)
            {
                int custCLKpref = preferences->getInt(preference_network_custom_clk, 0);

                std::string custName = NetworkUtil::GetCustomEthernetDeviceName(custPHY);
                eth_phy_type_t custEthtype = NetworkUtil::GetCustomEthernetType(custPHY);
                eth_clock_mode_t custCLK = NetworkUtil::GetCustomClock(custCLKpref);

                device = new EthernetDevice(hostname, preferences, ipConfiguration, custName,
                                            preferences->getInt(preference_network_custom_addr, -1),
                                            preferences->getInt(preference_network_custom_pwr, -1),
                                            preferences->getInt(preference_network_custom_mdc, -1),
                                            preferences->getInt(preference_network_custom_mdio, -1),
                                            custEthtype,
                                            custCLK);
            }
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
            else
            {
                device = new WifiDevice(hostname, preferences, ipConfiguration);
            }
#endif
        }
            break;
#if defined(CONFIG_IDF_TARGET_ESP32)
        case NetworkDeviceType::M5STACK_PoESP32_Unit:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "M5STACK PoESP32 Unit",
                                        ETH_PHY_ADDR_M5_POESP32,
                                        ETH_PHY_POWER_M5_POESP32,
                                        ETH_PHY_MDC_M5_POESP32,
                                        ETH_PHY_MDIO_M5_POESP32,
                                        ETH_CLK_MODE_M5_TYPE,
                                        ETH_CLK_MODE_M5_POESP32);
            break;
        case NetworkDeviceType::Olimex_LAN8720:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "Olimex (LAN8720)", ETH_PHY_ADDR_LAN8720, 12, ETH_PHY_MDC_LAN8720, ETH_PHY_MDIO_LAN8720, ETH_PHY_TYPE_LAN8720, ETH_CLOCK_GPIO17_OUT);
            break;
        case NetworkDeviceType::WT32_LAN8720:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "WT32-ETH01", 1, 16);
            break;
        case NetworkDeviceType::GL_S10:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "GL-S10", 1, 5, ETH_PHY_MDC_LAN8720, ETH_PHY_MDIO_LAN8720, ETH_PHY_IP101, ETH_CLOCK_GPIO0_IN);
            break;
        case NetworkDeviceType::LilyGO_T_ETH_POE:
            device = new EthernetDevice(hostname, preferences, ipConfiguration, "LilyGO T-ETH-POE", 0, -1, ETH_PHY_MDC_LAN8720, ETH_PHY_MDIO_LAN8720, ETH_PHY_TYPE_LAN8720, ETH_CLOCK_GPIO17_OUT);
            break;
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
        case NetworkDeviceType::WiFi:
            device = new WifiDevice(hostname, preferences, ipConfiguration);
            break;
        default:
            device = new WifiDevice(hostname, preferences, ipConfiguration);
            break;
#else
            default:
        device = new EthernetDevice(hostname, preferences, ipConfiguration, "Custom (W5500)",
                                    preferences->getInt(preference_network_custom_addr, -1),
                                    preferences->getInt(preference_network_custom_cs, -1),
                                    preferences->getInt(preference_network_custom_irq, -1),
                                    preferences->getInt(preference_network_custom_rst, -1),
                                    preferences->getInt(preference_network_custom_sck, -1),
                                    preferences->getInt(preference_network_custom_miso, -1),
                                    preferences->getInt(preference_network_custom_mosi, -1),
                                    ETH_PHY_W5500);
        break;
#endif
    }

    return device;
}
