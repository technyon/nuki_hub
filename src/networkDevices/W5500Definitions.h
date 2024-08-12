#pragma once

#include <hal/spi_types.h>

//static void init(uint8_t sspin = 10, uint8_t sckpin = 255, uint8_t misopin = 255, uint8_t mosipin = 255);
//Ethernet.init(19, 22, 23, 33);


#define ETH_PHY_TYPE_W5500 ETH_PHY_W5500
#define ETH_PHY_ADDR_M5_W5500         1
#define ETH_PHY_CS_M5_W5500           19
#define ETH_PHY_IRQ_M5_W5500          -1
#define ETH_PHY_RST_M5_W5500          -1
#define ETH_PHY_SPI_HOST_M5_W5500    SPI3_HOST
#define ETH_PHY_SPI_SCK_M5_W5500      22
#define ETH_PHY_SPI_MISO_M5_W5500     23
#define ETH_PHY_SPI_MOSI_M5_W5500    33

