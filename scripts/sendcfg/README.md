 
## About

sendcfg.py allows to send the initial device configuration via the serial interface.<br>
This allows automated configuration without prior connecting to the ESPs access point. It could be especially useful if an ESP with LAN hardware is used, and no connection via Wifi is desired.
In this case, sendcfg allows to configure the device without storing Wifi credentials on the device.<br>
This feature is for advanced users only. In certain cases it's necessary to check the C++ code for the desired configuration value, e. g. the network hardware setting.

## Usage

Calling the script itself is simple:

sendcfg.py <port> <file>

Prior to running the script, pyserial has to be installed via pip.
The ESP will only accept a configuration via serial as long as the device in not configured yet, meaning the ESP Wifi access point is open.
Once the ESP is connected to a network, it will ignore any commands send via the serial interface.<br><br>
To generate a configuration a configuration file, the export configuration function can be used.
After exporting a configuration, it can be edited to the required values. Usually only specific values like
the IP address, Wifi credentials or network hardware are intended to be set. In that case all other configuration values can be deleted from the configuration.<br>
For example configurations, see the "example_configurations" subdirectory.

## Common configuration entries

All configuration entries are saved in JSON format.

- wifiSSID: The SSID of the Wifi network
- wifiPass: The pre-shared key of the Wifi network
- dhcpena: Enable ("1") or disable ("0") DHCP
- hostname: Host name of the device
- mqttbroker: Address of the MQTT broker
- mqttpath: MQTT Path used for this device 
- ipaddr: Static IP address 
- ipsub: Subnet mask (e. g. 255.255.255.0)
- ipgtw: Gateway used to connect to the internet
- dnssrv: DNS server used to resolve domain names
- nwhw: Network hardware used. See WebCfgServer.cpp, method getNetworkDetectionOptions for possible values.
At the time of writing: 1=Wifi, 2=Generic W5500, 3 = M5Stack Atom POE (W5500), 4 = "Olimex ESP32-POE / ESP-POE-ISO, 5 = WT32-ETH01, 6 = M5STACK PoESP32 Unit, 7 = LilyGO T-ETH-POE, 8 = GL-S10, 9 = ETH01-Evo, 10 = M5Stack Atom POE S3 (W5500), 11 = Custom LAN module, 12 = LilyGO T-ETH ELite, 13 = Waveshare ESP32-S3-ETH / ESP32-S3-ETH-POE, 14 = LilyGO T-ETH-Lite-ESP32S3, 15 = Waveshare ESP32-P4-NANO, 16 = Waveshare ESP32-P4-Module-DEV-KIT, 17 = ESP32-P4-Function-EV-Board

## Serial commands

The serial interface supports a few commands in addition to receiving the configuration:

- uptime: Prints the uptime in seconds
- printerror: Prints the last error code returned from deserializing the configuration
- printcfgstr: Prints the raw configuration string received via the serial interface
- printcfg: Prints the deserialized configuration that is stored in RAM