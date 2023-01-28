#include <iostream>
#include <thread>
#include <espMqttClient.h>

#define MQTT_HOST IPAddress(192,168,1,10)
#define MQTT_PORT 1883

espMqttClient mqttClient;
std::atomic_bool exitProgram(false);

void connectToMqtt() {
  std::cout << "Connecting to MQTT..." << std::endl;
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  std::cout << "Connected to MQTT." << std::endl;
  std::cout << "Session present: " << sessionPresent << std::endl;
  uint16_t packetIdSub = mqttClient.subscribe("test/lol", 2);
  std::cout << "Subscribing at QoS 2, packetId: " << packetIdSub << std::endl;
  mqttClient.publish("test/lol", 0, true, "test 1");
  std::cout << "Publishing at QoS 0" << std::endl;
  uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
  std::cout << "Publishing at QoS 1, packetId: " << packetIdPub1 << std::endl;
  uint16_t packetIdPub2 = mqttClient.publish("test/lol", 2, true, "test 3");
  std::cout << "Publishing at QoS 2, packetId: " << packetIdPub2 << std::endl;
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  std::cout << "Disconnected from MQTT: %u.\n" << unsigned(static_cast<uint8_t>(reason)) << std::endl;
  exitProgram = true;
}

void onMqttSubscribe(uint16_t packetId, const espMqttClientTypes::SubscribeReturncode* codes, size_t len) {
  std::cout << "Subscribe acknowledged." << std::endl;
  std::cout << "  packetId: " << packetId << std::endl;
  for (size_t i = 0; i < len; ++i) {
    std::cout << "  qos: " << unsigned(static_cast<uint8_t>(codes[i])) << std::endl;
  }
}

void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  (void) payload;
  std::cout << "Publish received." << std::endl;
  std::cout << "  topic: " << topic << std::endl;
  std::cout << "  qos: " << unsigned(properties.qos) << std::endl;
  std::cout << "  dup: " << properties.dup << std::endl;
  std::cout << "  retain: " << properties.retain << std::endl;
  std::cout << "  len: " << len << std::endl;
  std::cout << "  index: " << index << std::endl;
  std::cout << "  total: " << total << std::endl;
}

void onMqttPublish(uint16_t packetId) {
  std::cout << "Publish acknowledged." << std::endl;
  std::cout << "  packetId: " << packetId << std::endl;
}

void ClientLoop(void* arg) {
  (void) arg;
  for(;;) {
    mqttClient.loop();  // includes a yield
    if (exitProgram) break;
  }
}

int main() {
  std::cout << "Setting up sample MQTT client" << std::endl;

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  std::cout << "Starting sample MQTT client" << std::endl;
  std::thread t = std::thread(ClientLoop, nullptr);

  connectToMqtt();
  
  while(1) {
    if (exitProgram) break;
    std::this_thread::yield();
  }
  
  t.join();
  return EXIT_SUCCESS;
}
