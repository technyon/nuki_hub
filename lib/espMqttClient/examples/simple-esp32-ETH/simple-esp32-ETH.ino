
#if __has_include(<Network.h>) == 0
#  error "Network/Ethernet stack not available."
#endif

#include <ETH.h>
#include <SPI.h>

#include <Network.h>

#include <espMqttClient.h>

#define MQTT_HOST IPAddress(192, 168, 1, 10)
#define MQTT_PORT 8883

#define MQTT_PSK_ID "mqttpskid"
#define MQTT_PSK    "70736B70736B70736B"


espMqttClientSecure mqttClient;
bool reconnectMqtt = false;
uint32_t lastReconnect = 0;

const uint8_t
  pinCsETH     = GPIO_NUM_14, // Chip Select LAN-Interface
  pinClkSCK    = GPIO_NUM_18, // Serial Clock
  pinMISO      = GPIO_NUM_19, // MISO
  pinMOSI      = GPIO_NUM_23, // MOSI
  pinIntETH    = GPIO_NUM_15, // Interrupt LAN-Interface
  pinRstETH    = GPIO_NUM_27; // Reset LAN-Interface

void connectToNetwork() {
  Serial.println("Connecting to Network...");

  SPI.begin(pinClkSCK, pinMISO, pinMOSI);
  ETH.begin(ETH_PHY_W5500, 1, pinCsETH, -1, pinRstETH, SPI); // without IRQ
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  if (!mqttClient.connect()) {
    reconnectMqtt = true;
    lastReconnect = millis();
    Serial.println("Connecting failed.");
  } else {
    reconnectMqtt = false;
  }
}

static bool eth_connected = false;

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-eth0");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: Serial.println("ETH Connected"); break;
    case ARDUINO_EVENT_ETH_GOT_IP:    Serial.printf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif)); Serial.println(ETH);
#if USE_TWO_ETH_PORTS
      Serial.println(ETH1);
#endif
      eth_connected = true;
      connectToMqtt();
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH Lost IP");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default: break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe("foo/bar", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
  mqttClient.publish("foo/bar", 0, true, "test 1");
  Serial.println("Publishing at QoS 0");
  uint16_t packetIdPub1 = mqttClient.publish("foo/bar", 1, true, "test 2");
  Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1);
  uint16_t packetIdPub2 = mqttClient.publish("foo/bar", 2, true, "test 3");
  Serial.print("Publishing at QoS 2, packetId: ");
  Serial.println(packetIdPub2);
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.printf("Disconnected from MQTT: %u.\n", static_cast<uint8_t>(reason));

  if (eth_connected) {
    reconnectMqtt = true;
    lastReconnect = millis();
  }
}

void onMqttSubscribe(uint16_t packetId, const espMqttClientTypes::SubscribeReturncode* codes, size_t len) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  for (size_t i = 0; i < len; ++i) {
    Serial.print("  qos: ");
    Serial.println(static_cast<uint8_t>(codes[i]));
  }
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  (void) payload;
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  String pl = String((const char*)payload).substring(0, len);
  Serial.print("  payload: ");
  Serial.println(pl);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  Network.onEvent(onEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setPreSharedKey(MQTT_PSK_ID, MQTT_PSK);

  connectToNetwork();
}

void loop() {
  static uint32_t currentMillis = millis();

  if (reconnectMqtt && currentMillis - lastReconnect > 5000) {
    Serial.println("connectToMqtt()");
    connectToMqtt();
  }
}
