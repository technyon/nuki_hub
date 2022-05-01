## About

The NUKI Hub software acts as a bridge between a NUKI Lock and a smart home system. Exposes the lock state (and more) via MQTT, and allows to execute commands like locking and unlocking

## Installation

Flash the firmware to an ESP32. The easiest way to install is to use the web installer:<br>
https://technyon.github.io/nuki_hub/
<br>(Needs a web serial compatible browser like Chrome/Opera/Edge).

As an alternative, download a release:<br>
https://github.com/technyon/nuki_hub/releases
<br>Read the included readme.txt for installation instructions for either Espressif Flash Download Tools or esptool.



## Setup

The firmware uses the Wifi Manager to configure the WiFi network. Power up the ESP32, a new Access Point should appear. Connect to this access point and in a browser navigate to "192.168.4.1". Use the web interface configure your Wifi network.

After configuring the Wifi, the ESP should automatically connect to your network. Use the web interface to setup the MQTT broker; just navigate to the IP-Address assigned to the ESP32 via DHCP (often found the web interface of the internet router).

## Paring

Just enable pairing mode on the NUKI lock and power on the ESP32. Pairing should be automatic. Unfortunately there's no feedback at the time, except for debug output on the serial line. If it worked, the lock state and the battery level should be reported via MQTT.

## MQTT Interface

- lock/action: Allows to execute lock actions. After executing the action, the value is reset to an empty string. Possible actions: unlock, lock, unlatch, lockNgo, lockNgoUnlatch, fullLock, fobAction1, fobAction2, fobAction3
- lock/state: Reports the current lock state as a string. Possible values are: uncalibrated, locked, unlocked, unlatched, unlockedLnga, unlatching, bootRun, motorBlocked
- lock/trigger: The trigger of the last action: autoLock, automatic, button, manual, system
- lock/completionStatus: Status of the last action as reported by NUKI lock (needs bluetooth connection): success, motorBlocked, canceled, tooRecent, busy, lowMotorVoltage, clutchFailure, motorPowerFailure, incompleteFailure, invalidCode, otherError, unknown
- lock/commandResult: Result of the last action as reported by NUKI library: success, failed, timeOut, working, notPaired, error, undefined
- lock/doorSensorState: State of the door sensor: unavailable, deactivated, doorClosed, doorOpened, doorStateUnknown, calibrating

- battery/voltage: Reports the current battery voltage in Volts.
- battery/level: Battery level in percent
- battery/critical: 1 if battery level is critical, otherwise 0
- battery/charging: 1 if charging, otherwise 0
- battery/voltage: Current Battery voltage (V)
- battery/drain: The drain of the last lock action in Milliwattseconds (mWs)
- battery/maxTurnCurrent: The highest current of the turn motor during the last lock action (A)

- presence/devices: List of detected bluetooth devices as CSV. Can be used for presence detection

## Connecting via LAN (Optional)

If you prefer to connect to the MQTT Broker via LAN instead of WiFi, you can use a Wizent W5x00 Module (W5100, W5200, W5500 are supported).
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

## Disclaimer

This is a third party software for NUKI smart door locks. This project or any of it's authors aren't associated with Nuki Home Solutions GmbH. Please refer for official products and offical support to their website:

https://nuki.io/

For further license details, check the included LICENSE file.
