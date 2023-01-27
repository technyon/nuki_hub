#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <espMqttClient.h>

#define WIFI_SSID "yourSSID"
#define WIFI_PASSWORD "yourpass"

#define MQTT_HOST IPAddress(192, 168, 1, 10)
#define MQTT_PORT 1883

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
espMqttClient mqttClient;
Ticker reconnectTimer;

size_t fetchPayload(uint8_t* dest, size_t len, size_t index) {
  Serial.printf("filling buffer at index %zu\n", index);
  // fill the buffer with random bytes
  // but maybe don't fill the entire buffer
  size_t i = 0;
  for (; i < len; ++i) {
    dest[i] = random(0xFF);
    if (dest[i] > 0xFC) {
      ++i;  // extra increment to compensate 'break'
      break;
    }
  }
  return i; 
}

void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onWiFiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWiFiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  reconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  reconnectTimer.once(5, connectToWiFi);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  mqttClient.publish("topic/largepayload", 1, false, fetchPayload, 6000);
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.printf("Disconnected from MQTT: %u.\n", static_cast<uint8_t>(reason));

  if (WiFi.isConnected()) {
    reconnectTimer.once(5, connectToMqtt);
  }
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

  wifiConnectHandler = WiFi.onStationModeGotIP(onWiFiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWiFi();
}

void loop() {
  mqttClient.loop();
}