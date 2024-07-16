### Hybrid mode combining official MQTT implementation and Nuki Hub ###

The purpose of this mode is to have Nuki Hub work in conjunction with the official MQTT implementation by Nuki.

### Requirements ###

- ESP32 running Nuki Hub 8.35 or higher
- For WiFi: Nuki lock 3.0 Pro or Nuki Lock 4.0 Pro
- For Thread: Nuki Lock 4.0 or Nuki Lock 4.0 Pro. Note that you do ***NOT*** need to buy the remote access addon for the Nuki Lock 4.0
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
 
- Install Nuki Hub 8.35 or higher on a supported ESP32 device
- Make sure you are not paired as a bridge. Unpair your Nuki lock in Nuki Hub if Nuki Hub was paired as a bridge (this is mandatory even if you removed the bridge connection from the Nuki lock).
- Enable `Enable hybrid official MQTT and Nuki Hub setup`. The `Lock: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)` setting will be automatically be enabled.
- Optionally enable `Enable sending actions through official MQTT`, if not enabled lock actions will be sent over BLE as usual (slower)
- Set `Time between status updates when official MQTT is offline (seconds)` to a positive integer. If the Nuki lock MQTT connection goes offline for whatever reason Nuki Hub will update the lock state with the set interval in seconds.
- Optionally enable `Retry command sent using official MQTT over BLE if failed`. If sending a lock action over the official MQTT implementation fails the command will be resent over BLE if this is enabled. Requires `Enable sending actions through official MQTT` to be enabled.
- Save your configuration
- Consider setting the `Query intervals` on the `Advanced Nuki configuration` to high numbers (e.g. 86400) to further reduce battery drain.
- Pair your Nuki Lock with Nuki Hub
- Test that state changes are recieved and processed by Nuki Hub by looking at the MQTT topics using an application like `MQTT Explorer`
