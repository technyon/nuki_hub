## About

The NUKI Hub software runs on any ESP32 module and acts as a bridge between a NUKI Lock and a smart home system.
It connects to the NUKI device via bluetooth, a NUKI Bridge isn't required.

It exposes the lock state (and more) via MQTT, and allows to execute commands like locking and unlocking.
Optionally, a NUKI Opener is also supported.

Supported devices:<br>
NUKI Smart Lock 1.0<br>
NUKI Smart Lock 2.0<br>
NUKI Smart Lock 3.0<br>
NUKI Smart Lock 3.0 Pro<br>
NUKI Opener<br>
NUKI Keypad 1.0

## Installation

Flash the firmware to an ESP32. The easiest way to install is to use the web installer:<br>
https://technyon.github.io/nuki_hub/
<br>(Needs a web serial compatible browser like Chrome/Opera/Edge).

As an alternative, download a release:<br>
https://github.com/technyon/nuki_hub/releases
<br>Read the included readme.txt for installation instructions for either Espressif Flash Download Tools or esptool.

## Setup

The firmware uses the Wifi Manager to configure the WiFi network. Power up the ESP32, a new Access Point should appear. Connect to this access point and in a browser navigate to "192.168.4.1". Use the web interface configure your Wifi network.

After configuring the Wifi, the ESP should automatically connect to your network. Use the web interface to setup the MQTT broker; just navigate to the IP-Address assigned to the ESP32 via DHCP (often found in the web interface of the internet router).<br>
To configure MQTT, enter the adress of your MQTT broker and eventually a username and a password if required. The firmware supports SSL encryption for MQTT, however most people and especially home users don't use this. In that case leave all fields about "MQTT SSL" blank.

## Pairing

Just enable pairing mode on the NUKI lock (press button for a few seconds) and power on the ESP32. Pairing should be automatic. When pairing is successful, the web interface should show "Paired: Yes" (reload page in browser). MQTT nodes like lock state and battery level should now reflect the reported values from the lock.

Note: It is possible to run NUKI Hub alongside a NUKI Bridge. This is not recommended and can lead to either device missing updates. Enable "Register as app" before pairing to allow this. Otherwise the Bridge will be unregistered when pairing the NUKI Hub.

## Support

If you haven't ordered your NUKI product yet, you can support me by using my referrer code when placing your order:<br>
REFB4RWFVCD98<br>
This will also give you a 30€ discount for your order.

This project is free to use for everyone. However if you feel like donating, you can buy me a coffee at ko-fi.com:

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/C0C1IDUBV)

## MQTT Interface

### Lock

- lock/action: Allows to execute lock actions. After receiving the action, the value is set to "ack". Possible actions: unlock, lock, unlatch, lockNgo, lockNgoUnlatch, fullLock, fobAction1, fobAction2, fobAction3
- lock/state: Reports the current lock state as a string. Possible values are: uncalibrated, locked, unlocked, unlatched, unlockedLnga, unlatching, bootRun, motorBlocked
- lock/trigger: The trigger of the last action: autoLock, automatic, button, manual, system
- lock/completionStatus: Status of the last action as reported by NUKI lock (needs bluetooth connection): success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown
- lock/authorizationId: If enabled in the web interface, this node returns the authorization id of the last lock action
- lock/authorizationName: If enabled in the web interface, this node returns the authorization name of the last lock action
- lock/commandResult: Result of the last action as reported by NUKI library: success, failed, timeOut, working, notPaired, error, undefined
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating
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
- lock/completionStatus: Status of the last action as reported by NUKI lock (needs bluetooth connection): success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown
- lock/authorizationId: If enabled in the web interface, this node returns the authorization id of the last lock action
- lock/authorizationName: If enabled in the web interface, this node returns the authorization name of the last lock action
- lock/commandResult: Result of the last action as reported by NUKI library: success, failed, timeOut, working, notPaired, error, undefined
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating
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
After initially flashing the firmware via serial connection, further updates can be deployed via OTA update from a Web Browser. In the configuration portal, scroll down to "Firmware update" and click "Open". Then Click "Browse" and select the new "nuki_hub.bin" file and select "Upload file". After about a minute the new firmware should be installed.

## MQTT Encryption (optional; WiFi only)

The communication via MQTT can be SSL encrypted. To enable SSL encryption, supply the necessary information in the MQTT Configuration page.
The following configurations are supported:<br>

CA, CERT and KEY are empty -> No encryption<br>
CA is filled but CERT and KEY are empty -> Encrypted MQTT<br>
CA, CERT and KEY are filled -> Encrypted MQTT with client vaildation<br>

## Home Assistant Discovery (optional)

Home Assistant can be setup manually using the [MQTT Lock integration](https://www.home-assistant.io/integrations/lock.mqtt/).

For a simpler integration, this software supports [MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/). To enable autodiscovery, supply the discovery topic that is configured in your Home Assistant instance (typically "homeassistant") in the MQTT Configuration page. Once enabled, Smartlock and/or Opener should automatically appear on Home Assistant.

The following mapping between Home Assistant services and Nuki commands is setup when enabling autodiscovery:
|             | Smartlock | Opener                    |
|-------------|-----------|---------------------------|
| lock.lock   | Lock      | Disable Ring To Open               |
| lock.unlock | Unlock    | Enable Ring To Open                |
| lock.open   | Unlatch   | Electric Strike Actuation |

NOTE: MQTT Discovery uses retained MQTT messages to store devices configurations. In order to avoid orphan configurations on your broker please disable autodiscovery first if you no longer want to use this SW. Retained messages are automatically cleared when unpairing and when changing/disabling autodiscovery topic in MQTT Configuration page.

## Keypad control (optional)

If a keypad is connected to the lock, keypad codes can be added, updated and removed.
This has to enabled first in the configuration portal. Check "Enabled keypad control via MQTT" and save the configuration.
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

The lock can be controlled via GPIO. For security reasons, this has to be enabled in
the configuration portal (check "Enable control via GPIO" in the NUKI configuration
section). The Pins use pullup configuration, so they have to be connected to ground to
trigger the action.<br><br>
The Pin configuration is:<br>
32: Lock<br>
33: Unlock<br>
27: Unlatch

## Connecting via LAN (Optional)

If you prefer to connect to the MQTT Broker via LAN instead of WiFi, you can use a Wiznet W5x00 Module (W5100, W5200, W5500 are supported).
To connect, just wire the module and connect the LAN cable:

- Connect W5x00 to ESP32 SPI0:<br>
W5x00 SCK to GPIO18<br>
W5x00 MISO to GPIOGPIO19<br>
W5x00 MOSI to GPIO23<br>
W5x00 CS/SS to GPIO5
- Additionally connect:<br>
W5x00 reset to GPIO33
- Last but not least, on the ESP32 bridge GPIO26 and GND. This let's the firmware know that a LAN Module is connected

Wifi is now disabled, and the module doesn't boot into WifiManager anymore.

## Troubleshooting

### Random WiFi disconnects
Unfortunately the ESP32 has problems with some Access Points and reconnecting fails.
As a workaround you can navigate to "MQTT and Network Configuration" and enable "Restart on disconnect".
This will reboot the ESP as soon as it gets disconnected from WiFi. Also, this reduces
the config portal timeout to three minutes to prevent the ESP being stuck in config
mode in case an access point is offline temporarily.<br>
If this still doesn't fix the disconnects and the ESP becomes unreachable, the
"Restart timer" option can be used as a last resort. It will restart the ESP
after a configured amount of time.

### Pairing with the Lock (or Opener) doesn't work
First, try erasing the flash and then (re-)flash the firmware. To erase the flash, use the espressif download tool and click the "Erase" button.
Afterwards flash the firmware as described in the readme within the 7z file.
<br><br>
Also, there are reports that ESP32 "DEVKIT1" module don't work and pairing is not possible. The reason is unknown, but if you use such a module, try a different one.
<br><br>
Reported as working are:<br>
ESP32-WROOM-32D (DEVKIT V4)<br>
ESP32-WROOM-32E<br>
<br>
For more information check the related issue:<br>
https://github.com/technyon/nuki_hub/issues/39
<br><br>
Also, check that pairing is allowed. In the smartphone app, go to Settings --> Features & Configuration --> Button & LED and make sure "Bluetooh Pairing" is enabled.


### Authorization data isn't published
Reading the authorization data from the access log is protected by the configured PIN.
If you don't get any published data try setting the PIN or try reentering it to make
sure it's correct.

## Development VM

Since setting up the toolchain can be difficult, I've uploaded a virtual machine (vmware image) that is
setup to compile NUKI Hub:

https://mega.nz/file/8uRkDKyS#F0FNVJZTsUdcTMhiJIB47Fm-7YqKuyQs15E5zznmroc

User and password for the VM are both "nuki" and "nuki". The source is checked out at ~/projects/nuki_hub,
the cmake build directory is build. So to compile, run the following commands:

cd projects/nuki_hub/build<br>
ninja

To upload the image via serial port, run "ninja upload-nuki_hub". The serial device is defined in
~/.bashrc (Environment variable SERIAL_PORT), which you'll eventually have to adopt to your device.

## Disclaimer

This is a third party software for NUKI smart door locks. This project or any of it's authors aren't associated with Nuki Home Solutions GmbH. Please refer for official products and offical support to their website:

https://nuki.io/

For further license details, check the included LICENSE file.
