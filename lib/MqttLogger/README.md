# Remote logging to a MQTT server with the same print() interface as Serial

This library provides an object that can be used just like `Serial` for printing logs, 
however the text sent with `print()` etc. is published on a MQTT broker instead of 
printing over the Serial console. This comes in handy when working with devices like the 
ESP8266/ESP32 that are connected over WiFi. I use it for debugging my robots
that are based on ESP32.

The library uses [PubSubClient](https://github.com/knolleary/pubsubclient) for sending
the MQTT messages.

When no MQTT connection is available, the `MqttLogger` object behaves just like 
`Serial`, i.e. your `print()` text is shown on the `Serial` console. The logger offers
the following modes that can be passed as the third argument to the constructor
when instantiating the object:

* `MqttLoggerMode::MqttAndSerialFallback` - this is the default mode. `print()` will
  publish to the MQTT server, and only when no MQTT connection is available `Serial` 
  will be used. If you `print()` messages before the MQTT connection is established,
  these messages will be sent to the `Serial` console. 
* `MqttLoggerMode::MqttOnly` - no output on `Serial`. Beware: when no connection is 
  available, no output is produced
* `MqttLoggerMode::SerialOnly` - no messages are sent to the MQTT server. With this
  configuration `MqttLogger` can be used as a substitute for logging with `Serial`. 
* `MqttLoggerMode::MqttAndSerial` - messages are sent both to the MQTT server and to
  the `Serial` console. 

## Examples

See directory `examples`. Currently there is only one example in directory `esp32`.

In this directory, rename the file `wifi_secrets.h.txt` to `wifi_secrets.h` 
and edit the file. Enter your WiFi ssid and password, the example uses this
include file to set up your WiFi connection.

You'll need a MQTT broker to publish your messages to, I use [Mosquitto](https://mosquitto.org/) 
installed locally on my laptop. You can also use a free public service like 
`test.mosquitto.org` or `broker.hivemq.com`, but this makes logging slower 
(the messages have to be sent to and then downloaded from the online service). Also,
make sure no private information is logged!

The broker url is defined by the constant `mqtt_server` in the example, use
`localhost` if you have a local install as recommended.

For checking the mqtt logs events you'll use a MQTT client. The Mosquitto client 
can be invoked in a terminal like

    mosquitto_sub -h localhost -t mqttlogger/log

but any other mqtt client will do (on Android try MQTT Dash, hivemq has a online
version at (http://www.hivemq.com/demos/websocket-client/).

## Compatible Hardware

All devices that work with the PubSubClient should work with this libary, including:

 - Arduino Ethernet
 - Arduino Ethernet Shield
 - Arduino YUN – use the included `YunClient` in place of `EthernetClient`, and
   be sure to do a `Bridge.begin()` first
 - Arduino WiFi Shield - if you want to send packets > 90 bytes with this shield,
   enable the `MQTT_MAX_TRANSFER_SIZE` define in `PubSubClient.h`.
 - Sparkfun WiFly Shield – [library](https://github.com/dpslwk/WiFly)
 - TI CC3000 WiFi - [library](https://github.com/sparkfun/SFE_CC3000_Library)
 - Intel Galileo/Edison
 - ESP8266
 - ESP32

## License

This code is released under the MIT License.