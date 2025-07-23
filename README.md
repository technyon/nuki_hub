## About

***The scope of Nuki Hub is to have an efficient way to integrate Nuki devices in a local Home Automation platform.***

The Nuki Hub software runs on a ESP32 module and acts as a bridge between Nuki devices and a Home Automation platform.<br>
<br>
It communicates with a Nuki Lock and/or Opener through Bluetooth (BLE) and uses MQTT to integrate with other systems.<br>
<br>    
It exposes the lock state (and much more) through MQTT and allows executing commands like locking and unlocking as well as changing the Nuki Lock/Opener configuration through MQTT.<br>

***Nuki Hub does not integrate with the Nuki mobile app, it can't register itself as a bridge in the official Nuki mobile app.***

Feel free to join us on Discord: https://discord.gg/9nPq85bP4p

## Supported devices

<b>Supported ESP32 devices:</b>
- Nuki Hub is compiled against all ESP32 models with Wi-Fi and Bluetooh Low Energy (BLE) which are supported by ESP-IDF 5.5.0 and Arduino Core 3.3.0.
- Tested stable builds are provided for the ESP32, ESP32-S3, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-P4 (with the ESP32-C6-MINI-1 module for BLE and WiFi) and ESP32-H2.
- Untested builds are provided for the ESP32-Solo1 (as the developers don't own one).

<b>Not supported ESP32 devices:</b>
- The ESP32-S2 has no built-in BLE and as such can't run Nuki Hub.

<b>Supported Nuki devices:</b>
- Nuki Smart Lock 1.0
- Nuki Smart Lock 2.0
- Nuki Smart Lock 3.0
- Nuki Smart Lock 3.0 Pro
- Nuki Smart Lock 4.0
- Nuki Smart Lock 4.0 Pro
- Nuki Smart Lock Go
- Nuki Smart Lock 5.0 Pro
- Nuki Smart Lock Ultra
- Nuki Opener
- Nuki Keypad 1.0
- Nuki Keypad 2.0

<b>Supported Ethernet devices:</b><br>
As an alternative to Wi-Fi (which is available on any supported ESP32), the following ESP32 modules with built-in wired ethernet are supported:
- [Olimex ESP32-POE](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE/open-source-hardware)
- [Olimex ESP32-POE-ISO](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware)
- [WT32-ETH01](http://en.wireless-tag.com/product-item-2.html)
- [M5Stack Atom POE](https://docs.m5stack.com/en/atom/atom_poe)
- [M5Stack PoESP32 Unit](https://docs.m5stack.com/en/unit/poesp32)
- [LilyGO-T-ETH-POE](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-POE)
- [LilyGO-T-ETH-Lite](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series)
- [LilyGO-T-ETH-Lite-ESP32S3](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series)
- [LilyGO-T-ETH ELite](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series)
- [GL-S10 (Revisions 2.1, 2.3 / 1.0 might not be supported)](https://www.gl-inet.com/products/gl-s10/)
- [Waveshare ESP32-S3-ETH / ESP32-S3-POE-ETH](https://www.waveshare.com/esp32-s3-eth.htm?sku=28771)
- [Waveshare ESP32-P4-NANO](https://www.waveshare.com/esp32-p4-nano.htm)
- [Waveshare ESP32-P4-Module-DEV-KIT](https://www.waveshare.com/esp32-p4-module-dev-kit.htm)
- [Espressif ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html)
  
In principle all ESP32 (and variants) devices with built-in ethernet port are supported, but might require additional setup using the "Custom LAN setup" option.
See the "[Connecting via Ethernet](#connecting-via-ethernet-optional)" section for more information.

## Recommended ESP32 devices

We don't recommend using the original ESP32 or ESP32-Solo1 devices because these devices experience unexpected crashes related to the (closed-source) BLE controller.<br>
In newer models (e.g. ESP32-S3, ESP32-P4, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-H2) these unexpected crashed are seen a lot less.

We also don't recommend using the original ESP32 or ESP32-Solo1 devices because these devices experience unexpected crashes related to the (closed-source) BLE controller.<br>
In newer models (e.g. ESP32-S3, ESP32-P4, ESP32-C3, ESP32-C6, ESP32-H2) these unexpected crashes are seen less.

When buying a new device in 2025 we can only recommend the ESP32-P4 or ESP32-S3 with PSRAM (look for an ESP32-S3 with the designation N>=4 and R>=2 such as an ESP32-S3 N16R8).<br>

The ESP32-P4 with ESP32-C6-MINI-1 module for BLE/WiFi is the most powerfull ESP32 in 2025. 
It supports (with the C6 co-processor) anything the ESP32 range has to offer with the highest CPU clocks, largest flash and PSRAM, Ethernet, PoE and WiFi6.
The only function missing (when not using a C5 as co-processor) is 5Ghz WiFi support.

The ESP32-S3 is a dual-core CPU with many GPIO's, ability to enlarge RAM using PSRAM, ability to connect Ethernet modules over SPI and optionally power the device with a PoE splitter.<br>
The only functions missing from the ESP32-S3 as compared to other ESP devices is the ability to use some Ethernet modules only supported by the original ESP32 (and ESP32-P4) and the ability to connect over WIFI6 (C5, C6 or ESP32-P4 with C6 module)

The ESP32-C5 with PSRAM is a good option providing higher clockspeeds than the C6 and adding PSRAM and WIFI 6 on the 5 Ghz band support.
Nuki Hub uses both CPU cores (if available) to process tasks (e.g. HTTP server/MQTT client/BLE scanner/BLE client) and thus runs better on dual-core devices.<br>

Other considerations:
- If Ethernet/PoE is required: ESP32-P4 with ESP32-C6-MINI-1 module or ESP32-S3 with PSRAM in combination with a SPI Ethernet module ([W5500](https://www.aliexpress.com/w/wholesale-w5500.html)) and [PoE to Ethernet and USB type B/C splitter](https://aliexpress.com/w/wholesale-poe-splitter-usb-c.html) or the LilyGO-T-ETH ELite, LilyGO-T-ETH-Lite-ESP32S3 or M5stack Atom S3R with the M5stack AtomPoe W5500 module
- If WIFI6 is required: ESP32-P4 with ESP32-C6-MINI-1 module, ESP32-C5 or ESP32-C6

Devices ranked best-to-worst:
- ESP32-P4 with ESP32-C6-MINI-1 module
- ...... <br>
- ESP32-S3 with PSRAM
- ESP32-C5 with PSRAM
- ...... <br>
(Devices below will not support some Nuki Hub functions, be slower and/or are more likely to experience unexpected crashes)
- ESP32-S3 without PSRAM
- ......
- ESP32 with PSRAM
- ......
- ESP32 without PSRAM
- ...... <br>
(Devices below will not support more Nuki Hub functions, be slower and/or are more likely to experience unexpected crashes)
- ESP32-C5
- ESP32-C6
- ESP32-solo1
- ESP32-C3
- ESP32-H2

## Feature comparison

### Feature comparison Nuki Hub vs. Nuki Bridge

| Feature | Nuki Hub | Nuki Bridge |
|---|---|---|
| Smart Lock/Opener (remote) control | :white_check_mark: | :white_check_mark: |
| MQTT | :white_check_mark: | :no_entry: |
| Wired LAN support, optionally with Power over Ethernet (PoE) | :white_check_mark: | :no_entry: |
| Smart Home (e.g. Home Assistant) integration | :white_check_mark: | :white_check_mark: (limited) |
| Cloud support | :white_check_mark: (optional through smarthome solution) | :white_check_mark: |
| Cloud-less operation | :white_check_mark: | :white_check_mark: |
| Nuki Keypad (1.0 and 2.0) support | :white_check_mark: | :white_check_mark: |
| Control via GPIO | :white_check_mark: | :no_entry: |
| Hybrid mode for WiFI and Thread connected locks | :white_check_mark: | :no_entry: |

### Feature comparison Nuki Hub vs. Nuki official MQTT API over WiFi/Thread (Smart Lock >=3.0 Pro)

| Feature | Nuki Hub | Nuki official MQTT API |
|---|---|---|
| Smart Lock (remote) control | :white_check_mark: | :white_check_mark: |
| Opener support | :white_check_mark: | :no_entry: |
| Optional MQTT SSL support | :white_check_mark: | :no_entry: |
| Wired LAN support, optionally with Power over Ethernet (PoE) | :white_check_mark: | :no_entry: |
| Smart Home (e.g. Home Assistant) integration | :white_check_mark: | :white_check_mark: (limited) |
| Cloud support | :white_check_mark: (optional through smarthome solution) | :white_check_mark: (optional through smarthome solution) |
| Cloud-less operation | :white_check_mark: | :white_check_mark: (limited) |
| Nuki Keypad (1.0 and 2.0) support | :white_check_mark: | :white_check_mark: (limited) |
| Control via GPIO | :white_check_mark: | :no_entry: |
| Hybrid mode combining Bluetooth and WiFi/Thread | :white_check_mark: | :no_entry: |
| Feature parity with the Nuki app (e.g. changing device settings, keypad codes, authorizations etc.) | :white_check_mark: | :no_entry: |

## Support Nuki Hub development
This project is free to use for everyone. However if you feel like donating, you can buy me a coffee at ko-fi.com:<br>
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/C0C1IDUBV)
<br><br>
If you plan to buy NUKI hardware, you can use my referral code to earn a 10% discount. It allows the project to team to buy further NUKI hardware for testing:<br>
> REF-3TnaGNQJdc

## First time installation

Flash the firmware to an ESP32. The easiest way to install is to use the web installer using a compatible browser like Chrome/Opera/Edge:<br>
https://technyon.github.io/nuki_hub/<br>
<br>
Alternatively download the latest release for your ESP32 model from https://github.com/technyon/nuki_hub/releases<br>
Unpack the zip archive and read the included how-to-flash.txt for installation instructions for either "Espressif Flash Download Tools" or "esptool".<br>

## Initial setup (Network and MQTT)

Power up the ESP32 and a new Wi-Fi access point named "NukiHub" should appear.<br>
The password of the access point is "NukiHubESP32".<br>
Connect a client device to this access point and in a browser navigate to "http://192.168.4.1".<br>
Use the web interface to connect the ESP to your preferred Wi-Fi network.<br>
<br>
After configuring Wi-Fi, the ESP should automatically connect to your network.<br>
<br>
To configure the connection to the MQTT broker, first connect your client device to the same Wi-Fi network the ESP32 is connected to.<br>
In a browser navigate to the IP address assigned to the ESP32 via DHCP (often found in the web interface of your internet router) or static IP.<br><br>
Next click on "MQTT Configuration" and enter the address and port (usually 1883) of your MQTT broker and a username and password if required by your MQTT broker.<br>
<br>
The firmware supports SSL encryption for MQTT.<br>
See the "[MQTT Encryption](#mqtt-encryption-optional)" section of this README.

## Pairing with a Nuki Lock (1.0-4.0) or Opener

Make sure "Bluetooth pairing" is enabled for the Nuki device by enabling this setting in the official Nuki App in "Settings" > "Features & Configuration" > "Button and LED".
After enabling the setting press the button on the Nuki device for a few seconds.<br>
Pairing should be automatic whenever the ESP32 is powered on and no lock is paired.<br>
To pair with an opener you need to first enable the opener on the "Nuki configuration" page of Nuki Hub.<br>
<br>
When pairing is successful, the web interface should show "Paired: Yes".<br>
MQTT nodes like lock state and battery level should now reflect the reported values from the lock.<br>
<br>
<b>Note: It is possible to run Nuki Hub alongside a Nuki Bridge.
This is not recommended (unless when using [hybrid mode](/HYBRID.md)) and will lead to excessive battery drain and can lead to either device missing updates.
Enable "Register as app" before pairing to allow this. Otherwise the Bridge will be unregistered when pairing the Nuki Hub.</b>

## Pairing with a Nuki Lock Ultra / Nuki Lock Go / Nuki Lock 5th gen Pro

Make sure "Bluetooth pairing" is enabled for the Nuki device by enabling this setting in the official Nuki App in "Settings" > "Features & Configuration" > "Button and LED".

Before enabling pairing mode using the button on the Nuki Lock Ultra / Nuki Lock Go / Nuki Lock 5th gen Pro first setup Nuki Hub as follows:
- Enable both "Nuki Smartlock enabled" and "Nuki Smartlock Ultra/Go/5th gen enabled" settings on the "Basic Nuki Configuration" page and Save. Setting the "Nuki Smartlock Ultra/Go/5th gen enabled" will change multiple other NukiHub settings.
- Input your 6-digit Nuki Lock Ultra/Go/5th gen PIN on the "Credentials" page and Save
- Press the button on the Nuki device for a few seconds until the LED ring lights up and remains lit.
- It is **strongly** recommended(/mandatory) to setup and enable Hybrid mode over Thread/WiFi + official MQTT as Nuki Hub works best in Hybrid or Bridge mode and the Ultra/Go/5th gen Pro does not support Bridge mode

When pairing is successful, the web interface should show "Paired: Yes".<br>

## Hybrid mode

Hybrid mode allows you to use the official Nuki MQTT implemenation on a Nuki Lock 3.0 Pro, Nuki Lock 4.0, Nuki Lock 4.0 Pro, Nuki Lock 5.0 Pro, Nuki Lock Go or Nuki Lock Ultra in conjunction with Nuki Hub.<br>
See [hybrid mode](/HYBRID.md) for more information.

## Memory constraints

ESP32 devices have a limited amount of free RAM available.<br>
<br>
On version >=9.10 of Nuki Hub with only a Nuki Lock connected the expected free amount of RAM/Heap available is around:

- ESP32: 105.000 bytes
- ESP32 with PSRAM: 120.000 bytes + PSRAM
- ESP32-C3: 70.000 bytes
- ESP32-C5 with PSRAM: 130.000 bytes + PSRAM
- ESP32-C6: 200.000 bytes
- ESP32-P4: 450.000 bytes
- ESP32-S3 135.000 bytes
- ESP32-S3 with PSRAM: 185.000 bytes + PSRAM

This free amount of RAM can be reduced (temporarily) by certain actions (such as changing Nuki device config) or continuously when enabling the following:
- Connecting both a Nuki opener and a Nuki lock to Nuki Hub
- Enlarging stack sizes of the Nuki and Network task to accommodate large amounts of keypad codes, authorization entries or timecontrol entries
- MQTT SSL (Costs about 20k-30k RAM)
- HTTP SSL (Costs about 30k RAM)
- Developing/debugging Nuki devices and/or Nuki Hub, using WebSerial (Costs about 30k RAM)

The currently available RAM/Heap can be found on the info page of the Web configurator of Nuki Hub.<br>
<br>
When the ESP32 runs out of available RAM the device can crash or otherwise unexpected behaviour can occur.<br>
<br>
Nuki Hub does allow for the use of embedded PSRAM on the regular binaries whenever it is available.<br>
PSRAM is usually 2, 4 or 8MB in size and thus greatly enlarges the 320kb of internal RAM that is available.<br>
It is basically impossible to run out of RAM when PSRAM is available.
You can check on the info page of the Web configurator if PSRAM is available.

Note that there are two builds of Nuki Hub for the ESP32-S3 available.<br>
One for devices with no or Quad SPI PSRAM and one for devices with Octal SPI PSRAM.<br>
If your ESP32-S3 device has PSRAM but it is not detected please switch to the other S3 binary.<br>
You can do this by flashing the correct binaries manually or by selecting the option to switch S3 binary build from the Firmware Update page of the Web Configurator.

Note that there are also is a separate build of Nuki Hub available for the GL-S10 ESP32 which is needed to enable PSRAM on this device.<br>

## Configuration

In a browser navigate to the IP address assigned to the ESP32.

### Network Configuration

#### Network Configuration

- Hostname (needs to be unique): Set the hostname for the Nuki Hub ESP, will also be used as the MQTT client ID. Needs to be unique.
- Network hardware: "Wi-Fi only" by default, set to one of the specified ethernet modules if available, see the "Supported Ethernet devices" and "[Connecting via Ethernet](#connecting-via-ethernet-optional)" section of this README.
- Home Assistant device configuration URL: When using Home Assistant discovery the link to the Nuki Hub Web Configuration will be published to Home Assistant. By default when this setting is left empty this will link to the current IP of the Nuki Hub. When using a reverse proxy to access the Web Configuration you can set a custom URL here.
- RSSI Publish interval: Set to a positive integer to set the amount of seconds between updates to the maintenance/wifiRssi MQTT topic with the current Wi-Fi RSSI, set to -1 to disable, default 60.
- Restart on disconnect: Enable to restart the Nuki Hub when disconnected from the network.
- Check for Firmware Updates every 24h: Enable to allow the Nuki Hub to check the latest release of the Nuki Hub firmware on boot and every 24 hours. Requires the Nuki Hub to be able to connect to github.com. The latest version will be published to MQTT and will be visible on the main page of the Web Configurator.
- Set HTTP SSL Certificate (PSRAM enabled devices only): Optionally set to the SSL certificate of the HTTPS server, see the "[HTTPS Server](#https-server-optional-psram-enabled-devices-only)" section of this README.
- Set HTTP SSL Key (PSRAM enabled devices only): Optionally set to the SSL key of the HTTPS server, see the "[HTTPS Server](#https-server-optional-psram-enabled-devices-only)" section of this README.
- Generate self-signed HTTP SSL Certificate and key: Optionally generate a self-signed SSL certificate and key for the HTTPS server, see the "[HTTPS Server](#https-server-optional-psram-enabled-devices-only)" section of this README.
- Nuki Hub FQDN for HTTP redirect (PSRAM enabled devices only): FQDN hostname of the Nuki Hub device used for redirecting from HTTP to HTTPS.

#### IP Address assignment

- Enable DHCP: Enable to use DHCP for obtaining an IP address, disable to use the static IP settings below
- Static IP address: When DHCP is disabled set to the preferred static IP address for the Nuki Hub to use
- Subnet: When DHCP is disabled set to the preferred subnet for the Nuki Hub to use
- Default gateway: When DHCP is disabled set to the preferred gateway IP address for the Nuki Hub to use
- DNS Server: When DHCP is disabled set to the preferred DNS server IP address for the Nuki Hub to use

### MQTT Configuration

#### Basic MQTT Configuration

- MQTT Broker: Set to the IP address of the MQTT broker
- MQTT Broker port: Set to the Port of the MQTT broker (usually 1883)
- MQTT User: If using authentication on the MQTT broker set to a username with read/write rights on the MQTT broker, set to # to clear
- MQTT Password : If using authentication on the MQTT broker set to the password belonging to a username with read/write rights on the MQTT broker, set to # to clear
- MQTT Nuki Hub Path: Set to the preferred MQTT root topic for Nuki Hub, defaults to "nukihub". Make sure this topic is unique when using multiple ESP32 Nuki Hub devices
- Enable Home Assistant auto discovery: Enable Home Assistant MQTT auto discovery. Will automatically create entities in Home Assistant for Nuki Hub and connected Nuki Lock and/or Opener when enabled.

#### Advanced MQTT Configuration

- Home Assistant discovery topic: Set to the Home Assistant auto discovery topic. Usually "homeassistant" unless you manually changed this setting on the Home Assistant side.
- Set Nuki Opener Lock/Unlock action in Home Assistant to Continuous mode (Opener only): By default the lock entity in Home Assistant will enable Ring-to-Open (RTO) when unlocking and disable RTO when locking. By enabling this setting this behaviour will change and now unlocking will enable Continuous Mode and locking will disable Continuous Mode, for more information see the "[Home Assistant Discovery](#home-assistant-discovery-optional)" section of this README.
- MQTT SSL CA Certificate: Optionally set to the CA SSL certificate of the MQTT broker, see the "[MQTT Encryption](#mqtt-encryption-optional)" section of this README.
- MQTT SSL Client Certificate: Optionally set to the Client SSL certificate of the MQTT broker, see the "[MQTT Encryption](#mqtt-encryption-optional)" section of this README.
- MQTT SSL Client Key: Optionally set to the Client SSL key of the MQTT broker, see the "[MQTT Encryption](#mqtt-encryption-optional)" section of this README.
- MQTT Timeout until restart: Set to a positive integer to restart the Nuki Hub after the set amount of seconds has passed without an active connection to the MQTT broker, set to -1 to disable, default 60.
- Enable MQTT logging: Enable to fill the maintenance/log MQTT topic with debug log information.
- Allow updating using MQTT: Enable to allow starting the Nuki Hub update process using MQTT. Will also enable the Home Assistant update functionality if auto discovery is enabled.
- Disable some extraneous non-JSON topics: Enable to not publish non-JSON keypad and config MQTT topics.
- Enable hybrid official MQTT and Nuki Hub setup: Enable to combine the official MQTT over Thread/Wi-Fi with BLE. Improves speed of state changes. Needs the official MQTT to be setup first. Also requires Nuki Hub to be paired as app and unregistered as a bridge using the Nuki app. See [hybrid mode](/HYBRID.md)
- Enable sending actions through official MQTT: Enable to sent lock actions through the official MQTT topics (e.g. over Thread/Wi-Fi) instead of using BLE. Needs "Enable hybrid official MQTT and Nuki Hub setup" to be enabled. See [hybrid mode](/HYBRID.md)
- Time between status updates when official MQTT is offline (seconds): Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki lock for the current lock state when the official MQTT is offline, default 600.
- Retry command sent using official MQTT over BLE if failed: Enable to publish lock commands over BLE if Nuki Hub fails to use Hybrid mode to execute the command over MQTT
- Reboot Nuki lock on official MQTT failure: Enable to reboot the Nuki Lock if official MQTT over WiFi/Thread is offline for more than 180 seconds

### Nuki Configuration

#### Basic Nuki Configuration

- Nuki Smartlock enabled: Enable if you want Nuki Hub to connect to a Nuki Lock (1.0-4.0 and Ultra)
- Nuki Smartlock Ultra/Go/5th gen enabled: Enable if you want Nuki Hub to connect to a Nuki Lock Ultra/Go/5th gen Pro
- Nuki Opener enabled: Enable if you want Nuki Hub to connect to a Nuki Opener

#### Advanced Nuki Configuration

- Query interval lock state: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current lock state, default 1800.
- Query interval configuration: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current configuration, default 3600.
- Query interval battery: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current battery state, default 1800.
- Query interval keypad (Only available when a Keypad is detected): Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current keypad state, default 1800.
- Number of retries if command failed: Set to a positive integer to define the amount of times the Nuki Hub retries sending commands to the Nuki Lock or Opener when commands are not acknowledged by the device, default 3.
- Delay between retries: Set to the amount of milliseconds the Nuki Hub waits between resending not acknowledged commands, default 100.
- Lock: Nuki Bridge is running alongside Nuki Hub: Enable to allow Nuki Hub to co-exist with a Nuki Bridge by registering Nuki Hub as an (smartphone) app instead of a bridge. Changing this setting will require re-pairing. Enabling this setting is strongly discouraged as described in the "[Pairing with a Nuki Lock or Opener](#pairing-with-a-nuki-lock-or-opener)" section of this README, ***unless when used in [Hybrid mode](/HYBRID.md) (Official MQTT / Nuki Hub co-existance)***
- Opener: Nuki Bridge is running alongside Nuki Hub: Enable to allow Nuki Hub to co-exist with a Nuki Bridge by registering Nuki Hub as an (smartphone) app instead of a bridge. Changing this setting will require re-pairing. Enabling this setting is strongly discouraged as described in the "[Pairing with a Nuki Lock or Opener](#pairing-with-a-nuki-lock-or-opener)" section of this README
- Restart if bluetooth beacons not received: Set to a positive integer to restart the Nuki Hub after the set amount of seconds has passed without receiving a bluetooth beacon from the Nuki device, set to -1 to disable, default 60. Because the bluetooth stack of the ESP32 can silently fail it is not recommended to disable this setting.
- BLE transmit power in dB: Set to a integer between -12 and 9 (ESP32) or -12 and 20 (All newer ESP32 variants) to set the Bluetooth transmit power, default 9.
- Update Nuki Hub and Lock/Opener time using NTP: Enable to update the ESP32 time and Nuki Lock and/or Nuki Opener time every 12 hours using a NTP time server. Updating the Nuki device time requires the Nuki security code / PIN to be set, see "[Nuki Lock PIN / Nuki Opener PIN](#nuki-lock-pin--nuki-opener-pin)" below.
- NTP server: Set to the NTP server you want to use, defaults to "pool.ntp.org". If DHCP is used and NTP servers are provided using DHCP these will take precedence over the specified NTP server.

### Access Level Configuration

#### Nuki General Access Control
- Publish Nuki Hub configuration information: Publish Nuki Hub settings over MQTT, see "[Import and Export Nuki Hub settings over MQTT](#import-and-export-nuki-hub-settings-over-mqtt)"
- Modify Nuki Hub configuration over MQTT: Allow changing Nuki Hub settings using MQTT, see "[Import and Export Nuki Hub settings over MQTT](#import-and-export-nuki-hub-settings-over-mqtt)"
- Publish Nuki configuration information: Enable to publish information about the configuration of the connected Nuki device(s) through MQTT.

Note: All of the following requires the Nuki security code / PIN to be set, see "[Nuki Lock PIN / Nuki Opener PIN](#nuki-lock-pin--nuki-opener-pin)"

- Publish keypad entries information (Only available when a Keypad is detected): Enable to publish information about keypad codes through MQTT, see the "[Keypad control](#keypad-control-optional)" section of this README
- Also publish keypad codes (Only available when a Keypad is detected): Enable to publish the actual keypad codes through MQTT, note that is could be considered a security risk
- Add, modify and delete keypad codes (Only available when a Keypad is detected): Enable to allow configuration of keypad codes through MQTT, see the "[Keypad control](#keypad-control-optional)" section of this README
- Allow checking if keypad codes are valid (Only available when a Keypad is detected): Enable to allow checking if a given codeId and code combination is valid through MQTT, note that is could be considered a security risk
- Publish timecontrol information: Enable to publish information about timecontrol entries through MQTT, see the "[Timecontrol](#timecontrol)" section of this README
- Add, modify and delete timecontrol entries: Enable to allow configuration of timecontrol entries through MQTT, see the "[Timecontrol](#timecontrol)" section of this README
- Publish authorization information: Enable to publish information about authorization entries through MQTT, see the "[Authorization](#authorization)" section of this README
- Modify and delete authorization entries: Enable to allow configuration of authorization entries through MQTT, see the "[Authorization](#authorization)" section of this README
- Publish auth data: Enable to publish authorization data to the MQTT topic lock/log

#### Nuki Lock/Opener Access Control
- Enable or disable executing each available lock action for the Nuki Lock and Nuki Opener through MQTT. Note: GPIO control is not restricted through this setting.

#### Nuki Lock/Opener Config Control
- Enable or disable changing each available configuration setting for the Nuki Lock and Nuki Opener through MQTT.
- NOTE: Changing configuration settings requires the Nuki security code / PIN to be set, see "[Nuki Lock PIN / Nuki Opener PIN](#nuki-lock-pin--nuki-opener-pin)" below.

### Credentials

#### Credentials

- User: Pick a username to enable HTTP authentication for the Web Configuration, Set to "#" to disable authentication.
- Password/Retype password: Pick a password to enable HTTP authentication for the Web Configuration.
- HTTP Authentication type: Select from Basic, Digest or Form based authentication. Digest authentication is more secure than Basic or Form based authentication, especially over unencrypted (HTTP) connections. Form based authentication works best with password managers. Note: Firefox seems to have issues with basic authentication.
- Bypass authentication for reverse proxy with IP: IP for which authentication is bypassed. Use in conjunction with a reverse proxy server with separate authentication.
- Duo Push authentication enabled: Enable to use Duo Push Multi Factor Authentication (MFA). See [Duo Push authentication](/DUOAUTH.md) for instructions on how to setup Duo Push authentication.
- Require MFA (Duo/TOTP) authentication for all sensitive Nuki Hub operations (changing/exporting settings): Enable to require MFA approval on all sensitive operations.
- Bypass MFA (Duo/TOTP) authentication by pressing the BOOT button during login: Enable to allow bypassing MFA authentication by pressing the BOOT button on the ESP during login. Note that this does not work on all ESP's (nor do all ESP's have a boot button to begin with). Test before relying on this function.
- Bypass MFA (Duo/TOTP) authentication by pulling GPIO High: Set to a GPIO pin to allow bypassing MFA authentication by pulling the GPIO high during login.
- Bypass MFA (Duo/TOTP) authentication by pulling GPIO Low: Set to a GPIO pin to allow bypassing MFA authentication by pulling the GPIO low during login.
- Duo API hostname: Set to the Duo API hostname
- Duo integration key: Set to the Duo integration key
- Duo secret key: Set to the Duo secret key
- Duo user: Set to the Duo user that you want to receive the push notification
- TOTP Secret Key: Set a TOTP secret key to enable TOTP MFA. Enter the TOTP secret key in an authenticator application (Password manager, Microsoft/Google Authenticator etc.) to generate TOTP codes.
- One-time MFA Bypass: Set a 32 character long alphanumeric string that can be used as a one-time MFA bypass when the ESP32 is unable to sync it's time and TOTP and Duo are unavailable as a result.
- Admin key: Set a 32 character long alphanumeric string that can be used in combination with a TOTP code to export and import settings without needing to log in (for use with automated systems).
- Session validity (in seconds): Session validity to use with form authentication when the "Remember me" checkbox is disabled, default 3600 seconds.
- Session validity remember (in hours): Session validity to use with form authentication when the "Remember me" checkbox is enabled, default 720 hours.
- Duo Session validity (in seconds): Session validity to use with Duo authentication when the "Remember me" checkbox is disabled, default 3600 seconds.
- Duo Session validity remember (in hours): Session validity to use with Duo authentication when the "Remember me" checkbox is enabled, default 720 hours.
- TOTP Session validity (in seconds): Session validity to use with TOTP authentication when the "Remember me" checkbox is disabled, default 3600 seconds.
- TOTP Session validity remember (in hours): Session validity to use with TOTP authentication when the "Remember me" checkbox is enabled, default 720 hours.

#### Nuki Lock PIN / Nuki Opener PIN

- PIN Code: Fill with the Nuki Security Code of the Nuki Lock and/or Nuki Opener. Required for functions that require the security code to be sent to the lock/opener such as setting lock permissions/adding keypad codes, viewing the activity log or changing the Nuki device configuration. Set to "#" to remove the security code from the Nuki Hub configuration.
- PIN Code Ultra/5th gen: Fill with the 6-digit Nuki Security Code of the Nuki Lock Ultra/Go/5th gen Pro. Required for pairing (and many other functions)

#### Unpair Nuki Lock / Unpair Nuki Opener

- Type [4 DIGIT CODE] to confirm unpair: Set to the shown randomly generated code to unpair the Nuki Lock or Opener from the Nuki Hub.

#### Factory reset Nuki Hub

- Type [4 DIGIT CODE] to confirm factory reset: Set to the shown randomly generated code to reset all Nuki Hub settings to default and unpair Nuki Lock and/or Opener. Optionally also reset Wi-Fi settings to default (and reopen the Wi-Fi configurator) by enabling the checkbox.

### GPIO Configuration

- Gpio [2-33]: See the "[GPIO lock control](#gpio-lock-control-optional)" section of this README.

### Import/Export Configuration

The "Import/Export Configuration" menu option allows the importing and exporting of the Nuki Hub settings in JSON format.<br>
<br>
Create a (partial) backup of the current Nuki Hub settings by selecting any of the following:<br>
- Basic export: Will backup all settings that are not considered confidential (as such passwords and pincodes are not included in this export).
- Export with redacted settings: Will backup basic settings and redacted settings such as passwords and pincodes.

Both of the above options will not backup pairing data, so you will have to manually pair Nuki devices when importing this export on a factory reset or new device.

- Export with redacted settings and pairing data: Will backup all settings and pairing data. Can be used to completely restore a factory reset or new device based on the settings of this device. (Re)pairing Nuki devices will not be needed when importing this export.

- Export HTTP SSL certificate and key (PSRAM devices only): Export your HTTP server SSL certificate and key
- Export MQTT SSL CA, client certificate and client key: Export your MQTT SSL CA and client certificate and client key
- Export Coredump: Export the complete log gathered from the last ESP crash

<br>
To import settings copy and paste the contents of the JSON file that is created by any of the above export options and select "Import".
After importing the device will reboot.

### Advanced Configuration

The advanced configuration menu is not reachable from the main menu of the web configurator by default.<br>
You can reach the menu directly by browsing to http://NUKIHUBIP/get?page=advanced or enable showing it in the main menu by browsing to http://NUKIHUBIP/get?page=debugon once (http://NUKIHUBIP/get?page=debugoff to disable).

Note that the following options can break Nuki Hub and cause bootloops that will require you to erase your ESP and reflash following the instructions for first-time flashing.

- Disable Network if not connected within 60s: Enable to allow Nuki Hub to function without a network connection (for example when only using Nuki Hub with GPIO)
- Enable Bootloop prevention: Enable to reset the following stack size and max entry settings to default if Nuki Hub detects a bootloop.
- Char buffer size (min 4096, max 65536): Set the character buffer size, needs to be enlarged to support large amounts of auth/keypad/timecontrol/authorization entries. Default 4096.
- Task size Network (min 12288, max 65536): Set the Network task stack size, needs to be enlarged to support large amounts of auth/keypad/timecontrol/authorization entries. Default 12288.
- Task size Nuki (min 8192, max 65536): Set the Nuki task stack size. Default 8192.
- Max auth log entries (min 1, max 100): The maximum amount of log entries that will be requested from the lock/opener, default 5.
- Max keypad entries (min 1, max 200): The maximum amount of keypad codes that will be requested from the lock/opener, default 10.
- Max timecontrol entries (min 1, max 100): The maximum amount of timecontrol entries that will be requested from the lock/opener, default 10.
- Max authorization entries (min 1, max 100): The maximum amount of authorization entries that will be requested from the lock/opener, default 10.
- Show Pairing secrets on Info page: Enable to show the pairing secrets on the info page. Will be disabled on reboot.
- Manually set lock pairing data: Enable to save the pairing data fields and manually set pairing info for the lock.
- Manually set opener pairing data: Enable to save the pairing data fields and manually set pairing info for the opener.
- Custom URL to update Nuki Hub updater: Set to a HTTPS address to update to a custom Nuki Hub updater binary on next boot of the Nuki Hub partition.
- Custom URL to update Nuki Hub: Set to a HTTPS address to update to a custom Nuki Hub binary on next boot of the Nuki Hub updater partition.
- Force Lock ID to current ID: Enable to force the current Lock ID, irrespective of the config received from the lock.
- Force Lock Keypad connected: Enable to force Nuki Hub to function as if a keypad was connected, irrespective of the config received from the lock.
- Force Lock Doorsensor connected: Enable to force Nuki Hub to function as if a doorsensor was connected, irrespective of the config received from the lock.
- Force Opener ID to current ID: Enable to force the current Opener ID, irrespective of the config received from the opener.
- Force Opener Keypad: Enable to force Nuki Hub to function as if a keypad was connected, irrespective of the config received from the opener.
- Enable Nuki connect debug logging: Enable to log debug information regarding Nuki BLE connection to MQTT and/or Serial.
- Enable Nuki communication debug logging: Enable to log debug information regarding Nuki BLE communication to MQTT and/or Serial.
- Enable Nuki readable data debug logging: Enable to log human readable debug information regarding Nuki BLE to MQTT and/or Serial.
- Enable Nuki hex data debug logging: Enable to log hex debug information regarding Nuki BLE to MQTT and/or Serial.
- Enable Nuki command debug logging: Enable to log debug information regarding Nuki BLE commands to MQTT and/or Serial.
- Pubish free heap over MQTT: Enable to publish free heap to MQTT.

## Exposed MQTT Topics

### Lock

- lock/action: Allows to execute lock actions. After receiving the action, the value is set to "ack". Possible actions: unlock, lock, unlatch, lockNgo, lockNgoUnlatch, fullLock, fobAction1, fobAction2, fobAction3.
- lock/statusUpdated: 1 when the Nuki Lock/Opener signals the KeyTurner state has been updated, resets to 0 when Nuki Hub has queried the updated state.
- lock/state: Reports the current lock state as a string. Possible values are: uncalibrated, locked, unlocked, unlatched, unlockedLnga, unlatching, bootRun, motorBlocked.
- lock/hastate: Reports the current lock state as a string, specifically for use by Home Assistant. Possible values are: locking, locked, unlocking, unlocked, jammed.
- lock/json: Reports the lock state, lockngo_state, trigger, current time, time zone offset, night mode state, last action trigger, last lock action, lock completion status, door sensor state, auth ID and auth name as JSON data.
- lock/binaryState: Reports the current lock state as a string, mostly for use by Home Assistant. Possible values are: locked, unlocked.
- lock/trigger: The trigger of the last action: autoLock, automatic, button, manual, system.
- lock/lastLockAction: Reports the last lock action as a string. Possible values are: Unlock, Lock, Unlatch, LockNgo, LockNgoUnlatch, FullLock, FobAction1, FobAction2, FobAction3, Unknown.
- lock/log: If "Publish auth data" is enabled in the web interface, this topic will be filled with the log of authorization data. By default a maximum of 5 logs are published at a time.
- lock/shortLog: If "Publish auth data" is enabled in the web interface, this topic will be filled with the 3 most recent entries in the log of authorization data, updates faster than lock/log.
- lock/rollingLog: If "Publish auth data" is enabled in the web interface, this topic will be filled with the last log entry from the authorization data. Logs are published in order.
- lock/completionStatus: Status of the last action as reported by Nuki Lock: success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown.
- lock/authorizationId: If enabled in the web interface, this node returns the authorization id of the last lock action.
- lock/authorizationName: If enabled in the web interface, this node returns the authorization name of the last lock action.
- lock/commandResult: Result of the last action as reported by Nuki library: success, failed, timeOut, working, notPaired, error, undefined.
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating.
- lock/rssi: The signal strenght of the Nuki Lock as measured by the ESP32 and expressed by the RSSI Value in dBm.
- lock/address: The BLE address of the Nuki Lock.
- lock/retry: Reports the current number of retries for the current command. 0 when command is successful, "failed" if the number of retries is greater than the maximum configured number of retries.

### Opener

- opener/action: Allows to execute lock actions. After receiving the action, the value is set to "ack". Possible actions: activateRTO, deactivateRTO, electricStrikeActuation, activateCM, deactivateCM, fobAction1, fobAction2, fobAction3.
- opener/state: Reports the current lock state as a string. Possible values are: locked, RTOactive, open, opening, uncalibrated.
- opener/hastate: Reports the current lock state as a string, specifically for use by Home Assistant. Possible values are: locking, locked, unlocking, unlocked, jammed.
- opener/json: Reports the lock state, trigger, ring to open timer, current time, time zone offset, last action trigger, last lock action, lock completion status, door sensor state, auth ID and auth name as JSON data.
- opener/binaryState: Reports the current lock state as a string, mostly for use by Home Assistant. Possible values are: locked, unlocked.
- opener/continuousMode: Enable or disable continuous mode on the opener (0 = disabled; 1 = enabled).
- opener/ring: The string "ring" is published to this topic when a doorbell ring is detected while RTO or continuous mode is active or "ringlocked" when both are inactive.
- opener/binaryRing: The string "ring" is published to this topic when a doorbell ring is detected, the state will revert to "standby" after 2 seconds.
- opener/trigger: The trigger of the last action: autoLock, automatic, button, manual, system.
- opener/lastLockAction: Reports the last lock action as a string. Possible values are: ActivateRTO, DeactivateRTO, ElectricStrikeActuation, ActivateCM, DeactivateCM, FobAction1, FobAction2, FobAction3, Unknown.
- opener/log: If "Publish auth data" is enabled in the web interface, this topic will be filled with the log of authorization data.
- opener/completionStatus: Status of the last action as reported by Nuki Opener: success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown.
- opener/authorizationId: If enabled in the web interface, this topic is set to the authorization id of the last lock action.
- opener/authorizationName: If enabled in the web interface, this topic is set to the authorization name of the last lock action.
- opener/commandResult: Result of the last action as reported by Nuki library: success, failed, timeOut, working, notPaired, error, undefined.
- opener/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating.
- opener/rssi: The bluetooth signal strength of the Nuki Lock as measured by the ESP32 and expressed by the RSSI Value in dBm.
- opener/address: The BLE address of the Nuki Lock.
- opener/retry: Reports the current number of retries for the current command. 0 when command is successful, "failed" if the number of retries is greater than the maximum configured number of retries.

### Configuration
- [lock/opener/]configuration/buttonEnabled: 1 if the Nuki Lock/Opener button is enabled, otherwise 0.
- [lock/opener/]configuration/ledEnabled: 1 if the Nuki Lock/Opener LED is enabled, otherwise 0.
- [lock/]configuration/ledBrightness: Set to the brightness of the LED on the Nuki Lock (0=min; 5=max) (Lock only).
- [lock/]configuration/singleLock: 0 if the Nuki Lock is set to double-lock the door, otherwise 1 (= single-lock) (Lock only).
- [lock/]configuration/autoLock:  1 if the Nuki Lock is set to Auto Lock, otherwise 0 (Lock only).
- [lock/]configuration/autoUnlock: 1 if the Nuki Lock is set to Auto Unlock, otherwise 0 (Lock only).
- [opener/]configuration/soundLevel: Set to the volume for sounds the Nuki Opener plays (0 = min; 255 = max) (Opener only).
- [lock/opener/]configuration/action: Allows changing configuration settings of the Nuki Lock/Opener using a JSON formatted value. After receiving the action, the value is set to "--". See the "[Changing Nuki Lock/Opener Configuration](#changing-nuki-lockopener-configuration)" section of this README for possible actions/values
- [lock/opener/]configuration/commandResult: Result of the last configuration change action as JSON data. See the "[Changing Nuki Lock/Opener Configuration](#changing-nuki-lockopener-configuration)" section of this README for possible values
- [lock/opener/]configuration/basicJson: The current basic configuration of the Nuki Lock/Opener as JSON data. See [Nuki Bluetooth API](https://developer.nuki.io/t/bluetooth-api/27) for available settings. Please note: Longitude and Latitude of the Lock/Opener are not published to MQTT by design. These values can still be changed though.
- [lock/opener/]configuration/advancedJson: The current advanced configuration of the Nuki Lock/Opener as JSON data. See [Nuki Bluetooth API](https://developer.nuki.io/t/bluetooth-api/27) for available settings.
- configuration/action: Allows importing and exporting configuration settings of Nuki Hub using a JSON formatted value. After receiving the action, the value is set to "--", see "[Import and Export Nuki Hub settings over MQTT](#import-and-export-nuki-hub-settings-over-mqtt)"
- configuration/commandResult: Result of the last Nuki Hub configuration import action as JSON data, see "[Import and Export Nuki Hub settings over MQTT](#import-and-export-nuki-hub-settings-over-mqtt)"
- configuration/json: Topic where you can export Nuki Hub configuration as JSON data to, see "[Import and Export Nuki Hub settings over MQTT](#import-and-export-nuki-hub-settings-over-mqtt)"

### Query

- [lock/opener/]query/lockstate: Set to 1 to trigger query lockstate. Auto-resets to 0.
- [lock/opener/]query/config: Set to 1 to trigger query config. Auto-resets to 0.
- [lock/opener/]query/keypad: Set to 1 to trigger query keypad. Auto-resets to 0.
- [lock/opener/]query/battery: Set to 1 to trigger query battery. Auto-resets to 0.
- [lock/opener/]query/lockstateCommandResult: Set to 1 to trigger query lockstate command result. Auto-resets to 0.

### Battery

- [lock/]battery/level: Battery level in percent (Lock only).
- [lock/opener/]battery/critical: 1 if battery level is critical, otherwise 0.
- [lock/]battery/charging: 1 if charging, otherwise 0 (Lock only).
- [lock/opener/]battery/voltage: Current Battery voltage (V).
- [lock/]battery/drain: The drain of the last lock action in Milliwattseconds (mWs) (Lock only).
- [lock/]battery/maxTurnCurrent: The highest current of the turn motor during the last lock action (A) (Lock only).
- [lock/]battery/lockDistance: The total distance during the last lock action in centidegrees (Lock only).
- [lock/opener/]battery/keypadCritical: 1 if the battery level of a connected keypad is critical, otherwise 0.
- [lock/opener/]battery/doorSensorCritical (only available in hybrid mode): 1 if the battery level of a connected doorsensor is critical, otherwise 0.
- [lock/opener/]battery/basicJson: The current battery state (critical, charging, level and keypad critical) of the Nuki Lock/Opener as JSON data.
- [lock/opener/]battery/advancedJson: : The current battery state (critical, batteryDrain, batteryVoltage, lockAction, startVoltage, lowestVoltage, lockDistance, startTemperature, maxTurnCurrent and batteryResistance) of the Nuki Lock/Opener as JSON data.

### Keypad

- See the "[Keypad control](#keypad-control-optional)" section of this README.

### Time Control

- See the "[Time Control](#time-control)" section of this README.

### Info

- info/nukiHubVersion: Set to the current version number of the Nuki Hub firmware.
- [lock/opener/]info/firmwareVersion: Set to the current version number of the Nuki Lock/Opener firmware.
- [lock/opener/]info/hardwareVersion: Set to the hardware version number of the Nuki Lock/Opener.
- info/nukiHubIp: Set to the IP of the Nuki Hub.
- info/nukiHubLatest: Set to the latest available Nuki Hub firmware version number (if update checking is enabled in the settings).

### Maintanence

- maintenance/networkDevice: Set to the name of the network device that is used by the ESP. When using Wi-Fi will be set to "Built-in Wi-Fi". If using Ethernet will be set to "Wiznet W5500", "ETH01-Evo", "Olimex (LAN8720)", "WT32-ETH01", "M5STACK PoESP32 Unit", "LilyGO T-ETH-POE" or "GL-S10".
- maintenance/reset: Set to 1 to trigger a reboot of the ESP. Auto-resets to 0.
- maintenance/update: Set to 1 to auto update Nuki Hub to the latest version from GitHub. Requires the setting "Allow updating using MQTT" to be enabled. Auto-resets to 0.
- maintenance/mqttConnectionState: Last Will and Testament (LWT) topic. "online" when Nuki Hub is connected to the MQTT broker, "offline" if Nuki Hub is not connected to the MQTT broker.
- maintenance/uptime: Uptime in minutes.
- maintenance/wifiRssi: The Wi-Fi signal strength of the Wi-Fi Access Point as measured by the ESP32 and expressed by the RSSI Value in dBm.
- maintenance/log: If "Enable MQTT logging" is enabled in the web interface, this topic will be filled with debug log information.
- maintenance/freeHeap: Only available when debug mode is enabled. Set to the current size of free heap memory in bytes.
- maintenance/restartReasonNukiHub: Set to the last reason Nuki Hub was restarted. See [RestartReason.h](/src/RestartReason.h) for possible values
- maintenance/restartReasonNukiEsp: Set to the last reason the ESP was restarted. See [RestartReason.h](/src/RestartReason.h) for possible values

## Import and Export Nuki Hub settings over MQTT (BETA)

Consider this when deciding if you want to enable the following functionality:

- Any application/actor that has read access to `nukihub/configuration/action` and `nukihub/configuration/json` can view your changes and exports.
- If you have not enabled the setting to require MFA when changing settings any application/actor that has write access to `nukihub/configuration/action` can change Nuki Hub settings (including pairing data and credentials)

### Export Nuki Hub settings over MQTT

To allow Nuki Hub to export configuration over MQTT first enable "Publish Nuki Hub configuration information" in "Access Level Configuration" and save the configuration.
You can export Nuki Hub settings in JSON format by sending the following JSON values to the `nukihub/configuration/action` topic:
- Export Nuki Hub settings without redacted settings and without pairing settings:  `{"exportNH": 0}`

NOTE: The following settings can only be exported if you have setup a secure MQTT connection (MQTT over SSL)

- Export Nuki Hub settings with redacted settings and without pairing settings: `{"exportNH": 0, "redacted": 1}`
- Export Nuki Hub settings without redacted settings and with pairing settings: `{"exportNH": 0, "pairing": 1}`
- Export Nuki Hub settings with redacted settings and pairing settings: `{"exportNH": 0, "redacted": 1, "pairing": 1}`
- Export Nuki Hub MQTTS certificates and key: `{"exportMQTTS": 0}`
- Export Nuki Hub HTTPS certificate and key: `{"exportHTTPS": 0}`

The exported values will be available in the `nukihub/configuration/json` topic in JSON format.
A general explanation of the exported values can be found in the [PreferencesKeys.h](/src/PreferencesKeys.h) file

If you set the value of `exportNH`/`exportMQTTS`/`exportHTTPS` to an integer value > 0 the `nukihub/configuration/json` will be cleared after the given amount of seconds (e.g. `{"exportMQTTS": 30}` will clear the JSON topic after 30 seconds)

If you have enabled `Require MFA (Duo/TOTP) authentication for all sensitive Nuki Hub operations (changing/exporting settings)` you will need to either provide a currently valid TOTP code as part of the sent JSON in the `totp` node or approve the Duo Push before the settings will be exported.

### Import/Change Nuki Hub settings over MQTT

To allow Nuki Hub to import/change configuration over MQTT first enable "Modify Nuki Hub configuration over MQTT" in "Access Level Configuration" and save the configuration.
You can import Nuki Hub settings in JSON format by sending the desired JSON values to be changed to the `nukihub/configuration/action` topic.
The expected values and format is the same as the JSON files/values that can be exported over MQTT or through the Web Configurator.

The result of the import will be available in the `nukihub/configuration/commandResult` topic in JSON format.
After the import is complete the ESP32 will reboot.

If you have enabled `Require MFA (Duo/TOTP) authentication for all sensitive Nuki Hub operations (changing/exporting settings)` you will need to either provide a currently valid TOTP code as part of the sent JSON in the `totp` node or approve the Duo Push before the settings will be changed/imported.

Note: When importing settings using MQTT there are less/no checks on the values entered. These checks are only available when changing settings through the WebConfigurator.
Consider testing your configuration values by changing them in the Web Configurator before trying to use MQTT to change configuration.
A general explanation of the values that can be imported can be found in the [PreferencesKeys.h](/src/PreferencesKeys.h) file

## Changing Nuki Lock/Opener Configuration

To change Nuki Lock/Opener settings set the `configuration/action` topic to a JSON formatted value with any of the following settings. Multiple settings can be changed at once. See [Nuki Bluetooh API](https://developer.nuki.io/t/bluetooth-api/27) for more information on the available settings.<br>
Changing settings has to enabled first in the configuration portal. Check the settings you want to be able to change under "Nuki Lock/Opener Config Control" in "Access Level Configuration" and save the configuration.

### Nuki Lock Configuration

| Setting                                 | Usage                                                                                            | Possible values                                                   | Example                            |
|-----------------------------------------|--------------------------------------------------------------------------------------------------|-------------------------------------------------------------------|------------------------------------|
| name                                    | The name of the Smart Lock.                                                                      | Alphanumeric string, max length 32 chars                          |`{ "name": "Frontdoor" }`           |
| latitude                                | The latitude of the Smart Locks geoposition.                                                     | Float                                                             |`{ "latitude": "48.858093" }`       |
| longitude                               | The longitude of the Smart Locks geoposition                                                     | Float                                                             |`{ "longitude": "2.294694" }`       |
| autoUnlatch                             | Whether or not the door shall be unlatched by manually operating a door handle from the outside. | 1 = enabled, 0 = disabled                                         |`{ "autoUnlatch": "1" }`            |
| pairingEnabled                          | Whether or not activating the pairing mode via button should be enabled.                         | 1 = enabled, 0 = disabled                                         |`{ "pairingEnabled": "0" }`         |
| buttonEnabled                           | Whether or not the button should be enabled.                                                     | 1 = enabled, 0 = disabled                                         |`{ "buttonEnabled": "1" }`          |
| ledEnabled                              | Whether or not the flashing LED should be enabled to signal an unlocked door.                    | 1 = enabled, 0 = disabled                                         |`{ "ledEnabled": "1" }`             |
| ledBrightness                           | The LED brightness level                                                                         | 0 = off, …, 5 = max                                               |`{ "ledBrightness": "2" }`          |
| timeZoneOffset                          | The timezone offset (UTC) in minutes                                                             | Integer between 0 and 60                                          |`{ "timeZoneOffset": "0" }`         |
| dstMode                                 | The desired daylight saving time mode.                                                           | 0 = disabled, 1 = European                                        |`{ "dstMode": "0" }`                |
| fobAction1                              | The desired action, if a Nuki Fob is pressed once.                                               | "No Action", "Unlock", "Lock", "Lock n Go", "Intelligent"         |`{ "fobAction1": "Lock n Go" }`     |
| fobAction2                              | The desired action, if a Nuki Fob is pressed twice.                                              | "No Action", "Unlock", "Lock", "Lock n Go", "Intelligent"         |`{ "fobAction2": "Intelligent" }`   |
| fobAction3                              | The desired action, if a Nuki Fob is pressed three times.                                        | "No Action", "Unlock", "Lock", "Lock n Go", "Intelligent"         |`{ "fobAction3": "Unlock" }`        |
| singleLock                              | Whether only a single lock or double lock should be performed                                    | 0 = double lock, 1 = single lock                                  |`{ "singleLock": "0" }`             |
| advertisingMode                         | The desired advertising mode.                                                                    | "Automatic", "Normal", "Slow", "Slowest"                          |`{ "advertisingMode": "Normal" }`   |
| timeZone                                | The current timezone or "None" if timezones are not supported                                    | "None" or one of the timezones from [Nuki Bluetooh API](https://developer.nuki.io/t/bluetooth-api/27)                                                                                                                                                                              |`{ "timeZone": "Europe/Berlin" }`   |
| unlockedPositionOffsetDegrees           | Offset that alters the unlocked position in degrees.                                             | Integer between -90 and 180                              |`{ "unlockedPositionOffsetDegrees": "-90" }` |
| lockedPositionOffsetDegrees             | Offset that alters the locked position in degrees.                                               | Integer between -180 and 90                                 |`{ "lockedPositionOffsetDegrees": "80" }` |
| singleLockedPositionOffsetDegrees       | Offset that alters the single locked position in degrees.                                        | Integer between -180 and 180                         |`{ "singleLockedPositionOffsetDegrees": "120" }` |
| unlockedToLockedTransitionOffsetDegrees | Offset that alters the position where transition from unlocked to locked happens in degrees.     | Integer between -180 and 180                   |`{ "unlockedToLockedTransitionOffsetDegrees": "180" }` |
| lockNgoTimeout                          | Timeout for lock ‘n’ go in seconds                                                               | Integer between 5 and 60                                          |`{ "lockNgoTimeout": "60" }`        |
| singleButtonPressAction                 | The desired action, if the button is pressed once.                  | "No Action", "Intelligent", "Unlock", "Lock", "Unlatch", "Lock n Go", "Show Status"   |`{ "singleButtonPressAction": "Lock n Go" }` |
| doubleButtonPressAction                 | The desired action, if the button is pressed twice.                 | "No Action", "Intelligent", "Unlock", "Lock", "Unlatch", "Lock n Go", "Show Status" |`{ "doubleButtonPressAction": "Show Status" }` |
| detachedCylinder                        | Wheter the inner side of the used cylinder is detached from the outer side.                      | 0 = not detached, 1 = detached                                    |`{ "detachedCylinder": "1" }`       |
| batteryType                             | The type of the batteries present in the smart lock.                                             | "Alkali", "Accumulators", "Lithium"                               |`{ "batteryType": "Accumulators" }` |
| automaticBatteryTypeDetection           | Whether the automatic detection of the battery type is enabled.                                  | 1 = enabled, 0 = disabled                          |`{ "automaticBatteryTypeDetection": "Lock n Go" }` |
| unlatchDuration                         | Duration in seconds for holding the latch in unlatched position.                                 | Integer between 1 and 30                                          |`{ "unlatchDuration": "3" }`        |
| autoLockTimeOut                         | Seconds until the smart lock relocks itself after it has been unlocked.                          | Integer between 30 and 1800                                       |`{ "autoLockTimeOut": "60" }`       |
| autoUnLockDisabled                      | Whether auto unlock should be disabled in general.                                               | 1 = auto unlock disabled, 0 = auto unlock enabled                 |`{ "autoUnLockDisabled": "1" }`     |
| nightModeEnabled                        | Whether nightmode is enabled.                                                                    | 1 = enabled, 0 = disabled                                         |`{ "nightModeEnabled": "1" }`       |
| nightModeStartTime                      | Start time for nightmode if enabled.                                                             | Time in "HH:MM" format                                            |`{ "nightModeStartTime": "22:00" }` |
| nightModeEndTime                        | End time for nightmode if enabled.                                                               | Time in "HH:MM" format                                            |`{ "nightModeEndTime": "07:00" }`   |
| nightModeAutoLockEnabled                | Whether auto lock should be enabled during nightmode.                                            | 1 = enabled, 0 = disabled                                        |`{ "nightModeAutoLockEnabled": "1" }`|
| nightModeAutoUnlockDisabled             | Whether auto unlock should be disabled during nightmode.                                         | 1 = auto unlock disabled, 0 = auto unlock enabled             |`{ "nightModeAutoUnlockDisabled": "1" }`|
| nightModeImmediateLockOnStart           | Whether the door should be immediately locked on nightmode start.                                | 1 = enabled, 0 = disabled                                   |`{ "nightModeImmediateLockOnStart": "1" }`|
| autoLockEnabled                         | Whether auto lock is enabled.                                                                    | 1 = enabled, 0 = disabled                                         |`{ "autoLockEnabled": "1" }`        |
| immediateAutoLockEnabled                | Whether auto lock should be performed immediately after the door has been closed.                | 1 = enabled, 0 = disabled                                        |`{ "immediateAutoLockEnabled": "1" }`|
| autoUpdateEnabled                       | Whether automatic firmware updates should be enabled.                                            | 1 = enabled, 0 = disabled                                         |`{ "autoUpdateEnabled": "1" }`      |
| motorSpeed                              | The desired motor speed (Ultra/5th gen Pro only)                                                 | "Standard", "Insane", "Gentle"                                    |`{ "motorSpeed": "Standard" }`      |
| enableSlowSpeedDuringNightMode          | Whether the slow speed should be applied during Night Mode (Ultra/5th gen Pro only)              | 1 = enabled, 0 = disabled                            |`{ "enableSlowSpeedDuringNightMode": "1" }`      |
| rebootNuki                              | Reboot the Nuki device immediately                                                               | 1 = reboot nuki                                                   |`{ "rebootNuki": "1" }`             |

### Nuki Opener Configuration

| Setting                                 | Usage                                                                                            | Possible values                                                   | Example                            |
|-----------------------------------------|--------------------------------------------------------------------------------------------------|-------------------------------------------------------------------|------------------------------------|
| name                                    | The name of the Opener.                                                                          | Alphanumeric string, max length 32 chars                          |`{ "name": "Frontdoor" }`           |
| latitude                                | The latitude of the Openers geoposition.                                                         | Float                                                             |`{ "latitude": "48.858093" }`       |
| longitude                               | The longitude of the Openers geoposition                                                         | Float                                                             |`{ "longitude": "2.294694" }`       |
| pairingEnabled                          | Whether or not activating the pairing mode via button should be enabled.                         | 1 = enabled, 0 = disabled                                         |`{ "pairingEnabled": "0" }`         |
| buttonEnabled                           | Whether or not the button should be enabled.                                                     | 1 = enabled, 0 = disabled                                         |`{ "buttonEnabled": "1" }`          |
| ledFlashEnabled                         | Whether or not the flashing LED should be enabled to signal CM or RTO.                           | 1 = enabled, 0 = disabled                                         |`{ "ledFlashEnabled": "1" }`        |
| timeZoneOffset                          | The timezone offset (UTC) in minutes                                                             | Integer between 0 and 60                                          |`{ "timeZoneOffset": "0" }`         |
| dstMode                                 | The desired daylight saving time mode.                                                           | 0 = disabled, 1 = European                                        |`{ "dstMode": "0" }`                |
| fobAction1                              | The desired action, if a Nuki Fob is pressed once.                                     | "No Action", "Toggle RTO", "Activate RTO", "Deactivate RTO", "Open", "Ring" |`{ "fobAction1": "Toggle RTO" }`    |
| fobAction2                              | The desired action, if a Nuki Fob is pressed twice.                                    | "No Action", "Toggle RTO", "Activate RTO", "Deactivate RTO", "Open", "Ring" |`{ "fobAction2": "Open" }`          |
| fobAction3                              | The desired action, if a Nuki Fob is pressed three times.                              | "No Action", "Toggle RTO", "Activate RTO", "Deactivate RTO", "Open", "Ring" |`{ "fobAction3": "Ring" }`          |
| operatingMode                           | The desired operating mode                                                             | "Generic door opener", "Analogue intercom", "Digital intercom", "Siedle", "TCS", "Bticino", "Siedle HTS", "STR", "Ritto", "Fermax", "Comelit", "Urmet BiBus", "Urmet 2Voice", "Golmar", "SKS", "Spare"                                                                                                                            |`{ "operatingMode": "TCS" }`        |
| advertisingMode                         | The desired advertising mode.                                                                    | "Automatic", "Normal", "Slow", "Slowest"                          |`{ "advertisingMode": "Normal" }`   |
| timeZone                                | The current timezone or "None" if timezones are not supported                                    | "None" or one of the timezones from [Nuki Bluetooh API](https://developer.nuki.io/t/bluetooth-api/27)                                                                                                                                                                              |`{ "timeZone": "Europe/Berlin" }`   |
| intercomID                              | Database ID of the connected intercom.                                                           | Integer                                                           |`{ "intercomID": "1" }`             |
| busModeSwitch                           | Method to switch between data and analogue mode                                                  | 0 = none, 1 =vshort circuit                                       |`{ "busModeSwitch": "0" }`          |
| shortCircuitDuration                    | Duration of the short circuit for BUS mode switching in ms.                                      | Integer                                                           |`{ "shortCircuitDuration": "250" }` |
| electricStrikeDelay                     | Delay in ms of electric strike activation in case of an electric strike actuation by RTO         | Integer between 0 and 30000                                       |`{ "electricStrikeDelay": "2080" }` |
| randomElectricStrikeDelay               | Random delay (3-7s) in order to simulate a person inside actuating the electric strike.          | 1 = enabled, 0 = disabled                                       |`{ "randomElectricStrikeDelay": "1" }`|
| electricStrikeDuration                  | Duration in ms of electric strike actuation.                                  .                  | Integer between 1000 and 30000                                 |`{ "electricStrikeDuration": "5000" }` |
| disableRtoAfterRing                     | Whether to disable RTO after ring.                                                               | 1 = disable RTO after ring, 0 = Don't disable RTO after ring      |`{ "disableRtoAfterRing": "0" }`    |
| rtoTimeout                              | After this period of time in minutes, RTO gets deactivated automatically                         | Integer between 5 and 60                                          |`{ "rtoTimeout": "60" }`            |
| doorbellSuppression                     | Whether the doorbell is suppressed when Ring, CM and/or RTO are active     | "Off", "CM", "RTO", "CM & RTO", "Ring", "CM & Ring", "RTO & Ring", "CM & RTO & Ring"|`{ "doorbellSuppression": "CM & Ring" }`|
| doorbellSuppressionDuration             | Duration in ms of doorbell suppression.                                                          | Integer between 500 and 10000                             |`{ "doorbellSuppressionDuration": "2000" }` |
| soundRing                               | The Ring sound                                                                                   | "No Sound", "Sound 1", "Sound 2", "Sound 3"                       |`{ "soundRing": "No Sound" }`       |
| soundOpen                               | The Open sound.                                                                                  | "No Sound", "Sound 1", "Sound 2", "Sound 3"                       |`{ "soundOpen": "Sound 1" }`        |
| soundRto                                | The RTO sound.                                                                                   | "No Sound", "Sound 1", "Sound 2", "Sound 3"                       |`{ "soundRto": "Sound 2" }`         |
| soundCm                                 | The CM sound.                                                                                    | "No Sound", "Sound 1", "Sound 2", "Sound 3"                       |`{ "soundCm": "Sound 3" }`          |
| soundConfirmation                       | Sound confirmation                                                                               | 0 = no sound, 1 = sound                                           |`{ "soundConfirmation": "1" }`      |
| soundLevel                              | The sound level for the opener                                                                   | Integer between 0 and 255                                         |`{ "soundLevel": "200" }`           |
| singleButtonPressAction      | The desired action, if the button is pressed once.  | "No Action", "Toggle RTO", "Activate RTO", "Deactivate RTO", "Toggle CM", "Activate CM", "Deactivate CM", "Open"      |`{ "singleButtonPressAction": "Open" }` |
| doubleButtonPressAction      | The desired action, if the button is pressed twice. | "No Action", "Toggle RTO", "Activate RTO", "Deactivate RTO", "Toggle CM", "Activate CM", "Deactivate CM", "Open" |`{ "doubleButtonPressAction": "No Action" }` |
| batteryType                             | The type of the batteries present in the smart lock.                                             | "Alkali", "Accumulators", "Lithium"                               |`{ "batteryType": "Accumulators" }` |
| automaticBatteryTypeDetection           | Whether the automatic detection of the battery type is enabled.                                  | 1 = enabled, 0 = disabled                                  |`{ "automaticBatteryTypeDetection": "1" }` |
| rebootNuki                              | Reboot the Nuki device immediately                                                               | 1 = reboot nuki                                                   |`{ "rebootNuki": "1" }`             |

Example usage for changing multiple settings at once:<br>
- `{ "buttonEnabled": "1", "lockngoTimeout": "60", "automaticBatteryTypeDetection": "1" }`
- `{ "fobAction1": "Unlock", "fobAction2": "Intelligent", "nightModeImmediateLockOnStart": "1" }`

### Result of attempted configuration changes

The result of the last configuration change action will be published to the `configuration/commandResult` MQTT topic as JSON data.<br>
<br>
The JSON data will include a node called "general" and a node for every setting that Nuki Hub detected in the action.<br>
Possible values for the "general" node are "noValidPinSet", "invalidJson", "invalidConfig", "success" and "noChange".<br>
Possible values for the node per setting are "unchanged", "noValueSet", "invalidValue", "valueTooLong", "accessDenied", "success", "failed", "timeOut", "working", "notPaired", "error" and "undefined"<br>
<br>
Example:
- `{"advertisingMode":"success","general":"success"}`

### Home Assistant discovery

If Home Assistant discovery is enabled (see the [Home  Assistant Discovery](#home-assistant-discovery-optional) section of this README) Nuki Hub will create entities for almost all of the above settings.

## Upgrading using Over-the-air Update (OTA)

After the initial installation of the Nuki Hub firmware via serial connection, further updates can be deployed via OTA update from a browser.<br>
In the configuration portal, select "Firmware update" from the main page.<br>
<br>
The easiest way to upgrade Nuki Hub, if Nuki Hub is connected to the internet, is to select "Update to latest version".<br>
This will download the latest Nuki Hub and Nuki Hub updater and automatically upgrade both applications.<br>
Nuki Hub will reboot 3 times during this process, which will take about 5 minutes.<br>
If you have enabled "Allow updating using MQTT" you can also use the Home Assistant updater or write "1" to the `nukihub/maintanance/update` topic to start the update process.<br>
<br>
Alternatively you can select a binary file from your file system to update Nuki Hub or the Nuki Hub updator manually<br>
You can only update Nuki Hub from the Nuki Hub updater and update the updater only from Nuki Hub<br>
You can reboot from Nuki Hub to the updater and vice versa by selecting the reboot option from the "Firmware update" page<br>
When you are on the right application you can upload the new binary by clicking on "Browse" and select the new "nuki_hub\[board\].bin" or "nuki_hub_updater\[board\].bin" file and select "Upload file".<br>
After about a minute the new firmware should be installed afterwhich the ESP will reboot automatically to the updated binary.<br>
Selecting the wrong binary will lead to an unsuccessfull update<br>
<br>
<b> Note for users upgrading from Nuki Hub 9.09 or lower:</b><br>
Updating to version 9.10 requires a change to the partition table of the ESP32.<br>
Please follow the instructions for the [First time installation](#first-time-installation) one time when updating to Nuki Hub 9.10 from an earlier version.<br>

## MQTT Encryption (optional)

The communication via MQTT can be SSL encrypted.<br>
Note: MQTT SSL requires a significant amount of RAM and might not work on low RAM devices (ESP32 without PSRAM or ESP32-C3)

To enable SSL encryption, supply the necessary information in the MQTT Configuration page.<br>
<br>
The following configurations are supported:<br>
CA, CERT and KEY are empty -> No encryption<br>
CA is filled but CERT and KEY are empty -> Encrypted MQTT<br>
CA, CERT and KEY are filled -> Encrypted MQTT with client vaildation<br>
<br>
Example certificate creation for your MQTT server:
```console
# make a ca key
openssl genpkey -algorithm RSA -out ca.key

# make a CA cert
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt -subj "/C=US/ST=YourState/L=YourCity/O=YourOrganization/OU=YourUnit/CN=YourCAName"

# make a server key
openssl genpkey -algorithm RSA -out server.key

# Make a sign request, MAKE SURE THE CN MATCHES YOUR MQTT SERVERNAME
openssl req -new -key server.key -out server.csr -subj "/C=US/ST=YourState/L=YourCity/O=YourOrganization/OU=YourUnit/CN=homeserver.local"

# sign it
 openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 3650
```

## HTTPS Server (optional, PSRAM enabled devices only)

The Webconfigurator can use/force HTTPS on PSRAM enabled devices.<br>
To enable SSL encryption, supply the certificate and key in the Network configuration page.<br>
You can also let Nuki Hub generate a self-signed certificate by clicking "Generate" on the Network configuration page.<br>
Reboot Nuki Hub afterwards to enable to HTTPS server (and disable HTTP).<br>

Example self-signed certificate creation for your HTTPS server:
```console

# make a Certificate and key pair, MAKE SURE THE CN MATCHES THE DOMAIN AT WHICH NUKI HUB IS AVAILABLE
openssl req -x509 -nodes -days 3650 -newkey rsa:2048 -keyout nukihub.key -out nukihub.crt -subj "/C=US/ST=YourState/L=YourCity/O=YourOrganization/OU=YourUnit/CN=YourCAName"

```

Although you can use the HTTPS server in this way your client device will not trust the certificate by default.
An option would be to configure a proxy SSL server (such as Caddy, Traefik, nginx) with a non-self signed (e.g. let's encrypt) SSL certificate and have this proxy server connect to Nuki Hub over SSL and trust the self-signed NukiHub certificate for this connection.

## Home Assistant Discovery (optional)

This software supports [MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/) for integrating Nuki Hub with Home Assistant.<br>
To enable autodiscovery, enable the checkbox on the "MQTT Configuration" page.<br>
Once enabled, the Nuki Lock and/or Opener and related entities should automatically appear in your Home Assistant MQTT devices.

The following mapping between Home Assistant services and Nuki commands is setup when enabling autodiscovery:
|             | Smartlock | Opener (default)          | Opener (alternative)      |
|-------------|-----------|---------------------------|---------------------------|
| lock.lock   | Lock      | Disable Ring To Open      | Disable Continuous Mode   |
| lock.unlock | Unlock    | Enable Ring To Open       | Enable Continuous Mode    |
| lock.open   | Unlatch   | Electric Strike Actuation | Electric Strike Actuation |

NOTE: MQTT Discovery uses retained MQTT messages to store devices configurations.<br>
In order to avoid orphan configurations on your broker please disable autodiscovery first if you no longer want to use this software.<br>
Retained messages are automatically cleared when unpairing and when changing/disabling autodiscovery topic in MQTT Configuration page.<br>
If you experience "ghost" entities/devices related to Nuki Hub you can completely purge Nuki Hub related Home Assistant discovery topics from your MQTT broker by following the instructions [here](#purging-home-assistant-discovery-mqtt-topics)

## Keypad control using JSON (optional)

If a keypad is connected to the lock, keypad codes can be added, updated and removed. This has to enabled first in the configuration portal. Check "Add, modify and delete keypad codes" under "Access Level Configuration" and save the configuration.

Information about current keypad codes is published as JSON data to the "[lock/opener]/keypad/json" MQTT topic.<br>
This needs to be enabled separately by checking "Publish keypad codes information" under "Access Level Configuration" and saving the configuration.
For security reasons, the code itself is not published, unless this is explicitly enabled in the Nuki Hub settings.
By default a maximum of 10 entries are published.

To change Nuki Lock/Opener keypad settings set the `[lock/opener]/keypad/actionJson` topic to a JSON formatted value containing the following nodes.

| Node             | Delete   | Add      | Update   |  Check   | Usage                                                                                                            | Possible values                        |
|------------------|----------|----------|----------|----------|------------------------------------------------------------------------------------------------------------------|----------------------------------------|
| action           | Required | Required | Required | Required | The action to execute                                                                                            | "delete", "add", "update", "check"     |
| codeId           | Required | Not used | Required | Required | The code ID of the existing code to delete or update                                                             | Integer                                |
| code             | Not used | Required | Optional | Required | The code to create or update                                                                       | 6-digit Integer without zero's, can't start with "12"|
| enabled          | Not used | Not used | Optional | Not used | Enable or disable the code, always enabled on add                                                                | 1 = enabled, 0 = disabled              |
| name             | Not used | Required | Optional | Not used | The name of the code to create or update                                                                         | String, max 20 chars                   |
| timeLimited      | Not used | Optional | Optional | Not used | If this authorization is restricted to access only at certain times, requires enabled = 1                        | 1 = enabled, 0 = disabled              |
| allowedFrom      | Not used | Optional | Optional | Not used | The start timestamp from which access should be allowed (requires enabled = 1 and timeLimited = 1)               | "YYYY-MM-DD HH:MM:SS"                  |
| allowedUntil     | Not used | Optional | Optional | Not used | The end timestamp until access should be allowed (requires enabled = 1 and timeLimited = 1)                      | "YYYY-MM-DD HH:MM:SS"                  |
| allowedWeekdays  | Not used | Optional | Optional | Not used | Weekdays on which access should be allowed (requires enabled = 1 and timeLimited = 1)     | Array of days: "mon", "tue", "wed", "thu" , "fri" "sat", "sun"|
| allowedFromTime  | Not used | Optional | Optional | Not used | The start time per day from which access should be allowed (requires enabled = 1 and timeLimited = 1)            | "HH:MM"                                |
| allowedUntilTime | Not used | Optional | Optional | Not used | The end time per day until access should be allowed (requires enabled = 1 and timeLimited = 1)                   | "HH:MM"                                |

Examples:
- Delete: `{ "action": "delete", "codeId": "1234" }`
- Add: `{ "action": "add", "code": "589472", "name": "Test", "timeLimited": "1", "allowedFrom": "2024-04-12 10:00:00", "allowedUntil": "2034-04-12 10:00:00", "allowedWeekdays": [ "wed", "thu", "fri" ], "allowedFromTime": "08:00", "allowedUntilTime": "16:00" }`
- Update: `{ "action": "update", "codeId": "1234", "enabled": "1", "name": "Test", "timeLimited": "1", "allowedFrom": "2024-04-12 10:00:00", "allowedUntil": "2034-04-12 10:00:00", "allowedWeekdays": [ "mon", "tue", "sat", "sun" ], "allowedFromTime": "08:00", "allowedUntilTime": "16:00" }`

### Result of attempted keypad code changes

The result of the last keypad change action will be published to the `[lock/opener]/configuration/commandResultJson` MQTT topic.<br>
Possible values are "noValidPinSet", "keypadControlDisabled", "keypadNotAvailable", "keypadDisabled", "invalidConfig", "invalidJson", "noActionSet", "invalidAction", "noExistingCodeIdSet", "noNameSet", "noValidCodeSet", "noCodeSet", "invalidAllowedFrom", "invalidAllowedUntil", "invalidAllowedFromTime", "invalidAllowedUntilTime", "success", "failed", "timeOut", "working", "notPaired", "error" and "undefined".<br>

## Keypad control (alternative, optional)

If a keypad is connected to the lock, keypad codes can be added, updated and removed.
This has to enabled first in the configuration portal. Check "Add, modify and delete keypad codes" under "Access Level Configuration" and save the configuration.

Information about codes is published under "keypad/code_x", x starting from 0 up the number of configured codes. This needs to be enabled separately by checking "Publish keypad codes information" under "Access Level Configuration" and saving the configuration.
By default a maximum of 10 entries are published.

For security reasons, the code itself is not published, unless this is explicitly enabled in the Nuki Hub settings. To modify keypad codes, a command
structure is setup under keypad/command:

- keypad/command/id: The id of an existing code, found under keypad_code_x
- keypad/command/name: Display name of the code
- keypad/command/code: The actual 6-digit keypad code
- keypad/command/enabled: Set to 1 to enable the code, 0 to disable
- keypad/command/action: The action to execute. Possible values are add, delete and update

To modify keypad codes, the first four parameter nodes have to be set depending on the command:

- To add a code, set name, code, enabled **
- To delete a code, set id
- To update a code, set id, name, code, enabled

** Note: Rules for codes are:
- The code must be a 6 digit number
- The code can't contain 0
- The code can't start with 12

After setting the necessary parameters, write the action to be executed to the command node.
For example, to add a code:
- write "John Doe" to name
- write 111222 to code
- write 1 to enabled
- write "add" to action

## Timecontrol using JSON (optional)

Timecontrol entries can be added, updated and removed. This has to enabled first in the configuration portal. Check "Add, modify and delete timecontrol entries" under "Access Level Configuration" and save the configuration.

Information about current timecontrol entries is published as JSON data to the "[lock/opener]/timecontrol/json" MQTT topic.<br>
This needs to be enabled separately by checking "Publish timecontrol entries information" under "Access Level Configuration" and saving the configuration.
By default a maximum of 10 entries are published.

To change Nuki Lock/Opener timecontrol settings set the `[lock/opener]/timecontrol/actionJson` topic to a JSON formatted value containing the following nodes.

| Node             | Delete   | Add      | Update   | Usage                                                                                    | Possible values                                                |
|------------------|----------|----------|----------|------------------------------------------------------------------------------------------|----------------------------------------------------------------|
| action           | Required | Required | Required | The action to execute                                                                    | "delete", "add", "update"                                      |
| entryId          | Required | Not used | Required | The entry ID of the existing entry to delete or update                                   | Integer                                                        |
| enabled          | Not used | Not used | Optional | Enable or disable the entry, always enabled on add                                       | 1 = enabled, 0 = disabled                                      |
| weekdays         | Not used | Optional | Optional | Weekdays on which the chosen lock action should be exectued (requires enabled = 1)       | Array of days: "mon", "tue", "wed", "thu" , "fri" "sat", "sun" |
| time             | Not used | Required | Optional | The time on which the chosen lock action should be executed (requires enabled = 1)       | "HH:MM"                                                        |
| lockAction       | Not used | Required | Optional | The lock action that should be executed on the chosen weekdays at the chosen time (requires enabled = 1) | For the Nuki lock: "Unlock", "Lock", "Unlatch", "LockNgo", "LockNgoUnlatch", "FullLock". For the Nuki Opener: "ActivateRTO", "DeactivateRTO", "ElectricStrikeActuation", "ActivateCM", "DeactivateCM |

Examples:
- Delete: `{ "action": "delete", "entryId": "1234" }`
- Add: `{ "action": "add", "weekdays": [ "wed", "thu", "fri" ], "time": "08:00", "lockAction": "Unlock" }`
- Update: `{ "action": "update", "entryId": "1234", "enabled": "1", "weekdays": [ "mon", "tue", "sat", "sun" ], "time": "08:00", "lockAction": "Lock" }`

## Authorization entries control using JSON (optional)

Authorization entries can be updated and removed. This has to enabled first in the configuration portal. Check "Modify and delete authorization entries" under "Access Level Configuration" and save the configuration.
It is currently not (yet) possible to add authorization entries this way.

Information about current authorization entries is published as JSON data to the "[lock/opener]/authorization/json" MQTT topic.<br>
This needs to be enabled separately by checking "Publish authorization entries information" under "Access Level Configuration" and saving the configuration.
By default a maximum of 10 entries are published.

To change Nuki Lock/Opener authorization settings set the `[lock/opener]/authorization/action` topic to a JSON formatted value containing the following nodes.

| Node             | Delete   | Add      | Update   | Usage                                                                                                            | Possible values                        |
|------------------|----------|----------|----------|------------------------------------------------------------------------------------------------------------------|----------------------------------------|
| action           | Required | Required | Required | The action to execute                                                                                            | "delete", "add", "update"              |
| authId           | Required | Not used | Required | The auth ID of the existing entry to delete or update                                                            | Integer                                |
| enabled          | Not used | Not used | Optional | Enable or disable the authorization, always enabled on add                                                       | 1 = enabled, 0 = disabled              |
| name             | Not used | Required | Optional | The name of the authorization to create or update                                                                | String, max 20 chars                   |
| remoteAllowed    | Not used | Optional | Optional | If this authorization is allowed remote access, requires enabled = 1                                             | 1 = enabled, 0 = disabled              |
| timeLimited      | Not used | Optional | Optional | If this authorization is restricted to access only at certain times, requires enabled = 1                        | 1 = enabled, 0 = disabled              |
| allowedFrom      | Not used | Optional | Optional | The start timestamp from which access should be allowed (requires enabled = 1 and timeLimited = 1)               | "YYYY-MM-DD HH:MM:SS"                  |
| allowedUntil     | Not used | Optional | Optional | The end timestamp until access should be allowed (requires enabled = 1 and timeLimited = 1)                      | "YYYY-MM-DD HH:MM:SS"                  |
| allowedWeekdays  | Not used | Optional | Optional | Weekdays on which access should be allowed (requires enabled = 1 and timeLimited = 1)     | Array of days: "mon", "tue", "wed", "thu" , "fri" "sat", "sun"|
| allowedFromTime  | Not used | Optional | Optional | The start time per day from which access should be allowed (requires enabled = 1 and timeLimited = 1)            | "HH:MM"                                |
| allowedUntilTime | Not used | Optional | Optional | The end time per day until access should be allowed (requires enabled = 1 and timeLimited = 1)                   | "HH:MM"                                |

Examples:
- Delete: `{ "action": "delete", "authId": "1234" }`
- Update: `{ "action": "update", "authId": "1234", "enabled": "1", "name": "Test", "timeLimited": "1", "allowedFrom": "2024-04-12 10:00:00", "allowedUntil": "2034-04-12 10:00:00", "allowedWeekdays": [ "mon", "tue", "sat", "sun" ], "allowedFromTime": "08:00", "allowedUntilTime": "16:00" }`

## GPIO lock control (optional)

The lock can be controlled via GPIO. To trigger actions, a connection to ground has to be present for at least 300ms (or to +3.3V for "General input (pull-down)"). <br>
<br>
To enable GPIO control, go the the "GPIO Configuration" page where each GPIO can e configured for a specific role:
- Disabled: The GPIO is disabled
- Input: Lock: When connect to Ground, a lock command is sent to the lock
- Input: Unlock: When connect to Ground, an unlock command is sent to the lock
- Input: Unlatch: When connect to Ground, an unlatch command is sent to the lock
- Input: Lock n Go: When connect to Ground, a Lock n Go command is sent to the lock
- Input: Lock n Go and unlatch: When connect to Ground, a Lock n Go and unlatch command is sent to the lock
- Input: Electric strike actuation: When connect to Ground, an electric strike actuation command is sent to the opener (open door for configured amount of time)
- Input: Activate RTO: When connect to Ground, Ring-to-open is activated (opener)
- Input: Activate CM: When connect to Ground, Continuous mode is activated (opener)
- Input: Deactivate RTO/CM: Disable RTO or CM, depending on which is active (opener)
- Input: Dectivate RTO: When connect to Ground, Ring-to-open is deactivated (opener)
- Input: Dectivate CM: When connect to Ground, Continuous mode is deactivated (opener)
- Output: High when locked: Outputs a high signal when the door is locked
- Output: High when unlocked: Outputs a high signal when the door is unlocked
- Output: High when motor blocked: Outputs a high signal when the motor is blocked (lock)
- Output: High when RTO active: Outputs a high signal when ring-to-open is active (opener)
- Output: High when CM active: Outputs a high signal when continuous mode is active (opener)
- Output: High when RTO or CM active: Outputs a high signal when either ring-to-open or continuous mode is active (opener)
- General input (pull-down): The pin is configured in pull-down configuration and its state is published to the "gpio/pin_x/state" topic
- General input (pull-up): The pin is configured in pull-up configuration and its state is published to the "gpio/pin_x/state" topic
- Genral output: The pin is set to high or low depending on the "gpio/pin_x/state" topic

## Connecting via Ethernet (Optional)

If you prefer to connect to via Ethernet instead of Wi-Fi, you either use one of the supported ESP32 modules with built-in ethernet (see "[Supported devices](#supported-devices)" section)
or wire a seperate SPI Ethernet module.<Br>
Currently the Wiznet W5x00 Module (W5100, W5200, W5500), DN9051 and KSZ8851SNL chips are supported.<br>
To use a supported module, flash the firmware, connect via Wi-Fi and select the correct network hardware in the "Network Configuration" section.

To wire an external W5x00 module to the ESP, use this wiring scheme:

- Connect W5x00 to ESP32 SPI:<br>
  - W5x00 SCK to GPIO 8<br>
  - W5x00 MISO to GPIO 9<br>
  - W5x00 MOSI to GPIO 10<br>
  - W5x00 CS/SS to GPIO 5<br>
  Optional:
  - W5x00 RST to GPIO 4<br>
  - W5x00 INT/IRQ to GPIO 3<br>

Now connect via Wi-Fi and change the network hardware to "Generic W5500".<br>

If Ethernet hardware isn't detected or initialised properly after changing the network device, Wi-Fi will be used as a fallback.<br>
<br>
Note: LAN8720 modules are only supported on the ESP32, ESP32-P4 and ESP32-Solo1, not on the ESP32-S3, ESP32-C3, ESP32-C5 or ESP-C6<br>

## Debugging crashes

If you are running a pre-compiled version of NukiHub (latest release, beta or nightly) you can use https://technyon.github.io/nuki_hub/stacktrace/ to debug crashes.
You will need to collect a stack trace of the crash while connected to the serial logger (over USB).

A stack trace usually starts with `Guru Mediation Error` and ends with `ELF file SHA256: ......`
Copy the entire stack trace to the box in our online decoder and select the correct binary you are using.
Click Run and check the output to find in which function the crash occurs and consider creating an issue on GitHub.

## FAQ / Troubleshooting

### Pairing with the lock (or opener) doesn't work

First, make sure the firmware version of the Nuki device is up-to-date, older versions have issues pairing.<br>

Check that pairing is allowed. In the Nuki smartphone app, go to "Settings" --> "Features & Configuration" --> "Button & LED" and make sure "Bluetooh Pairing" is enabled.<br>
Next press the button for several seconds untill the LED light remains lit.

On some devices, such as the [M5Stack PoESP32 Unit](https://docs.m5stack.com/en/unit/poesp32) the Bluetooth reception is very poor (range less than one meter).<br>
The reason is that some modules do not have an antenna on the PCB, but only an IPEX connector.<br>
By retrofitting an external SMA antenna (IPEX, or other names U.FL, IPAX, IPX, AMC, MHF, UMCC), Bluetooth/Wi-Fi works over several meters.<br>

### In Home Assistant, the lock/opener is shown as unavailable

Make sure you are using at least version 2023.8.0 of Home Assistant.<br>
The Home Assistant developers have made changes to MQTT auto discovery which break support for older version and Nuki Hub has adopted these changes.<br>
This unfortunately means that older versions of Home Assistant are not supported by the Nuki Hub discovery implementation anymore.

### Purging Home Assistant discovery MQTT topics

- Download the latest version of MQTT Explorer for your system from https://mqtt-explorer.com/
- Run MQTT Explorer and connect to your MQTT broker
- Enter `ids":["nuki_` in the MQTT explorer search bar
- Click on the root `homeassistant` topic
- Press delete on your keyboard
- Click Yes to confirm deletion
- Reboot all Nuki Hub devices connected to the MQTT broker to recreate the correct topics for Home Assistant

### Nuki Hub in bridge mode doesn't work when Thread or Wi-Fi on a Nuki Smartlock (3.0 Pro / 4.0 / 4.0 Pro) is turned on.

According to Nuki this is by design and part of the specification of Wi-Fi/Thread enabled locks.<br>
You can use either the built-in Wi-Fi/Thread or a Bridge (which Nuki Hub registers as), using both at the same time is not supported.<br>
Or you can use Nuki Hub in Hybrid mode using Wi-Fi or Thread, see [hybrid mode](/HYBRID.md)<br>

### Certain functionality doesn't work (e. g. changing configuration, setting keypad codes)

Some functionality is restricted by the Lock (or Opener) firmware and is only accessible when the PIN is provided.<br>
When first setting up the lock (or opener), you have to set a PIN in the Nuki smartphone app.<br>
Navigate to the Nuki Hub Credentials page, enter this PIN, click save and reboot.<br>
Check the main page of the configurator to see if the entered PIN is valid.

Also make sure that the you have enabled the specific functionality on the "Access Level Configuration" page of Nuki Hub.

### Authorization data isn't published

See the previous point, this functionality needs the correct PIN to be configured.

### Using Home Assistant, it's only possible to lock or unlock the door, but not to unlatch it

Make sure the "Unlatch" option is checked under "Access Level Configuration".<br>
<br>
Unlatching can be triggered using the lock.open service.<br>
<br>
Alternatively an "Unlatch" button is exposed through Home Assistant discovery.<br>
This button is disabled by default, but can be enabled in the Home Assistant UI.

### When controlling two locks (or openers) connected to two ESPs, both devices react to the same command. When using Home Asistant, the same status is display for both locks.

When using multiple Nuki devices, different paths for each device have to be configured.<br>
Navigate to "MQTT Configuration" and change the "MQTT Nuki Hub Path" under "Basic MQTT Configuration" for at least one of the devices.<br>

### The Nuki battery is draining quickly.

This often is a result of enabling "Nuki Bridge is running alongside Nuki Hub" when not using [Hybrid mode](/HYBRID.md) (Official MQTT / Nuki Hub co-existance).<br>
Doing so will cause Nuki Hub to constantly query the lock and as such cause excessive battery drain.<br>
To prevent this behaviour, unpair Nuki Hub, disable "Nuki Bridge is running alongside Nuki Hub", and re-pair.<br>
<br>
<b>Never enable "Nuki Bridge is running alongside Nuki Hub" unless you intend to use a Nuki Bridge in addition to Nuki Hub or you are using Hybrid mode!</b>

## Building from source
<b>Docker (Preferred)</b><br>
See the [README](/Docker/README.md) in the Docker directory for instructions on building using Docker.<br>
<br>
<b>Platform IO, instructions for Debian-based Linux distro (e.g. Ubuntu)</b><br>
```console
apt-get update
apt-get install -y git python3 pip make
python3 -m venv .venv
source .venv/bin/activate

git clone https://github.com/technyon/nuki_hub --recursive
cd nuki_hub

# install tools platformio and esptool
make deps

# build all binary boards
make updater
make release
```

## Disclaimer

This is third party software for Nuki devices.<br>
This project or any of it's authors are not associated with Nuki Home Solutions GmbH.<br>
Please refer for official products and support to the Nuki official website:<br>
https://nuki.io/<br>
<br>
For further license details, check the included LICENSE file.
