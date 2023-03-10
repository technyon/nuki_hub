#include <ESP8266WiFi.h>
#include <Updater.h>

#include <espMqttClient.h>

#define WIFI_SSID "yourSSID"
#define WIFI_PASSWORD "yourpass"

#define MQTT_HOST IPAddress(192, 168, 130, 10)
#define MQTT_PORT 1883

#define UPDATE_TOPIC "device/firmware/set"

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
espMqttClient mqttClient;
bool reconnectMqtt = false;
uint32_t lastReconnect = 0;
bool disconnectFlag = false;
bool restartFlag = false;

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

void onWiFiConnect(const WiFiEventStationModeGotIP& event) {
  (void) event;
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWiFiDisconnect(const WiFiEventStationModeDisconnected& event) {
  (void) event;
  Serial.println("Disconnected from Wi-Fi.");
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe(UPDATE_TOPIC, 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.printf("Disconnected from MQTT: %u.\n", static_cast<uint8_t>(reason));

  if (disconnectFlag) {
    restartFlag = true;
    return;
  }
  
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

void handleUpdate(const uint8_t* payload, size_t length, size_t index, size_t total) {
  // The Updater class takes a non-const pointer to write data although it doesn't change the data
  uint8_t* data = const_cast<uint8_t*>(payload);
  static size_t written = 0;
  Update.runAsync(true);
  if (index == 0) {
    if (Update.isRunning()) {
      Update.end();
      Update.clearError();
    }
    Update.begin(total);
    written = Update.write(data, length);
    Serial.printf("Updating %u/%u\n", written, Update.size());
  } else {
    if (!Update.isRunning()) return;
    written += Update.write(data, length);
    Serial.printf("Updating %u/%u\n", written, Update.size());
  }
  if (Update.isFinished()) {
    if (Update.end()) {
      Serial.println("Update succes");
      disconnectFlag = true;
    } else {
      Serial.printf("Update error: %u\n", Update.getError());
      Update.printError(Serial);
      Update.clearError();
    }
  }
}

void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  (void) properties;
  if (strcmp(UPDATE_TOPIC, topic) != 0) {
    Serial.println("Topic mismatch");
    return;
  }
  handleUpdate(payload, len, index, total);
}

void setup() {
  Serial.begin(74880);
  Serial.println();
  Serial.println();

  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(true);
  wifiConnectHandler = WiFi.onStationModeGotIP(onWiFiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWiFi();
}

void loop() {
  if (restartFlag) {
    Serial.println("Rebooting... See you next time!");
    Serial.flush();
    ESP.reset();
  }

  static uint32_t currentMillis = millis();

  mqttClient.loop();

  if (!disconnectFlag && reconnectMqtt && currentMillis - lastReconnect > 5000) {
    connectToMqtt();
  }

  if (disconnectFlag) {
    // it's safe to call this multiple times
    mqttClient.disconnect();
  }
}