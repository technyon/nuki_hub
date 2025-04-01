### Hybrid mode combining official MQTT implementation and Nuki Hub ###

The purpose of this mode is to have Nuki Hub work in conjunction with the official MQTT implementation by Nuki.

For Nuki Hub to work properly it is essential that Nuki Hub is notified by the lock of state changes (e.g. locking/unlocking) as soon as possible.
Starting from the first versions of Nuki Hub this was achieved by registering Nuki Hub as a Nuki Bridge.

When a Nuki Bridge is registered with a Nuki lock or opener the device will signal state changes using a Bluetooth Low Energy (BLE) iBeacon.
If this state change is seen by Nuki Hub the Nuki Hub device will connect to the lock/opener over BLE and receive additional information about the state change.
The beacon itself contains no information other than that a state change has occured.
After the Nuki Bridge device (e.g. Nuki Hub in this case) has connected and requested the state change report the Nuki device will reset the iBeacon state to the "unchanged" state.

With the introduction of WiFi/Thread and MQTT enabled locks there now was a second method of retrieving state changes, which we call "Hybrid mode".
In Hybrid mode Nuki Hub subscribes to the MQTT topics that the Nuki lock writes to itself using the official Nuki MQTT implementation over WiFi/Thread.
Nuki Hub will now pick up state changes from these official MQTT topics. 
Part of these state changes is directly proxied to Nuki Hubs own MQTT topics (that your smart home system (e.g.) HomeAssistant is subscribed to) making this (in practise) just as fast as connecting your smart home application directly to the official MQTT implementation.
Nuki Hub will however also request additional information from the lock using BLE that is not available using the official Nuki MQTT implementation and publish this to Nuki Hubs MQTT topics.
If you have setup Hybrid mode consider taking a look at the information published on the official MQTT topics (base path "nuki/") and comparing this with the information Nuki Hub publishes (base topic "nukihub/" by default)

All functionality of Nuki Hub is available in both regular and hybrid mode.
In hybrid mode Nuki Hub will automatically choose the best way to communicate with the lock for retrieving information from the lock and pushing information (e.g. lock/unlock commands, change settings) to the lock based on the capabilities of the MQTT API and Bluetooth API.
When compared to regular/bridge mode this leads to speed increases in getting state changes and pushing state changes (because we can send and receive usefull information directly over MQTT without having to connect over BLE)
When compared to the official MQTT implementation this adds many many features that are not available in the official MQTT implementation and would normally require you to use the app or Web API (which has its own issues, downtime and cloud requirement).

**As the Nuki Smartlock Ultra/5th gen Pro has no support for the Nuki Bridge it is mandatory to setup Hybrid mode to receive prompt state changes from the lock in Nuki Hub.**

### Requirements ###

- ESP32 running Nuki Hub 9.08 or higher
- For WiFi: Nuki lock 3.0 Pro, Nuki Lock 4.0 Pro, Nuki Lock 5.0 Pro, Nuki Lock Go or Nuki Lock Ultra
- For Thread: Nuki Lock 4.0, Nuki Lock 4.0 Pro, Nuki Lock 5.0 Pro, Nuki Lock Go or Nuki Lock Ultra. Note that you do ***NOT*** need to buy the remote access addon for the Nuki Lock 4.0 or Nuki Lock Go
- For Thread: The Nuki Lock needs to have network access to the same MQTT server as the one that Nuki Hub is conected to. Depending if the MQTT server is reachable over IPv6 you might need an OpenThread Border router that supports NAT64 and has this enabled. Currently this means an Apple Device or Home Assistant with the Matter server and OpenThread Border Router
- The Nuki Opener does not have WiFI or Thread and thus doesn't benefit from the hybrid solutions added speed. You can however use and connect a Nuki Opener as usual which will function over regular BLE and can still connect Nuki Hub as a bridge to an Opener.

### Resources ###

- Discussion about setting up Nuki and Thread on the Nuki Developer forum: https://developer.nuki.io/t/smart-lock-4th-generation-set-up-home-assistant-for-remote-access-via-thread/25181
- Nuki KB article on MQTT: https://support.nuki.io/hc/en-us/articles/14052016143249-Activate-MQTT-via-the-Nuki-app
- Nuki KB article on Thread: https://support.nuki.io/hc/en-us/articles/18155425155217-Requirements-for-Remote-Access-via-Thread

### Reasons for using Hybrid mode combining official MQTT implementation and Nuki Hub

| Setup                                       | Speed         | Functionality  | Battery life | 
|---------------------------------------------|---------------|----------------|--------------|
| Nuki Hub paired as Bridge                   | --            | +++            | +++          |
| Nuki Hub paired as App                      | ++            | +++            | ---          |
| Matter over Thread                          | +++           | -              | ++           |
| Official MQTT over WiFi                     | +++           | +              | -            |
| Official MQTT over Thread                   | +++           | +              | ++           |
| Hybrid Official MQTT over WiFi + Nuki Hub   | +++           | +++            | --           |
| Hybrid Official MQTT over Thread + Nuki Hub | +++           | +++            | ++           |

The Hybrid Official MQTT over Thread + Nuki Hub solution allows for the best combination of state change speed, lock action execution, functionality and battery life.

### Setup ###

- Follow the official instruction by Nuki on setting up MQTT over Thread or WiFi on https://support.nuki.io/hc/en-us/articles/14052016143249-Activate-MQTT-via-the-Nuki-app.
  - Make sure to connect to the same MQTT server as Nuki Hub
  - Make sure ***NOT*** to enable `Auto discovery`, Home Assistant will be setup using Nuki Hub
  - Optionally enable `Allow locking`. Note that if you enable this setting it is preferred to set ACL on your MQTT broker to only allow the Nuki lock and Nuki Hub MQTT user access to the topic `nuki/NUKI-ID/lockAction` to make sure that only Nuki Hub can execute commands on the lock (otherwise ACL settings through Nuki Hub can not be 100% enforced)
  - Make sure that MQTT is setup correctly by checking if you get a green check mark in the Nuki app
 
- Install Nuki Hub 9.08 or higher on a supported ESP32 device
- Make sure you are not paired as a bridge. Unpair your Nuki lock in Nuki Hub if Nuki Hub was paired as a bridge (this is mandatory even if you removed the bridge connection from the Nuki lock).
- Enable `Enable hybrid official MQTT and Nuki Hub setup`. The `Lock: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)` setting will automatically be enabled.
- Optionally enable `Enable sending actions through official MQTT`, if not enabled lock actions will be sent over BLE as usual (slower)
- Set `Time between status updates when official MQTT is offline (seconds)` to a positive integer. If the Nuki lock MQTT connection goes offline for whatever reason Nuki Hub will update the lock state with the set interval in seconds.
- Optionally enable `Retry command sent using official MQTT over BLE if failed`. If sending a lock action over the official MQTT implementation fails the command will be resent over BLE if this is enabled. Requires `Enable sending actions through official MQTT` to be enabled.
- Optionally enable `Reboot Nuki lock on official MQTT failure`. If Nuki Hub determains that the official MQTT implementation is offline (usually because of the Nuki lock losing Thread/WiFi connection and not properly reconnecting) for more than 3 minutes Nuki Hub will try to reboot the Nuki lock (equivalent to removing and reinstalling the batteries).
- Save your configuration
- Consider setting the `Query intervals` on the `Advanced Nuki configuration` to high numbers (e.g. 86400) to further reduce battery drain.
- Pair your Nuki Lock with Nuki Hub
- Test that state changes are recieved and processed by Nuki Hub by looking at the MQTT topics using an application like `MQTT Explorer`