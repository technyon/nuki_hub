## About

***The scope of Nuki Hub is to have an efficient way to integrate Nuki devices in a local Home Automation platform.***

The Nuki Hub software runs on a ESP32 module and acts as a bridge between Nuki devices and a Home Automation platform.<br>
<br>
It communicates with a Nuki Lock and/or Opener through Bluetooth (BLE) and uses MQTT to integrate with other systems.<br>
<br>
It exposes the lock state (and much more) through MQTT and allows executing commands like locking and unlocking through MQTT.<br>

***Nuki Hub does not integrate with the Nuki mobile app, it can't register itself as a bridge in the official Nuki mobile app.***

## Supported devices

<b>Supported ESP32 devices:</b>
- Any dual-core ESP32, except the ESP32-S3

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
As an alternative to WIFI (which is available on any supported ESP32), the following ESP32 modules with wired ethernet are supported:
- [Olimex ESP32-POE](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE/open-source-hardware)
- [Olimex ESP32-POE-ISO](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware)
- [WT32-ETH01](http://en.wireless-tag.com/product-item-2.html)
- [M5Stack Atom POE](https://docs.m5stack.com/en/atom/atom_poe)
- [M5Stack PoESP32 Unit](https://docs.m5stack.com/en/unit/poesp32)
- [LilyGO-T-ETH-POE](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-POE)

<b>Note for users upgrading from Nuki Hub 8.21 or lower:</b><br>
Please go to "MQTT and Network Configuration" and select "WIFI only" as the network device (unless you use other network hardware).

## Installation

Flash the firmware to an ESP32.<br>
The easiest way to install is to use the web installer using a compatible browser like Chrome/Opera/Edge:<br>
https://technyon.github.io/nuki_hub/<br>
<br>
Alternatively download the latest release from https://github.com/technyon/nuki_hub/releases<br>
Unpack the 7z archive and read the included readme.txt for installation instructions for either "Espressif Flash Download Tools" or "esptool".

## Initial setup (Network and MQTT)

Power up the ESP32 and a new WIFI access point named "ESP32_(8 CHARACTER ALPHANUMERIC)" should appear.<br>
Connect to this access point and in a browser navigate to "http://192.168.4.1".<br>
Use the web interface to connect the ESP to your WIFI network.<br>
<br>
After configuring the WIFI, the ESP should automatically connect to your network.<br>
<br>
To configure the connection to the MQTT broker first connect your client device to the same WIFI network the ESP32 is connected to.<br>
In a browser navigate to the IP address assigned to the ESP32 via DHCP (often found in the web interface of your internet router).<br>
Next click on "Edit" below "MQTT and Network Configuration" and enter the address and port (usually 1883) of your MQTT broker and a username and a password if required.<br>
<br>
The firmware supports SSL encryption for MQTT, however most people and especially home users don't use this.<br>
In that case leave all fields starting with "MQTT SSL" blank.<br>
Otherwise see the "MQTT Encryption" section of this README.

## Pairing with a Nuki Lock or Opener

Enable pairing mode on the Nuki Lock or Opener (press the button on the Nuki device for a few seconds) and power on the ESP32.<br>
Pairing should be automatic. When pairing is successful, the web interface should show "Paired: Yes" (reload page in browser).<br>
MQTT nodes like lock state and battery level should now reflect the reported values from the lock.<br>
<br>
Note: It is possible to run Nuki Hub alongside a Nuki Bridge.<br>
This is not recommended and can lead to either device missing updates.<br>
Enable "Register as app" before pairing to allow this.<br>
Otherwise the Bridge will be unregistered when pairing the Nuki Hub.

## Support

If you haven't ordered your Nuki product yet, you can support me by using my referrer code when placing your order:<br>
REFVSU6HN9HWK<br>
This will also give you a 10% discount on your order.<br>
<br>
This project is free to use for everyone. However if you feel like donating, you can buy me a coffee at ko-fi.com:<br>
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/C0C1IDUBV)

## Exposed MQTT Topics

### Lock

- lock/action: Allows to execute lock actions. After receiving the action, the value is set to "ack". Possible actions: unlock, lock, unlatch, lockNgo, lockNgoUnlatch, fullLock, fobAction1, fobAction2, fobAction3
- lock/state: Reports the current lock state as a string. Possible values are: uncalibrated, locked, unlocked, unlatched, unlockedLnga, unlatching, bootRun, motorBlocked
- lock/trigger: The trigger of the last action: autoLock, automatic, button, manual, system
- lock/completionStatus: Status of the last action as reported by Nuki lock (needs bluetooth connection): success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown
- lock/authorizationId: If enabled in the web interface, this node returns the authorization id of the last lock action
- lock/authorizationName: If enabled in the web interface, this node returns the authorization name of the last lock action
- lock/commandResult: Result of the last action as reported by Nuki library: success, failed, timeOut, working, notPaired, error, undefined
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating
- query/lockstate: Set to 1 to trigger query lockstage. Auto-resets to 0.
- query/config: Set to 1 to trigger query config. Auto-resets to 0.
- query/keypad: Set to 1 to trigger query keypad. Auto-resets to 0.
<br><br>
- battery/level: Battery level in percent
- battery/critical: 1 if battery level is critical, otherwise 0
- battery/charging: 1 if charging, otherwise 0
- battery/voltage: Current Battery voltage (V)
- battery/drain: The drain of the last lock action in Milliwattseconds (mWs)
- battery/maxTurnCurrent: The highest current of the turn motor during the last lock action (A)
  <br><br>
- configuration/autoLock: enable or disable autoLock (0 = disabled; 1 = enabled). Maps to "Auto lock enabled" in the bluetooth API.
- configuration/autoUnlock: enable or disable autoLock in general (0 = disabled; 1 = enabled). Maps to "Auto unlock disabled" in the bluetooth API.
- configuration/buttonEnabled: enable or disable the button on the lock (0 = disabled; 1 = enabled)
- configuration/ledBrightness: Set the brightness of the LED on the lock (0=min; 5=max)
- configuration/ledEnabled: enable or disable the LED on the lock (0 = disabled; 1 = enabled)
- configuration/singleLock: configures wether to single- or double-lock the door (1 = single; 2 = double)

### Opener

- lock/action: Allows to execute lock actions. After receiving the action, the value is set to "ack". Possible actions: activateRTO, deactivateRTO, electricStrikeActuation, activateCM, deactivateCM, fobAction1, fobAction2, fobAction3
- lock/state: Reports the current lock state as a string. Possible values are: locked, RTOactive, ring, open, opening, uncalibrated
- lock/trigger: The trigger of the last action: autoLock, automatic, button, manual, system
- lock/completionStatus: Status of the last action as reported by Nuki lock (needs bluetooth connection): success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown
- lock/authorizationId: If enabled in the web interface, this node returns the authorization id of the last lock action
- lock/authorizationName: If enabled in the web interface, this node returns the authorization name of the last lock action
- lock/commandResult: Result of the last action as reported by Nuki library: success, failed, timeOut, working, notPaired, error, undefined
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating
- query/lockstate: Set to 1 to trigger query lockstage. Auto-resets to 0.
- query/config: Set to 1 to trigger query config. Auto-resets to 0.
- query/keypad: Set to 1 to trigger query keypad. Auto-resets to 0.
  <br><br>
- battery/voltage: Reports the current battery voltage in Volts.
- battery/critical: 1 if battery level is critical, otherwise 0
  <br><br>
- configuration/buttonEnabled: enable or disable the button on the lock (0 = disabled; 1 = enabled)
- configuration/ledEnabled: enable or disable the LED on the lock (0 = disabled; 1 = enabled)
- configuration/soundLevel: configures the volume of sounds the opener plays back (0 = min; 255 = max)

### Misc
- presence/devices: List of detected bluetooth devices as CSV. Can be used for presence detection

## Over-the-air Update (OTA)
After the initial installation of the Nuki Hub firmware via serial connection, further updates can be deployed via OTA update from a browser.<br>
In the configuration portal, scroll down to "Firmware update" and click "Open".<br>
Then Click "Browse" and select the new "nuki_hub.bin" file and select "Upload file".<br>
After about a minute the new firmware should be installed.

## MQTT Encryption (optional; WiFi and LAN8720 only)

The communication via MQTT can be SSL encrypted.<br>
To enable SSL encryption, supply the necessary information in the MQTT Configuration page.<br>
<br>
The following configurations are supported:<br>
CA, CERT and KEY are empty -> No encryption<br>
CA is filled but CERT and KEY are empty -> Encrypted MQTT<br>
CA, CERT and KEY are filled -> Encrypted MQTT with client vaildation

## Home Assistant Discovery (optional)

Home Assistant can be setup manually using the [MQTT Lock integration](https://www.home-assistant.io/integrations/lock.mqtt/).

For a simpler integration, this software supports [MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/). To enable autodiscovery, supply the discovery topic that is configured in your Home Assistant instance (default is "homeassistant", that is the default topic HA looks for, unless you changed it also in HA) in the MQTT Configuration page. Once enabled, Smartlock and/or Opener should automatically appear on Home Assistant.
NOTE: _this is the root HA autodiscovery topic, don't put subtopics under that_

The following mapping between Home Assistant services and Nuki commands is setup when enabling autodiscovery:
|             | Smartlock | Opener                    |
|-------------|-----------|---------------------------|
| lock.lock   | Lock      | Disable Ring To Open               |
| lock.unlock | Unlock    | Enable Ring To Open                |
| lock.open   | Unlatch   | Electric Strike Actuation |

NOTE: MQTT Discovery uses retained MQTT messages to store devices configurations. In order to avoid orphan configurations on your broker please disable autodiscovery first if you no longer want to use this SW. Retained messages are automatically cleared when unpairing and when changing/disabling autodiscovery topic in MQTT Configuration page.

## Keypad control (optional)

If a keypad is connected to the lock, keypad codes can be added, updated and removed.
This has to enabled first in the configuration portal. Check "Enable keypad control via MQTT" and save the configuration.
After enabling keypad control, information about codes is published under "keypad/code_x", x starting from 0 up the number of configured codes.
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

If you prefer to connect to the MQTT Broker via Ethernet instead of WiFi, you either use one of the supported ESP32 modules (see about section above),
or wire a seperate Wiznet W5x00 Module (W5100, W5200, W5500 are supported). To use a supported module, flash the firmware, connect via WIFI and
select the correct network hardware in the MQTT and network settings section.

To wire an external W5x00 module to the ESP, use this wiring scheme:

- Connect W5x00 to ESP32 SPI0:<br>
  - W5x00 SCK to GPIO18<br>
  - W5x00 MISO to GPIO19<br>
  - W5x00 MOSI to GPIO23<br>
  - W5x00 CS/SS to GPIO5<br>
- Optionally connect:<br>
  - W5x00 reset to GPIO33<br>

Now connect via WIFI and change the network hardware to "Generic W5500".<br>
If the W5500 hwardware isn't detected, WIFI is used as a fallback.<br>
Note: Encrypted MQTT is only available for WIFI and LAN8720 modules, W5x00 modules don't support encryption<br>
(that leaves Olimex, WT32-ETH01 and M5Stack PoESP32 Unit if encryption is desired).<br>
<br>
If encryption is needed, Olimex is the easiest option, since it has USB for flashing onboard.

## Troubleshooting

### Random WiFi disconnects
Unfortunately the ESP32 has problems with some access points and reconnecting fails.<br>
As a workaround you can navigate to "MQTT and Network Configuration" and enable "Restart on disconnect".<br>
This will reboot the ESP as soon as it gets disconnected from WiFi.<br>
Also, this reduces the config portal timeout to three minutes to prevent the ESP being stuck in config mode in case an access point is offline temporarily.<br>
If this still doesn't fix the disconnects and the ESP becomes unreachable, the "Restart timer" option can be used as a last resort.<br>
It will restart the ESP after a configured amount of time.

### Pairing with the lock (or opener) doesn't work
First, make sure the firmware version of the Nuki device is up-to-date, older versions have issues pairing.<br>
Next, try erasing the ESP32 flash and then (re-)flash the firmware.<br>
To erase the flash, use the espressif download tool and click the "Erase" button.<br>
Afterwards flash the firmware as described in the readme within the 7z file.<br>
<br>
Also, there are reports that ESP32 "DEVKIT1" module don't work and pairing is not possible. The reason is unknown, but if you use such a module, try a different one.<br>
<br>
Reported as working are:<br>
[M5Stack ATOM Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit)<br>
ESP32-WROOM-32D (DEVKIT V4)<br>
ESP32-WROOM-32E<br>
<br>
For more information check the related issue:<br>
https://github.com/technyon/nuki_hub/issues/39

Also, check that pairing is allowed. In the Nuki smartphone app, go to "Settings" --> "Features & Configuration" --> "Button & LED" and make sure "Bluetooh Pairing" is enabled.

### In Home Assistant, the lock/opener is shown as unavailable

Make sure you are using at least version 2023.8.0 of Home Assistant.<br>
The Home Assistant developers have made changes to the MQTT auto discovery which break support for older version and Nuki Hub has adopted these changes.<br>
This unfortunately means that older versions of Home Assistant are not supported by the Nuki Hub discovery implemenation anymore.

## FAQ

### Nuki Hub doesn't work when WIFI on a Nuki Smartlock Pro (3.0 / 4.0) is turned on.

This is by design and according to Nuki part of the specification of the Pro lock.<br>
You can use either the built-in WIFI or a Bridge (which Nuki Hub registers as).<br>
Using both at the same time is not supported.

### Certain functionality doesn't work (e. g. changing configuration, setting keypad codes)
Some functionality is restricted by the Lock (or Opener) firmware and is only accessible when the PIN is provided.<br>
When setting up the lock (or opener), you have to set a PIN in the Nuki smartphone app.<br>
Navigate to the credentials page, enter this PIN and click save.

### Authorization data isn't published
See previous point, this functionality needs the correct PIN to be configured.

### Using Home Assistant, it's only possible to lock or unlock the door, but not to unlatch it
Make sure "Access level" under "Advanced Nuki Configuration" is set to "Full".<br>
<br>
Unlatching can be triggered using the lock.open service.<br>
<br>
Alternatively a "Unlatch" button is exposed through Home Assistant discovery.<br>
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
<b>VMWare image</b><br>
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