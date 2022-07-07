# Ethernet Library

This library is designed to work with the Arduino Ethernet Shield, Arduino Ethernet Shield 2, Leonardo Ethernet, and any other W5100/W5200/W5500-based devices. The library allows an Arduino board to connect to the Internet. The board can serve as either a server accepting incoming connections or a client making outgoing ones. The library supports up to eight (W5100 and boards with <= 2 kB SRAM are limited to four) concurrent connections (incoming, outgoing, or a combination).

The Arduino board communicates with the shield using the SPI bus. This is on digital pins 11, 12, and 13 on the Uno and pins 50, 51, and 52 on the Mega. On both boards, pin 10 is used as SS. On the Mega, the hardware SS pin, 53, is not used to select the Ethernet controller chip, but it must be kept as an output or the SPI interface won't work.

![Arduino UNO Pin map.](./arduino_uno_ethernet_pins.png)

![Arduino MEGA Pin map.](./arduino_mega_ethernet_pins.png)

To use this library

```
#include <SPI.h>
#include <Ethernet.h>
```