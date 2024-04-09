## About

***The scope of Nuki Hub is to have an efficient way to integrate Nuki devices in a local Home Automation platform.***

The Nuki Hub software runs on a ESP32 module and acts as a bridge between Nuki devices and a Home Automation platform.<br>
<br>
It communicates with a Nuki Lock and/or Opener through Bluetooth (BLE) and uses MQTT to integrate with other systems.<br>
<br>
It exposes the lock state (and much more) through MQTT and allows executing commands like locking and unlocking as well as changing the Nuki Lock/Opener configuration through MQTT.<br>

***Nuki Hub does not integrate with the Nuki mobile app, it can't register itself as a bridge in the official Nuki mobile app.***

Feel free to join us on Discord: https://discord.gg/feB9FnMY

## Supported devices

<b>Supported ESP32 devices:</b>
- All dual-core ESP32 models with WIFI and BLE which are supported by Arduino Core 2.0.14 should work, but builds are currently only provided for the ESP32 and not for the ESP32-S3 or ESP32-C3.
- The ESP32-S2 has no BLE and as such can't run Nuki Hub.
- The ESP32-C6 and ESP32-H2 are not supported by Arduino Core 2.0.14 as such can't run Nuki Hub (at this time).

<b>Supported Nuki devices:</b>
- Nuki Smart Lock 1.0
- Nuki Smart Lock 2.0
- Nuki Smart Lock 3.0
- Nuki Smart Lock 3.0 Pro (read FAQ below)
- Nuki Smart Lock 4.0
- Nuki Smart Lock 4.0 Pro (read FAQ below)
- Nuki Opener
- Nuki Keypad 1.0
- Nuki Keypad 2.0

<b>Supported Ethernet devices:</b><br>
As an alternative to Wi-Fi (which is available on any supported ESP32), the following ESP32 modules with wired ethernet are supported:
- [Olimex ESP32-POE](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE/open-source-hardware)
- [Olimex ESP32-POE-ISO](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware)
- [WT32-ETH01](http://en.wireless-tag.com/product-item-2.html)
- [M5Stack Atom POE](https://docs.m5stack.com/en/atom/atom_poe)
- [M5Stack PoESP32 Unit](https://docs.m5stack.com/en/unit/poesp32)
- [LilyGO-T-ETH-POE](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-POE)

<b>Note for users upgrading from Nuki Hub 8.21 or lower:</b><br>
Please go to "MQTT and Network Configuration" and select "Wi-Fi only" as the network device (unless you use other network hardware).

## Installation

Flash the firmware to an ESP32. The easiest way to install is to use the web installer using a compatible browser like Chrome/Opera/Edge:<br>
https://technyon.github.io/nuki_hub/<br>
<br>
Alternatively download the latest release from https://github.com/technyon/nuki_hub/releases<br>
Unpack the 7z archive and read the included readme.txt for installation instructions for either "Espressif Flash Download Tools" or "esptool".

## Initial setup (Network and MQTT)

Power up the ESP32 and a new Wi-Fi access point named "ESP32_(8 CHARACTER ALPHANUMERIC)" should appear.<br>
Connect a client device to this access point and in a browser navigate to "http://192.168.4.1".<br>
Use the web interface to connect the ESP to your preferred Wi-Fi network.<br>
<br>
After configuring Wi-Fi, the ESP should automatically connect to your network.<br>
<br>
To configure the connection to the MQTT broker first connect your client device to the same Wi-Fi network the ESP32 is connected to.<br>
In a browser navigate to the IP address assigned to the ESP32 via DHCP (often found in the web interface of your internet router).<br><br>
Next click on "Edit" below "MQTT and Network Configuration" and enter the address and port (usually 1883) of your MQTT broker and a username and a password if required by your MQTT broker.<br>
<br>
The firmware supports SSL encryption for MQTT, however most people and especially home users don't use this.<br>
In that case leave all fields starting with "MQTT SSL" blank. Otherwise see the "[MQTT Encryption](#mqtt-encryption-optional-wi-fi-and-lan8720-only)" section of this README.

## Pairing with a Nuki Lock or Opener

Enable pairing mode on the Nuki Lock or Opener (press the button on the Nuki device for a few seconds) and power on the ESP32.<br>
Pairing should be automatic.<br>
<br>
When pairing is successful, the web interface should show "Paired: Yes" (it might be necessary to reload the page in your browser).<br>
MQTT nodes like lock state and battery level should now reflect the reported values from the lock.<br>
<br>
<b>Note: It is possible to run Nuki Hub alongside a Nuki Bridge. 
This is not recommended and will lead to excessive battery drain and can lead to either device missing updates.
Enable "Register as app" before pairing to allow this. Otherwise the Bridge will be unregistered when pairing the Nuki Hub.</b>

## Support

If you haven't ordered your Nuki product yet, you can support me by using my referrer code when placing your order:<br>
REFVSU6HN9HWK<br>
This will also give you a 10% discount on your order.<br>
<br>
This project is free to use for everyone. However if you feel like donating, you can buy me a coffee at ko-fi.com:<br>
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/C0C1IDUBV)

## Configuration

In a browser navigate to the IP address assigned to the ESP32.

### MQTT and Network Configuration 

#### Basic MQTT and Network Configuration

- Host name: Set the hostname for the Nuki Hub ESP
- MQTT Broker: Set to the IP address of the MQTT broker
- MQTT Broker port: Set to the Port of the MQTT broker (usually 1883)
- MQTT User: If using authentication on the MQTT broker set to a username with read/write rights on the MQTT broker, set to # to clear
- MQTT Password : If using authentication on the MQTT broker set to the password belonging to a username with read/write rights on the MQTT broker, set to # to clear

#### Advanced MQTT and Network Configuration

- Home Assistant discovery topic: Set to the Home Assistant auto discovery topic, leave empty to disable auto discovery. Usually "homeassistant" unless you manually changed this setting on the Home Assistant side.
- Home Assistant device configuration URL: When using Home Assistant discovery the link to the Nuki Hub Web Configuration will be published to Home Assistant. By default when this setting is left empty this will link to the current IP of the Nuki Hub. When using a reverse proxy to access the Web Configuration you can set a custom URL here.
- Set Nuki Opener Lock/Unlock action in Home Assistant to Continuous mode (Opener only): By default the lock entity in Home Assistant will enable Ring-to-Open (RTO) when unlocking and disable RTO when locking. By enabling this setting this behaviour will change and now unlocking will enable Continuous Mode and locking will disable Continuous Mode, for more information see the "[Home Assistant Discovery](#home-assistant-discovery-optional)" section of this README. 
- MQTT SSL CA Certificate: Optionally set to the CA SSL certificate of the MQTT broker, see the "[MQTT Encryption](#mqtt-encryption-optional-wi-fi-and-lan8720-only)" section of this README.
- MQTT SSL Client Certificate: Optionally set to the Client SSL certificate of the MQTT broker, see the "[MQTT Encryption](#mqtt-encryption-optional-wi-fi-and-lan8720-only)" section of this README.
- MQTT SSL Client Key: Optionally set to the Client SSL key of the MQTT broker, see the "[MQTT Encryption](#mqtt-encryption-optional-wi-fi-and-lan8720-only)" section of this README.
- Network hardware: "Wi-Fi only" by default, set to one of the specified ethernet modules if available, see the "Supported Ethernet devices" and "[Connecting via Ethernet](#connecting-via-ethernet-optional)" section of this README.
- Disable fallback to Wi-Fi / Wi-Fi config portal: By default the Nuki Hub will fallback to Wi-Fi and open the Wi-Fi configuration portal when the network connection fails. Enable this setting to disable this fallback.
- RSSI Publish interval: Set to a positive integer to set the amount of seconds between updates to the maintenance/wifiRssi MQTT topic with the current Wi-Fi RSSI, set to -1 to disable, default 60. 
- Network Timeout until restart: Set to a positive integer to restart the Nuki Hub after the set amount of seconds has passed without an active connection to the MQTT broker, set to -1 to disable, default 60.
- Restart on disconnect: Enable to restart the Nuki Hub after 60 seconds without a connection to a network.
- Enable MQTT logging: Enable to fill the maintenance/log MQTT topic with debug log information.
- Check for Firmware Updates every 24h: Enable to allow the Nuki Hub to check the latest release of the Nuki Hub firmware on boot and every 24 hours. Requires the Nuki Hub to be able to connect to github.com. The latest version will be published to MQTT and will be visible on the main page of the Web Configurator.

#### IP Address assignment

- Enable DHCP: Enable to use DHCP for obtaining an IP address, disable to use the static IP settings below
- Static IP address: When DHCP is disabled set to the preferred static IP address for the Nuki Hub to use
- Subnet: When DHCP is disabled set to the preferred subnet for the Nuki Hub to use
- Default gateway: When DHCP is disabled set to the preferred gateway IP address for the Nuki Hub to use
- DNS Server: When DHCP is disabled set to the preferred DNS server IP address for the Nuki Hub to use

### Nuki Configuration

#### Basic Nuki Configuration

- Nuki Smartlock enabled: Enable if you want Nuki Hub to connect to a Nuki Lock (1.0-4.0)
- MQTT Nuki Smartlock Path (Lock only): Set to the preferred MQTT root topic for the Nuki Lock, defaults to "nuki". Make sure this topic is not the same as the setting for the opener and is unique when using multiple Nuki Hub devices (when using multiple Nuki Locks)
- Nuki Opener enabled: Enable if you want Nuki Hub to connect to a Nuki Opener
- MQTT Nuki Opener Path (Opener only): Set to the preferred MQTT root topic for the Nuki Opener, defaults to "nukiopener". Make sure this topic is not the same as the setting for the lock and is unique when using multiple Nuki Hub devices (when using multiple Nuki Openers)

#### Advanced Nuki Configuration

- Query interval lock state: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current lock state, default 1800.
- Query interval configuration: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current configuration, default 3600.
- Query interval battery: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current battery state, default 1800.
- Query interval keypad (Only available when a Keypad is detected): Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current keypad state, default 1800.
- Number of retries if command failed: Set to a positive integer to define the amount of times the Nuki Hub retries sending commands to the Nuki Lock or Opener when commands are not acknowledged by the device, default 3.
- Delay between retries: Set to the amount of milliseconds the Nuki Hub waits between resending not acknowledged commands, default 100.
- Nuki Bridge is running alongside Nuki Hub: Enable to allow Nuki Hub to co-exist with a Nuki Bridge by registering Nuki Hub as an (smartphone) app instead of a bridge. Changing this setting will require re-pairing. Enabling this setting is strongly discouraged as described in the "[Pairing with a Nuki Lock or Opener](#pairing-with-a-nuki-lock-or-opener)" section of this README
- Presence detection timeout: Set to a positive integer to set the amount of seconds between updates to the presence/devices MQTT topic with the list of detected bluetooth devices, set to -1 to disable presence detection, default 60.
- Restart if bluetooth beacons not received: Set to a positive integer to restart the Nuki Hub after the set amount of seconds has passed without receiving a bluetooth beacon from the Nuki device, set to -1 to disable, default 60. Because the bluetooth stack of the ESP32 can silently fail it is not recommended to disable this setting.

### Access Level Configuration

#### Nuki General Access Control
- Publish keypad codes information (Only available when a Keypad is detected): Enable to publish information about keypad codes through MQTT, see the "[Keypad control](#keypad-control-optional)" section of this README
- Add, modify and delete keypad codes (Only available when a Keypad is detected): Enable to allow configuration of keypad codes through MQTT, see the "[Keypad control](#keypad-control-optional)" section of this README
- Publish auth data: Enable to publish authorization data to the MQTT topic lock/log. Requires the Nuki security code / PIN to be set, see "Nuki Lock PIN / Nuki Opener PIN" below.

#### Nuki Lock/Opener Access Control
- Enable or disable executing each available lock action for the Nuki Lock and Nuki Opener through MQTT. Note: GPIO control is not restricted through this setting.

#### Nuki Lock/Opener Config Control
- Enable or disable changing each available configuration setting for the Nuki Lock and Nuki Opener through MQTT.
- NOTE: Changing configuration settings requires the Nuki security code / PIN to be set, see "Nuki Lock PIN / Nuki Opener PIN" below.

### Credentials

#### Credentials

- User: Pick a username to enable HTTP Basic authentication for the Web Configuration, Set to "#" to disable authentication. 
- Password/Retype password: Pick a password to enable HTTP Basic authentication for the Web Configuration.

#### Nuki Lock PIN / Nuki Opener PIN

- PIN Code: Fill with the Nuki Security Code of the Nuki Lock and/or Nuki Opener. Required for functions that require the security code to be sent to the lock/opener such as setting lock permissions/adding keypad codes, viewing the activity log or changing the Nuki device configuration. Set to "#" to remove the security code from the Nuki Hub configuration.

#### Unpair Nuki Lock / Unpair Nuki Opener

- Type [4 DIGIT CODE] to confirm unpair: Set to the shown randomly generated code to unpair the Nuki Lock or Opener from the Nuki Hub.

### GPIO Configuration

- Gpio [2-33]: See the "GPIO lock control" section of this README.

## Exposed MQTT Topics

### Lock

- lock/action: Allows to execute lock actions. After receiving the action, the value is set to "ack". Possible actions: unlock, lock, unlatch, lockNgo, lockNgoUnlatch, fullLock, fobAction1, fobAction2, fobAction3.
- lock/state: Reports the current lock state as a string. Possible values are: uncalibrated, locked, unlocked, unlatched, unlockedLnga, unlatching, bootRun, motorBlocked.
- lock/hastate: Reports the current lock state as a string, specifically for use by Home Assistant. Possible values are: locking, locked, unlocking, unlocked, jammed.
- lock/json: Reports the lock state, last action trigger, last lock action, lock completion status, door sensor state, auth ID and auth name as JSON data.
- lock/binaryState: Reports the current lock state as a string, mostly for use by Home Assistant. Possible values are: locked, unlocked.
- lock/trigger: The trigger of the last action: autoLock, automatic, button, manual, system.
- lock/lastLockAction: Reports the last lock action as a string. Possible values are: Unlock, Lock, Unlatch, LockNgo, LockNgoUnlatch, FullLock, FobAction1, FobAction2, FobAction3, Unknown.
- lock/log: If "Publish auth data" is enabled in the web interface, this topic will be filled with the log of authorization data.
- lock/completionStatus: Status of the last action as reported by Nuki Lock: success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown.
- lock/authorizationId: If enabled in the web interface, this node returns the authorization id of the last lock action.
- lock/authorizationName: If enabled in the web interface, this node returns the authorization name of the last lock action.
- lock/commandResult: Result of the last action as reported by Nuki library: success, failed, timeOut, working, notPaired, error, undefined.
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating.
- lock/rssi: The signal strenght of the Nuki Lock as measured by the ESP32 and expressed by the RSSI Value in dBm.
- lock/address: The BLE address of the Nuki Lock.
- lock/retry: Reports the current number of retries for the current command. 0 when command is succesfull, "failed" if the number of retries is greater than the maximum configured number of retries.

### Opener

- lock/action: Allows to execute lock actions. After receiving the action, the value is set to "ack". Possible actions: activateRTO, deactivateRTO, electricStrikeActuation, activateCM, deactivateCM, fobAction1, fobAction2, fobAction3.
- lock/state: Reports the current lock state as a string. Possible values are: locked, RTOactive, open, opening, uncalibrated.
- lock/hastate: Reports the current lock state as a string, specifically for use by Home Assistant. Possible values are: locking, locked, unlocking, unlocked, jammed.
- lock/json: Reports the lock state, last action trigger, last lock action, lock completion status, door sensor state, auth ID and auth name as JSON data.
- lock/binaryState: Reports the current lock state as a string, mostly for use by Home Assistant. Possible values are: locked, unlocked.
- lock/continuousMode: Enable or disable continuous mode on the opener (0 = disabled; 1 = enabled).
- lock/ring: The string "ring" is published to this topic when a doorbell ring is detected while RTO or continuous mode is active or "ringlocked" when both are inactive.
- lock/binaryRing: The string "ring" is published to this topic when a doorbell ring is detected, the state will revert to "standby" after 2 seconds.
- lock/trigger: The trigger of the last action: autoLock, automatic, button, manual, system.
- lock/lastLockAction: Reports the last lock action as a string. Possible values are: ActivateRTO, DeactivateRTO, ElectricStrikeActuation, ActivateCM, DeactivateCM, FobAction1, FobAction2, FobAction3, Unknown.
- lock/log: If "Publish auth data" is enabled in the web interface, this topic will be filled with the log of authorization data.
- lock/completionStatus: Status of the last action as reported by Nuki Opener: success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown.
- lock/authorizationId: If enabled in the web interface, this topic is set to the authorization id of the last lock action.
- lock/authorizationName: If enabled in the web interface, this topic is set to the authorization name of the last lock action.
- lock/commandResult: Result of the last action as reported by Nuki library: success, failed, timeOut, working, notPaired, error, undefined.
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating.
- lock/rssi: The bluetooth signal strength of the Nuki Lock as measured by the ESP32 and expressed by the RSSI Value in dBm.
- lock/address: The BLE address of the Nuki Lock.
- lock/retry: Reports the current number of retries for the current command. 0 when command is succesfull, "failed" if the number of retries is greater than the maximum configured number of retries.

### Configuration
- configuration/buttonEnabled: 1 if the Nuki Lock/Opener button is enabled, otherwise 0.
- configuration/ledEnabled: 1 if the Nuki Lock/Opener LED is enabled, otherwise 0.
- configuration/ledBrightness: Set to the brightness of the LED on the Nuki Lock (0=min; 5=max) (Lock only).
- configuration/singleLock: 0 if the Nuki Lock is set to double-lock the door, otherwise 1 (= single-lock) (Lock only).
- configuration/autoLock:  1 if the Nuki Lock is set to Auto Lock, otherwise 0 (Lock only).
- configuration/autoUnlock: 1 if the Nuki Lock is set to Auto Unlock, otherwise 0 (Lock only).
- configuration/soundLevel: Set to the volume for sounds the Nuki Opener plays (0 = min; 255 = max) (Opener only).
- configuration/action: Allows changing configuration settings of the Nuki Lock/Opener using a JSON formatted value. After receiving the action, the value is set to "--". See the "[Changing Nuki Lock/Opener Configuration](#changing-nuki-lockopener-configuration)" section of this README for possible actions/values
- configuration/commandResult: Result of the last configuration change action as JSON data. See the "[Changing Nuki Lock/Opener Configuration](#changing-nuki-lockopener-configuration)" section of this README for possible values
- configuration/basicJson: The current basic configuration of the Nuki Lock/Opener as JSON data. See [Nuki Smart Lock API](https://developer.nuki.io/page/nuki-smart-lock-api-2/2/#heading--set-config) and [Nuki Opener API](https://developer.nuki.io/page/nuki-opener-api-1/7/#heading--set-config) for available settings. Please note: Longitude and Latitude of the Lock/Opener are not published to MQTT by design. These values can still be changed though.
- configuration/advancedJson: The current advanced configuration of the Nuki Lock/Opener as JSON data. See [Nuki Smart Lock API](https://developer.nuki.io/page/nuki-smart-lock-api-2/2/#heading--advanced-config) and [Nuki Opener API](https://developer.nuki.io/page/nuki-opener-api-1/7/#heading--advanced-config) for available settings.

### Query

- lock/query/lockstate: Set to 1 to trigger query lockstate. Auto-resets to 0.
- lock/query/config: Set to 1 to trigger query config. Auto-resets to 0.
- lock/query/keypad: Set to 1 to trigger query keypad. Auto-resets to 0.
- lock/query/battery: Set to 1 to trigger query battery. Auto-resets to 0.
- lock/query/lockstateCommandResult: Set to 1 to trigger query lockstate command result. Auto-resets to 0.

### Battery

- battery/level: Battery level in percent (Lock only).
- battery/critical: 1 if battery level is critical, otherwise 0.
- battery/charging: 1 if charging, otherwise 0 (Lock only).
- battery/voltage: Current Battery voltage (V).
- battery/drain: The drain of the last lock action in Milliwattseconds (mWs) (Lock only).
- battery/maxTurnCurrent: The highest current of the turn motor during the last lock action (A) (Lock only).
- battery/lockDistance: The total distance during the last lock action in centidegrees (Lock only).
- battery/keypadCritical: 1 if the battery level of a connected keypad is critical, otherwise 0.

### Keypad

- See the "[Keypad control](#keypad-control-optional)" section of this README.

### Info

- info/nukiHubVersion: Set to the current version number of the Nuki Hub firmware.
- info/firmwareVersion: Set to the current version number of the Nuki Lock/Opener firmware.
- info/hardwareVersion: Set to the hardware version number of the Nuki Lock/Opener.
- info/nukiHubIp: Set to the IP of the Nuki Hub.
- info/nukiHubLatest: Set to the latest available Nuki Hub firmware version number (if update checking is enabled in the settings).

### Maintanence

- maintenance/networkDevice: Set to the name of the network device that is used by the ESP. When using Wi-Fi will be set to "Built-in Wi-Fi". If using Ethernet will be set to "Wiznet W5500", "Olimex (LAN8720)", "WT32-ETH01", "M5STACK PoESP32 Unit" or "LilyGO T-ETH-POE".
- maintenance/reset: Set to 1 to trigger a reboot of the ESP. Auto-resets to 0.
- maintenance/mqttConnectionState: Last Will and Testament (LWT) topic. "online" when Nuki Hub is connected to the MQTT broker, "offline" if Nuki Hub is not connected to the MQTT broker.
- maintenance/uptime: Uptime in minutes.
- maintenance/wifiRssi: The Wi-Fi signal strength of the Wi-Fi Access Point as measured by the ESP32 and expressed by the RSSI Value in dBm.
- maintenance/log: If "Enable MQTT logging" is enabled in the web interface, this topic will be filled with debug log information.
- maintenance/freeHeap: Only available when debug mode is enabled. Set to the current size of free heap memory in bytes.
- maintenance/restartReasonNukiHub: Only available when debug mode is enabled. Set to the last reason Nuki Hub was restarted. See [RestartReason.h](/RestartReason.h) for possible values
- maintenance/restartReasonNukiEsp: Only available when debug mode is enabled. Set to the last reason the ESP was restarted. See [RestartReason.h](/RestartReason.h) for possible values
 
### Misc

- presence/devices: List of detected bluetooth devices as CSV. Can be used for presence detection.

## Changing Nuki Lock/Opener Configuration

To change Nuki Lock/Opener settings set the `configuration/action` topic to a JSON formatted value with any of the following settings. Multiple settings can be changed at once. See [Nuki Smart Lock API Basic Config](https://developer.nuki.io/page/nuki-smart-lock-api-2/2/#heading--set-config), [Nuki Smart Lock API Advanced Config](https://developer.nuki.io/page/nuki-smart-lock-api-2/2/#heading--advanced-config), [Nuki Opener API Basic Config](https://developer.nuki.io/page/nuki-opener-api-1/7/#heading--set-config) and [Nuki Opener API Advanced Config](https://developer.nuki.io/page/nuki-opener-api-1/7/#heading--advanced-config) for more information on the available settings.<br>

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
| timeZone                                | The current timezone or "None" if timezones are not supported                                    | "None" or one of the timezones from [Nuki Timezones](https://developer.nuki.io/page/nuki-smart-lock-api-2/2/#heading--list-of-timezone-ids)                                                                                                                                                                              |`{ "timeZone": "Europe/Berlin" }`   |
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
| autoLockTimeOut                         | Seconds until the smart lock relocks itself after it has been unlocked.                          | Integer between 30 and 180                                        |`{ "autoLockTimeOut": "60" }`       |
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
| timeZone                                | The current timezone or "None" if timezones are not supported                                    | "None" or one of the timezones from [Nuki Timezones](https://developer.nuki.io/page/nuki-smart-lock-api-2/2/#heading--list-of-timezone-ids)                                                                                                                                                                              |`{ "timeZone": "Europe/Berlin" }`   |
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

Example usage for changing multiple settings at once:<br>
- `{ "buttonEnabled": "1", "lockngoTimeout": "60", "automaticBatteryTypeDetection": "1" }`
- `{ "fobAction1": "Unlock", "fobAction2": "Intelligent", "nightModeImmediateLockOnStart": "1" }`

### Result of attempted configuration changes

The result of the last configuration change action will be published to the `configuration/commandResult` MQTT topic as JSON data.<br>
<br>
The JSON data will include a node called "general" and a node for every setting that Nuki Hub detected in the action.<br>
Possible values for the "general" node are "noPinSet", "invalidJson", "invalidConfig", "success" and "noChange".<br>
Possible values for the node per setting are "unchanged", "invalidValue", "valueTooLong", "accessDenied", "success", "failed", "timeOut", "working", "notPaired", "error" and "undefined"<br>
<br>
Example:
- `{"advertisingMode":"success","general":"success"}`

### Home Assistant discovery

If Home Assistant discovery is enabled (see the [Home  Assistant Discovery](#home-assistant-discovery-optional) section of this README) Nuki Hub will create entities for almost all of the above settings. These entities will be disabled by default in Home Assistant, but can be found in the MQTT devices section of the Home Assistant UI under the "Configuration" section of the Nuki Lock/Opener and enabled there.

## Over-the-air Update (OTA)

After the initial installation of the Nuki Hub firmware via serial connection, further updates can be deployed via OTA update from a browser.<br>
In the configuration portal, scroll down to "Firmware update" and click "Open".<br>
Then Click "Browse" and select the new "nuki_hub.bin" file and select "Upload file".<br>
After about a minute the new firmware should be installed afterwhich the ESP will reboot automatically.

## MQTT Encryption (optional; Wi-Fi and LAN8720 only)

The communication via MQTT can be SSL encrypted.<br>
To enable SSL encryption, supply the necessary information in the MQTT Configuration page.<br>
<br>
The following configurations are supported:<br>
CA, CERT and KEY are empty -> No encryption<br>
CA is filled but CERT and KEY are empty -> Encrypted MQTT<br>
CA, CERT and KEY are filled -> Encrypted MQTT with client vaildation

## Home Assistant Discovery (optional)

This software supports [MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/) for integrating Nuki Hub with Home Assistant.<br>
To enable autodiscovery, supply the discovery topic that is configured in your Home Assistant instance (If you have not changed this setting in Home Assistant the default is "homeassistant") in the MQTT Configuration page.<br>
Once enabled, the Nuki Lock and/or Opener and related entities should automatically appear in your Home Assistant MQTT devices.

The following mapping between Home Assistant services and Nuki commands is setup when enabling autodiscovery:
|             | Smartlock | Opener (default)          | Opener (alternative)      |
|-------------|-----------|---------------------------|---------------------------|
| lock.lock   | Lock      | Disable Ring To Open      | Disable Continuous Mode   |
| lock.unlock | Unlock    | Enable Ring To Open       | Enable Continuous Mode    |
| lock.open   | Unlatch   | Electric Strike Actuation | Electric Strike Actuation |

NOTE: MQTT Discovery uses retained MQTT messages to store devices configurations. In order to avoid orphan configurations on your broker please disable autodiscovery first if you no longer want to use this software. Retained messages are automatically cleared when unpairing and when changing/disabling autodiscovery topic in MQTT Configuration page.<br>
NOTE2: Home Assistant can be setup manually using the [MQTT Lock integration](https://www.home-assistant.io/integrations/lock.mqtt/), but this is not recommended

## Keypad control (optional)

If a keypad is connected to the lock, keypad codes can be added, updated and removed.
This has to enabled first in the configuration portal. Check "Enable keypad control via MQTT" and save the configuration.
After enabling keypad control, information about codes is published under "keypad/code_x", x starting from 0 up the number of configured codes. 
The same data is also published as JSON data to the "keypad/json" MQTT topic.
<br>
For security reasons, the code itself is not published. To modify keypad codes, a command
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

## GPIO lock control (optional)

The lock can be controlled via GPIO.<br>
<br>
To enable GPIO control, go the the "GPIO Configuration" page where each GPIO can be configured for a specific role:
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

Note: The old setting "Enable control via GPIO" is removed. If you had enabled this setting before upgrading to 8.22, the PINs are automatically configured to be compatible with the previously hard-coded PINs.

## Connecting via Ethernet (Optional)

If you prefer to connect to the MQTT Broker via Ethernet instead of Wi-Fi, you either use one of the supported ESP32 modules (see about section above),
or wire a seperate Wiznet W5x00 Module (W5100, W5200, W5500 are supported). To use a supported module, flash the firmware, connect via Wi-Fi and
select the correct network hardware in the "MQTT and Network Configuration" section.

To wire an external W5x00 module to the ESP, use this wiring scheme:

- Connect W5x00 to ESP32 SPI0:<br>
  - W5x00 SCK to GPIO18<br>
  - W5x00 MISO to GPIO19<br>
  - W5x00 MOSI to GPIO23<br>
  - W5x00 CS/SS to GPIO5<br>
- Optionally connect:<br>
  - W5x00 reset to GPIO33<br>

Now connect via Wi-Fi and change the network hardware to "Generic W5500".<br>
If the W5500 hwardware isn't detected, Wi-Fi is used as a fallback.<br>
<br>
Note: Encrypted MQTT is only available for Wi-Fi and LAN8720 modules, W5x00 modules don't support encryption<br>
(that leaves Olimex, WT32-ETH01 and M5Stack PoESP32 Unit if encryption is desired).<br>
<br>
If encryption is needed, Olimex is the easiest option, since it has USB for flashing onboard.

## Troubleshooting

### Random Wi-Fi disconnects

Unfortunately the ESP32 has problems with some access points and reconnecting fails.<br>
As a workaround you can navigate to "MQTT and Network Configuration" and enable "Restart on disconnect".<br>
This will reboot the ESP as soon as it gets disconnected from Wi-Fi.<br>
Also, this reduces the config portal timeout to three minutes to prevent the ESP being stuck in config mode in case an access point is offline temporarily.<br>
If this still doesn't fix the disconnects and the ESP becomes unreachable, the "Restart timer" option can be used as a last resort.<br>
It will restart the ESP after a configured amount of time.

### Pairing with the lock (or opener) doesn't work

First, make sure the firmware version of the Nuki device is up-to-date, older versions have issues pairing.<br>
Next, try erasing the ESP32 flash and then (re-)flash the firmware.<br>
To erase the flash, use the espressif download tool and click the "Erase" button.<br>
Afterwards flash the firmware as described in the readme within the 7z file.<br>
<br>

Also, there are reports that the ESP32 "DEVKIT1" module doesn't work and pairing is not possible. The reason is unknown, but if you use such a module, try a different one.<br>
<br>
Reported as working are:
- [M5Stack ATOM Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit)
- ESP32-WROOM-32D (DEVKIT V4)
- ESP32-WROOM-32E

For more information check the related issue: https://github.com/technyon/nuki_hub/issues/39

Also, check that pairing is allowed. In the Nuki smartphone app, go to "Settings" --> "Features & Configuration" --> "Button & LED" and make sure "Bluetooh Pairing" is enabled.

A note about the [M5Stack PoESP32 Unit](https://docs.m5stack.com/en/unit/poesp32). Here the initial Bluetooth reception is very poor (range less than one meter). The reason is that the module does not have an antenna on the PCB, but only an IPEX connector. By retrofitting an external SMA antenna (IPEX, or other names U.FL, IPAX, IPX, AMC, MHF, UMCC), bluetooth/Wifi works over several meters.

### In Home Assistant, the lock/opener is shown as unavailable

Make sure you are using at least version 2023.8.0 of Home Assistant.<br>
The Home Assistant developers have made changes to MQTT auto discovery which break support for older version and Nuki Hub has adopted these changes.<br>
This unfortunately means that older versions of Home Assistant are not supported by the Nuki Hub discovery implemenation anymore.

## FAQ

### Nuki Hub doesn't work when Wi-Fi on a Nuki Smartlock Pro (3.0 / 4.0) is turned on.

According to Nuki this is by design and part of the specification of the Pro lock.<br>
You can use either the built-in Wi-Fi or a Bridge (which Nuki Hub registers as).<br>
Using both at the same time is not supported.

### Certain functionality doesn't work (e. g. changing configuration, setting keypad codes)

Some functionality is restricted by the Lock (or Opener) firmware and is only accessible when the PIN is provided.<br>
When setting up the lock (or opener), you have to set a PIN in the Nuki smartphone app.<br>
Navigate to the Nuki Hub Credentials page, enter this PIN and click save.

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
Navigate to "Nuki Configuration" and change the "MQTT Nuki Smartlock Path" or "MQTT Nuki Opener Path" under "Basic Nuki Configuration" for at least one of the devices.<br>

### The Nuki battery is draining quickly.

This often is a result of enabling "Register as app".<br>
Doing so will cause Nuki Hub to constantly query the lock and as such cause excessive battery drain.<br>
To prevent this behaviour, unpair Nuki Hub, disable "Register as app", and re-pair.<br>
<br>
<b>Never enable "Register as app" unless you intend to use a Nuki Bridge in addition to Nuki Hub!</b>

## Building from source

<b>Docker (Preferred)</b><br>
See the [README](/Docker/README.md) in the Docker directory for instructions on building using Docker.<br>
<br>
<b>VMWare image (Not preferred, not using the latest Arduino ESP32 release at this time)</b><br>
A virtual machine (VMWare image) that is setup to compile Nuki Hub is available for download at:<br>
https://drive.google.com/file/d/1fUVYHDtxXAZOAfQ321iRNIwkqFwuDsBp/view?usp=share_link<br>
<br>
User and password for the VM are both "nuki" and "nuki".<br>
The source is checked out at ~/projects/nuki_hub, the cmake build directory is build.<br>
<br>
To compile, run the following commands:
```
cd projects/nuki_hub/build
ninja
```

To upload the image via serial port, run:
```
ninja upload-nuki_hub
```

The serial device is defined in ~/.bashrc (Environment variable SERIAL_PORT), which you'll eventually have to adopt to your device.

## Disclaimer

This is third party software for Nuki devices.<br>
This project or any of it's authors are not associated with Nuki Home Solutions GmbH.<br>
Please refer for official products and support to the Nuki official website:<br>
https://nuki.io/<br>
<br>
For further license details, check the included LICENSE file.
