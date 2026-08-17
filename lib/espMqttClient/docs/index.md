![platformio](https://github.com/bertmelis/espMqttClient/actions/workflows/build_platformio.yml/badge.svg)
![cpplint](https://github.com/bertmelis/espMqttClient/actions/workflows/cpplint.yml/badge.svg)
![cppcheck](https://github.com/bertmelis/espMqttClient/actions/workflows/cppcheck.yml/badge.svg)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/bertmelis/library/espMqttClient.svg)](https://registry.platformio.org/libraries/bertmelis/espMqttClient)

# Features

- MQTT 3.1.1 compliant library
- Sending and receiving at all QoS levels
- TCP and TCP/TLS using standard WiFiClient and WiFiClientSecure connections
- Virtually unlimited incoming and outgoing payload sizes
- Readable and understandable code
- Fully async clients available via [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) or [ESPAsnycTCP](https://github.com/ESP32Async/ESPAsyncTCP) (no TLS supported).
- Supported platforms:
  - Espressif ESP8266 and ESP32 using the Arduino framework
  - Espressif ESP32 using the ESP IDF, see [esp idf component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
- Basic Linux compatibility*. This includes WSL on Windows

    > Linux compatibility is mainly for automatic testing. It relies on a quick and dirty Arduino-style `Client` with a POSIX TCP client underneath and Arduino-style `IPAddress` class. These are lacking many features needed for proper Linux support.

# Contents

1. [Runtime behaviour](#runtime-behaviour)
2. [API Reference](#api-reference)
3. [Compile-time configuration](#compile-time-configuration)
4. [Code samples](#code-samples)

# Runtime behaviour

A normal operation cycle of an MQTT client goes like this:

1. setup the client
2. connect to the broker
3. subscribe/publish/receive
4. disconnect/reconnect when disconnected
5. Cleanly disconnect

### Setup

Setting up the client means to tell which host and port to connect to, possible credentials to use and so on. espMqttClient has a set of methods to configure the client. Setup is generally done in the `setup()` function of the Arduino framework.
One important thing to remember is that there are a number of settings that are not stored inside the library: `username`, `password`, `willTopic`, `willPayload`, `clientId` and `host`. Make sure these variables stay available during the lifetime of the `espMqttClient`.

For TLS secured connections, the relevant methods from `WiFiClientSecure` have been made available to setup the TLS mechanisms.

### Connecting

After setting up the client, you are ready to connect. A simple call to `connect()` does the job. If you set an `OnConnectCallback`, you will be notified when the connection has been made. On failure, `OnDisconnectCallback` will be called. Although good code structure can avoid this, you can call `connect()` multiple times.

### Subscribing, publishing and receiving

Once connected, you can subscribe, publish and receive. The methods to do this return the packetId of the generated packet or `1` for packets without packetId. In case of an error, the method returns `0`. When the client is not connected, you cannot subscribe, unsubscribe or publish (configurable, see [EMC_ALLOW_NOT_CONNECTED_PUBLISH](#EMC_ALLOW_NOT_CONNECTED_PUBLISH)).

Receiving packets is done via the `onMessage`-callback. This callback gives you the topic, properties (qos, dup, retain, packetId) and payload. For the payload, you get a pointer to the data, the index, length and total length. On long payloads it is normal that you get multiple callbacks for the same packet. This way, you can receive payloads longer than what could fit in the microcontroller's memory.

    > Beware that MQTT payloads are binary. MQTT payloads are **not** c-strings unless explicitely constructed like that. You therefore can **not** print the payload to your Serial monitor without supporting code.

### Disconnecting

You can disconnect from the broker by calling `disconnect()`. If you do not force-disconnect, the client will first send the remaining messages that are in the queue and disconnect afterwards. During this period however, no new incoming PUBLISH messages will be processed.

# API Reference

```cpp
espMqttClient()
espMqttClientSecure()
espMqttClientAsync()
```

Instantiate a new espMqttClient or espMqttSecure object.
On ESP32, three optional parameters are available: `espMqttClient(bool internalTask = true, uint8_t priority = 1, uint8_t core = 1)`. By default, espMqttclient creates its own task to manage TCP. By setting `internalTask` to false, no task will be created and you will be responsible yourself to call `espMqttClient.loop()`. `priority` changes the priority of the MQTT client task and the core on which it runs (higher priority = more cpu-time).

For the asynchronous version, use `espMqttClientAsync`.

### Configuration

```cpp
espMqttClient& setKeepAlive(uint16_t keepAlive)
```

Set the keep alive. Defaults to 15 seconds.

* **`keepAlive`**: Keep alive in seconds

```cpp
espMqttClient& setClientId(const char* clientId)
```

Set the client ID. Defaults to `esp8266123456` or `esp32123456` where `123456` is the chip ID.
The library only stores a pointer to the client ID. Make sure the variable pointed to stays available throughout the lifetime of espMqttClient.

- **`clientId`**: Client ID, expects a null-terminated char array (c-string)

```cpp
espMqttClient& setCleanSession(bool cleanSession)
```

Set the CleanSession flag. Defaults to `true`.

- **`cleanSession`**: clean session wanted or not

```cpp
espMqttClient& setCredentials(const char* username, const char* password)
```

Set the username/password. Defaults to non-auth.
The library only stores a pointer to the username and password. Make sure the variable to pointed stays available throughout the lifetime of espMqttClient.

- **`username`**: Username, expects a null-terminated char array (c-string)
- **`password`**: Password, expects a null-terminated char array (c-string)

```cpp
espMqttClient& setWill(const char* topic, uint8_t qos, bool retain, const uint8_t* payload, size_t length)
```

Set the Last Will. Defaults to none.
The library only stores a pointer to the topic and payload. Make sure the variable pointed to stays available throughout the lifetime of espMqttClient.

- **`topic`**: Topic of the LWT, expects a null-terminated char array (c-string)
- **`qos`**: QoS of the LWT
- **`retain`**: Retain flag of the LWT
- **`payload`**: Payload of the LWT.
- **`length`**: Payload length

```cpp
espMqttClient& setWill(const char* topic, uint8_t qos, bool retain, const char* payload)
```

Set the Last Will. Defaults to none.
The library only stores a pointer to the topic and payload. Make sure the variable pointed to stays available throughout the lifetime of espMqttClient.

- **`topic`**: Topic of the LWT, expects a null-terminated char array (c-string)
- **`qos`**: QoS of the LWT
- **`retain`**: Retain flag of the LWT
- **`payload`**: Payload of the LWT, expects a null-terminated char array (c-string). Its lenght will be calculated using `strlen(payload)`

```cpp
espMqttClient& setServer(IPAddress ip, uint16_t port)
```

Set the server. Mind that when using `espMqttClientSecure` with a certificate, the hostname will be chacked against the certificate. Often IP-addresses are not valid and the connection will fail.

- **`ip`**: IP of the server
- **`port`**: Port of the server

```cpp
espMqttClient& setServer(const char* host, uint16_t port)
```

Set the server.

- **`host`**: Host of the server, expects a null-terminated char array (c-string)
- **`port`**: Port of the server

```cpp
espMqttClient& setTimeout(uint16_t timeout)
```

Set the timeout for packets that need acknowledgement. Defaults to 10 seconds.
When no acknowledgement has been received from the broker after sending a packet, the client will retransmit **all** the packets in the queue.

* **`timeout`**: Timeout in seconds

#### Options for TLS connections

All common options from WiFiClientSecure to setup an encrypted connection are made available. These include:

- `espMqttClientSecure& setInsecure()`
- `espMqttClientSecure& setCACert(const char* rootCA)` (ESP32 only)
- `espMqttClientSecure& setCertificate(const char* clientCa)` (ESP32 only)
- `espMqttClientSecure& setPrivateKey(const char* privateKey)` (ESP32 only)
- `espMqttClientSecure& setPreSharedKey(const char* pskIdent, const char* psKey)` (ESP32 only)
- `espMqttClientSecure& setFingerprint(const uint8_t fingerprint[20])` (ESP8266 only)
- `espMqttClientSecure& setTrustAnchors(const X509List *ta)` (ESP8266 only)
- `espMqttClientSecure& setClientRSACert(const X509List *cert, const PrivateKey *sk)` (ESP8266 only)
- `espMqttClientSecure& setClientECCert(const X509List *cert, const PrivateKey *sk, unsigned allowed_usages, unsigned cert_issuer_key_type)` (ESP8266 only)
- `espMqttClientSecure& setCertStore(CertStoreBase *certStore)` (ESP8266 only)

For documenation, please visit [ESP8266's documentation](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html#bearssl-client-secure-and-server-secure) or [ESP32's documentation](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure).

### Events handlers

```cpp
espMqttClient& onConnect(espMqttClientTypes::OnConnectCallback callback)
```

Add a connect event handler. Function signature: `void(bool sessionPresent)`

- **`callback`**: Function to call

```cpp
espMqttClient& onDisconnect(espMqttClientTypes::OnDisconnectCallback callback)
```

Add a disconnect event handler. Function signature: `void(espMqttClientTypes::DisconnectReason reason)`

- **`callback`**: Function to call

```cpp
espMqttClient& onSubscribe(espMqttClientTypes::OnSubscribeCallback callback)
```

Add a subscribe acknowledged event handler. Function signature: `void(uint16_t packetId, const espMqttClientTypes::SubscribeReturncode* returncodes, size_t len)`

- **`callback`**: Function to call

```cpp
espMqttClient& onUnsubscribe(espMqttClientTypes::OnUnsubscribeCallback callback)
```

Add an unsubscribe acknowledged event handler. Function signature: `void(uint16_t packetId)`

- **`callback`**: Function to call

```cpp
espMqttClient& onMessage(espMqttClientTypes::OnMessageCallback callback)
```

Add a publish received event handler. Function signature: `void(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)`

- **`callback`**: Function to call

```cpp
espMqttClient& onPublish(espMqttClientTypes::OnPublishCallback callback)
```

Add a publish acknowledged event handler. Function signature: `void(uint16_t packetId)`

- **`callback`**: Function to call

### Operational functions

```cpp
bool connected()
```

Returns `true` if the client is currently fully connected to the broker. During connecting or disconnecting, it will return `false`.

```cpp
bool disconnected()
```

Returns `true` if the client is currently disconnected from the broker. During disconnecting or connecting, it will return `false`.

```cpp
bool connect()
```

Start the connect procedure. Returns `true` if successful. A positive return value doesn not mean the client is already connected.

```cpp
bool disconnect(bool force = false)
```

Start the disconnect procedure, return `true` if successful. A positive return value doesn not mean the client is already disconnected.
When disconnecting with `force` false, the client first tries to handle all the outgoing messages in the queue and disconnect cleanly afterwards. During this time, no incoming PUBLISH messages are handled.

- **`force`**: Whether to force the disconnection. Defaults to `false` (clean disconnection).

```cpp
uint16_t subscribe(const char* topic, uint8_t qos)
```

Subscribe to the given topic at the given QoS. Return the packet ID or 0 if failed.

- **`topic`**: Topic, expects a null-terminated char array (c-string)
- **`qos`**: QoS

It is also possible to subscribe to multiple topics at once. Just add the topic/qos pairs to the parameters:

```cpp
uint16_t packetId = yourclient.subscribe(topic1, qos1, topic2, qos2, topic3, qos3);  // add as many topics as you like*
```

```cpp
uint16_t unsubscribe(const char* topic)
```

Unsubscribe from the given topic. Return the packet ID or 0 if failed.

- **`topic`**: Topic, expects a null-terminated char array (c-string)

It is also possible to unsubscribe to multiple topics at once. Just add the topics to the parameters:

```cpp
uint16_t packetId = yourclient.unsubscribe(topic1, topic2, topic3);  // add as many topics as you like*
```

```cpp
uint16_t publish(const char* topic, uint8_t qos, bool retain, const uint8* payload, size_t length)
```

Publish a packet. Return the packet ID (or 1 if QoS 0) or 0 if failed. The topic and payload will be buffered by the library.

- **`topic`**: Topic, expects a null-terminated char array (c-string)
- **`qos`**: QoS
- **`retain`**: Retain flag
- **`payload`**: Payload
- **`length`**: Payload length

```cpp
uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload)
```

Publish a packet. Return the packet ID (or 1 if QoS 0) or 0 if failed. The topic and payload will be buffered by the library.

- **`topic`**: Topic, expects a null-terminated char array (c-string)
- **`qos`**: QoS
- **`retain`**: Retain flag
- **`payload`**: Payload, expects a null-terminated char array (c-string). Its lenght will be calculated using `strlen(payload)`

```cpp
uint16_t publish(const char* topic, uint8_t qos, bool retain, espMqttClientTypes::PayloadCallback callback, size_t length)
```

Publish a packet with a callback for payload handling. Return the packet ID (or 1 if QoS 0) or 0 if failed. The topic will be buffered by the library.

- **`topic`**: Topic, expects a null-terminated char array (c-string)
- **`qos`**: QoS
- **`retain`**: Retain flag
- **`callback`**: callback to fetch the payload.

The callback has the following signature: `size_t callback(uint8_t* data, size_t maxSize, size_t index)`. When the library needs payload data, the callback will be invoked. It is the callback's job to write data indo `data` with a maximum of `maxSize` bytes, according the `index` and return the amount of bytes written.

```cpp
void clearQueue(bool deleteSessionData = false)
```

Clears all queued messages.
Keep in mind that this may also delete any session data and therefore is not MQTT compliant.

- **`deleteSessionData`**: When true, delete all outgoing messages. Not MQTT compliant!

```cpp
void loop()
```

This is the worker function of the MQTT client. For ESP8266 you must call this function in the Arduino loop. For ESP32 you have to call this function yourself **only if you have disabled the internal task** (see the constructors).

```cpp
const char* getClientId() const
```

Retuns the client ID.

```cpp
size_t queueSize();
```

Returns the amount of elements, regardless of type, in the queue.

# Compile time configuration

A number of constants which influence the behaviour of the client can be set at compile time. You can set these options in the `Config.h` file or pass the values as compiler flags. Because these options are compile-time constants, they are used for all instances of `espMqttClient` you create in your program.

### EMC_TX_TIMEOUT 10000

Timeout in milliseconds before a (qos > 0) message will be retransmitted.

### EMC_RX_BUFFER_SIZE 1440

The client copies incoming data into a buffer before parsing. This sets the buffer size.

### EMC_TX_BUFFER_SIZE 1440

When publishing using the callback, the client fetches data in chunks of EMC_TX_BUFFER_SIZE size. This is not necessarily the same as the actual outging TCP packets.

### EMC_MAX_TOPIC_LENGTH 128

For **incoming** messages, a maximum topic length is set. Topics longer than this will be truncated.

### EMC_PAYLOAD_BUFFER_SIZE 32

Set the incoming payload buffer size for SUBACK messages. When subscribing to multiple topics at once, the acknowledgement contains all the return codes in its payload. The detault of 32 means you can theoretically subscribe to 32 topics at once.

### EMC_MIN_FREE_MEMORY 4096

The client keeps all outgoing packets in a queue which stores its data in heap memory. With this option, you can set the minimum available (contiguous) heap memory that needs to be available for adding a message to the queue.

### EMC_ESP8266_MULTITHREADING 0

Set this to 1 if you use the async version on ESP8266. For the regular client this setting can be kept disabled because the ESP8266 doesn't use multithreading and is only single-core.

### EMC_ALLOW_NOT_CONNECTED_PUBLISH 1

By default, you can publish when the client is not connected. If you don't want this, set this to 0.
Regardless of this setting, after you called `disconnect()`, no messages can be published until fully disconnected.

### EMC_WAIT_FOR_CONNACK 1

espMqttClient waits for the CONNACK (connection acknowledge) packet before starting to send other packets.
The MQTT specification allows to start sending before the broker acknowledges the connection but some brokers
don't allow this (AWS for example doesn't).

### EMC_CLIENTID_LENGTH 18 + 1

The (maximum) length of the client ID. (Keep in mind that this is a c-string. You need to have 1 position available for the null-termination.)

### EMC_TASK_STACK_SIZE 5120

Only used on ESP32. Sets the stack size (in words) of the MQTT client worker task.

### EMC_MULTIPLE_CALLBACKS

This macro is by default not enabled so you can add a single callbacks to an event. Assigning a second will overwrite the existing callback. When enabling multiple callbacks, multiple callbacks (with uint32_t id) can be assigned. Removing is done by referencing the id.

### EMC_USE_WATCHDOG 0

(ESP32 only)

**Experimental**

You can enable a watchdog on the MQTT task. This is experimental and will probably result in resets because some (framework) function calls block without feeding the dog.

### EMC_USE_MEMPOOL 0

**Experimental**

When set to `1`, (outgoing) MQTT packets and the outbox data is stored in a memory pool. The memory pool is part of the espMqttClient object and is thus allocated in the same memory type. There are two pools: one to hold the outgoing packets (dynamic size elements) and one for the outbox itself (fixed-size elements).

#### EMC_NUM_POOL_ELEMENTS 32

This config variable is only used when enabling the memory pool. It defines
- the number of elements in the outbox-pool
- the number of blocks that will be allocated in the packet-pool

#### EMC_SIZE_POOL_ELEMENTS 128

This defines the size of one packet-pool element. Together with `EMC_NUM_POOL_ELEMENTS`, you get the total packet-pool size.
The packet-pool can hold any size of element. The configuration only guarantees a minimum of `EMC_NUM_POOL_ELEMENTS` of size `EMC_SIZE_POOL_ELEMENTS` can fit in the pool.

### Logging

If needed, you have to enable logging at compile time. This is done differently on ESP32 and ESP8266.

ESP8266:

- Enable logging for Arduino [see docs](https://arduino-esp8266.readthedocs.io/en/latest/Troubleshooting/debugging.html)
- Pass the `DEBUG_ESP_MQTT_CLIENT` flag to the compiler

ESP32

- Enable logging for Arduino [see docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/guides/tools_menu.html?#core-debug-level)

# Code samples

A number of examples are in the [examples](/examples) directory. These include basic operation on ESP8266 and ESP32. Please examine these to understand the basic operation of the MQTT client.

Below are examples on specific points for working with this library.

### Printing payloads

MQTT 3.1.1 defines no special format for the payload so it is treated as binary. If you want to print a payload to the Arduino serial console, you have to make sure that the payload is null-terminated (c-string).

```cpp
// option one: print the payload char by char
void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  Serial.println("Publish received:");
  Serial.printf("  topic: %s\n  payload:", topic);
  const char* p = reinterpret_cast<const char*>(payload);
  for (size_t i = 0; i < len; ++i) {
    Serial.print(p[i]);
  }
  Serial.print("\n");
}
```

```cpp
// option two: copy the payload into a c-string
// you cannot just do payload[len] = 0 because you do not own this memory location! 
void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  Serial.println("Publish received:");
  Serial.printf("  topic: %s\n  payload:", topic);
  char* strval = new char[len + 1];
  memcpy(strval, payload, len);
  strval[len] = "\0";
  Serial.println(strval);
  delete[] strval;
}
```

### Assembling chunked messages

The `onMessage`-callback is called as data comes in. So if the data comes in partially, the callback will be called on every receipt of a chunk, with the proper `index`, (chunk)`size` and `total` set. With little code, you can reassemble chunked messages yourself.

```cpp
const size_t maxPayloadSize = 8192;
uint8_t* payloadbuffer = nullptr;
size_t payloadbufferSize = 0;
size_t payloadbufferIndex = 0;

void onOversizedMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  // handle oversized messages
}

void onCompleteMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  // handle assembled messages
}

void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  // payload is bigger then max: return chunked
  if (total > maxPayloadSize) {
    onOversizedMqttMessage(properties, topic, payload, len, index, total);
    return;
  }

  // start new packet, increase buffer size if neccesary
  if (index == 0) {
    if (total > payloadbufferSize) {
      delete[] payloadbuffer;
      payloadbufferSize = total;
      payloadbuffer = new (std::nothrow) uint8_t[payloadbufferSize];
      if (!payloadbuffer) {
        // no buffer could be created. you might want to log this somewhere
        return;
      }
    }
    payloadbufferIndex = 0;
  }

  // add data and dispatch when done
  if (payloadBuffer) {
    memcpy(&payloadbuffer[payloadbufferIndex], payload, len);
    payloadbufferIndex += len;
    if (payloadbufferIndex == total) {
      // message is complete here
      onCompleteMqttMessage(properties, topic, payloadBuffer, total, 0, total);
      // optionally:
      delete[] payloadBuffer;
      payloadBuffer = nullptr;
      payloadbufferSize = 0;
    }
  }
}

// attach callback to MQTT client
mqttClient.onMessage(onMqttMessage);
```

### onMessage callbacks per topic

espMqttClient allows only one callback for incoming messages. You might want to have specific ones per topic. This example shows one way on how to achieve this.

Limitations of this code sample: only the first match is served and no wildcard topics allowed.

```cpp
#include <map>
#include <cstring>

// definitions of the std::map where we will store the topic/callback combinations
struct MatchTopic {
  bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};
std::map<const char*, espMqttClientTypes::OnMessageCallback, MatchTopic> topicCallbacks;

// callbacks per topic
void onTopic1(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  // received a packet on topic 1
}
void onTopic2(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  // received a packet on topic 2
}

// general callback to dispatch to specific handlers
void onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  auto it = topicCallbacks.find(topic);
  if (it != topicCallbacks.end()) {
    // if found, run specific callback
    (it->second)(properties, topic, payload, len, index, total);
  } else {
    // or handle it here
  }
}

// in your Arduino setup() function:
topicCallbacks.emplace("base/topic1", onTopic1);
topicCallbacks.emplace("base/topic2", onTopic2);

mqttClient.onMessage(onMessage);
```
