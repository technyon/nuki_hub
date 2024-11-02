#include <WiFi.h>

#include <espMqttClient.h>

#define WIFI_SSID "yourSSID"
#define WIFI_PASSWORD "yourpass"

#define MQTT_HOST "mqtt.yourhost.com"
#define MQTT_PORT 8883
#define MQTT_USER "username"
#define MQTT_PASS "password"

const char rootCA[] = \
  "-----BEGIN CERTIFICATE-----\n" \
  " add your certificate here \n" \
  "-----END CERTIFICATE-----\n";

espMqttClientSecure mqttClient(espMqttClientTypes::UseInternalTask::NO);
static TaskHandle_t taskHandle;
bool reconnectMqtt = false;
uint32_t lastReconnect = 0;

void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      break;
    default:
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  uint16_t packetIdSub0 = mqttClient.subscribe("foo/bar/0", 0);
  Serial.print("Subscribing at QoS 0, packetId: ");
  Serial.println(packetIdSub0);

  uint16_t packetIdPub0 = mqttClient.publish("foo/bar/0", 0, false, "test");
  Serial.println("Publishing at QoS 0, packetId: ");
  Serial.println(packetIdPub0);
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.printf("Disconnected from MQTT: %u.\n", static_cast<uint8_t>(reason));

  if (WiFi.isConnected()) {
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

void networkingTask() {
  for (;;) {
    mqttClient.loop();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(WiFiEvent);

  //mqttClient.setInsecure();
  mqttClient.setCACert(rootCA);
  mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCleanSession(true);

  xTaskCreatePinnedToCore((TaskFunction_t)networkingTask, "mqttclienttask", 5120, nullptr, 1, &taskHandle, 0);

  connectToWiFi();
}

void loop() {
  static uint32_t currentMillis = millis();

  if (reconnectMqtt && currentMillis - lastReconnect > 5000) {
    connectToMqtt();
  }

  static uint32_t lastMillis = 0;
  if (currentMillis - lastMillis > 5000) {
    lastMillis = currentMillis;
    Serial.printf("heap: %u\n", ESP.getFreeHeap());
  }

  static uint32_t millisDisconnect = 0;
  if (currentMillis - millisDisconnect > 60000) {
    millisDisconnect = currentMillis;
    mqttClient.disconnect();
  }
}
