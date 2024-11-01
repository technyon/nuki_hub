#include <unity.h>
#include <thread>
#include <iostream>
#include <espMqttClient.h>  // espMqttClient for Linux also defines millis()

void setUp() {}
void tearDown() {}

espMqttClient mqttClient;
uint32_t onConnectCbId = 1;
uint32_t onDisconnectCbId = 2;
uint32_t onSubscribeCbId = 3;
uint32_t onUnsubscribeCbId = 4;
uint32_t onMessageCbId = 5;
uint32_t onPublishCbId = 6;
std::atomic_bool exitProgram(false);
std::thread t;

//const IPAddress broker(127,0,0,1);
const char* broker = "mqtt";
//const char* broker = "test.mosquitto.org";
const uint16_t broker_port = 1883;

/*

- setup the client with basic settings
- connect to the broker
- successfully connect

*/
void test_connect() {
  std::atomic<bool> onConnectCalledTest(false);
  bool sessionPresentTest = true;
  mqttClient.setServer(broker, broker_port)
            .setCleanSession(true)
            .setKeepAlive(5)
            .onConnect([&](bool sessionPresent) mutable {
              sessionPresentTest = sessionPresent;
              onConnectCalledTest = true;
            }, onConnectCbId);
  mqttClient.connect();
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (onConnectCalledTest) {
      break;
    }
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_TRUE(onConnectCalledTest);
  TEST_ASSERT_FALSE(sessionPresentTest);

  mqttClient.removeOnConnect(onConnectCbId);
}

/*

- keepalive is set at 5 seconds in previous test
- client should stay connected during 2x keepalive period

*/

void test_ping() {
  bool pingTest = true;
  uint32_t start = millis();
  while (millis() - start < 11000) {
    if (mqttClient.disconnected()) {
      pingTest = false;
      break;
    }
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_TRUE(pingTest);
}

/*

- client subscribes to topic
- ack is received from broker

*/

void test_subscribe() {
  std::atomic<bool> subscribeTest(false);
  mqttClient.onSubscribe([&](uint16_t packetId, const espMqttClientTypes::SubscribeReturncode* returncodes, size_t len) mutable {
    (void) packetId;
    if (len == 1 && returncodes[0] == espMqttClientTypes::SubscribeReturncode::QOS0) {
      subscribeTest = true;
    }
  }, onSubscribeCbId);
  mqttClient.subscribe("test/test", 0);
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (subscribeTest) {
      break;
    }
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_TRUE(subscribeTest);

  mqttClient.removeOnSubscribe(onSubscribeCbId);
}

/*

- client publishes using all three qos levels
- all publish get packetID returned > 0 (equal to 1 for qos 0)
- 2 pubacks are received

*/

void test_publish() {
  std::atomic<int> publishSendTest(0);
  mqttClient.onPublish([&](uint16_t packetId) mutable {
    (void) packetId;
    publishSendTest++;
  }, onPublishCbId);
  std::atomic<int> publishReceiveTest(0);
  mqttClient.onMessage([&](const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) mutable {
    (void) properties;
    (void) topic;
    (void) payload;
    (void) len;
    (void) index;
    (void) total;
    publishReceiveTest++;
  }, onMessageCbId);
  uint16_t sendQos0Test = mqttClient.publish("test/test", 0, false, "test0");
  uint16_t sendQos1Test = mqttClient.publish("test/test", 1, false, "test1");
  uint16_t sendQos2Test = mqttClient.publish("test/test", 2, false, "test2");
  uint32_t start = millis();
  while (millis() - start < 6000) {
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_EQUAL_UINT16(1, sendQos0Test);
  TEST_ASSERT_GREATER_THAN_UINT16(0, sendQos1Test);
  TEST_ASSERT_GREATER_THAN_UINT16(0, sendQos2Test);
  TEST_ASSERT_EQUAL_INT(2, publishSendTest);
  TEST_ASSERT_EQUAL_INT(3, publishReceiveTest);

  mqttClient.removeOnPublish(onPublishCbId);
  mqttClient.removeOnMessage(onMessageCbId);
}

void test_publish_empty() {
  std::atomic<int> publishSendEmptyTest(0);
  mqttClient.onPublish([&](uint16_t packetId) mutable {
    (void) packetId;
    publishSendEmptyTest++;
  }, onPublishCbId);
  std::atomic<int> publishReceiveEmptyTest(0);
  mqttClient.onMessage([&](const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) mutable {
    (void) properties;
    (void) topic;
    (void) payload;
    (void) len;
    (void) index;
    (void) total;
    publishReceiveEmptyTest++;
  }, onMessageCbId);
  uint16_t sendQos0Test = mqttClient.publish("test/test", 0, false, nullptr, 0);
  uint16_t sendQos1Test = mqttClient.publish("test/test", 1, false, nullptr, 0);
  uint16_t sendQos2Test = mqttClient.publish("test/test", 2, false, nullptr, 0);
  uint32_t start = millis();
  while (millis() - start < 6000) {
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_EQUAL_UINT16(1, sendQos0Test);
  TEST_ASSERT_GREATER_THAN_UINT16(0, sendQos1Test);
  TEST_ASSERT_GREATER_THAN_UINT16(0, sendQos2Test);
  TEST_ASSERT_EQUAL_INT(2, publishSendEmptyTest);
  TEST_ASSERT_EQUAL_INT(3, publishReceiveEmptyTest);

  mqttClient.removeOnPublish(onPublishCbId);
  mqttClient.removeOnMessage(onMessageCbId);
}

/*

- subscribe to test/test, qos 1
- send to test/test, qos 1
- check if message is received at least once.

*/

void test_receive1() {
  std::atomic<int> publishReceive1Test(0);
  mqttClient.onMessage([&](const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) mutable {
    (void) properties;
    (void) topic;
    (void) payload;
    (void) len;
    (void) index;
    (void) total;
    publishReceive1Test++;
  }, onMessageCbId);
  mqttClient.onSubscribe([&](uint16_t packetId, const espMqttClientTypes::SubscribeReturncode* returncodes, size_t len) mutable {
    (void) packetId;
    if (len == 1 && returncodes[0] == espMqttClientTypes::SubscribeReturncode::QOS1) {
      mqttClient.publish("test/test", 1, false, "");
    }
  }, onSubscribeCbId);
  mqttClient.subscribe("test/test", 1);
  uint32_t start = millis();
  while (millis() - start < 6000) {
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_GREATER_THAN_INT(0, publishReceive1Test);

  mqttClient.removeOnMessage(onMessageCbId);
  mqttClient.removeOnSubscribe(onSubscribeCbId);
}

/*

- subscribe to test/test, qos 2
- send to test/test, qos 2
- check if message is received exactly once.

*/

void test_receive2() {
  std::atomic<int> publishReceive2Test(0);
  mqttClient.onMessage([&](const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) mutable {
    (void) properties;
    (void) topic;
    (void) payload;
    (void) len;
    (void) index;
    (void) total;
    publishReceive2Test++;
  }, onMessageCbId);
  mqttClient.onSubscribe([&](uint16_t packetId, const espMqttClientTypes::SubscribeReturncode* returncodes, size_t len) mutable {
    (void) packetId;
    if (len == 1 && returncodes[0] == espMqttClientTypes::SubscribeReturncode::QOS2) {
      mqttClient.publish("test/test", 2, false, "");
    }
  }, onSubscribeCbId);
  mqttClient.subscribe("test/test", 2);
  uint32_t start = millis();
  while (millis() - start < 6000) {
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_EQUAL_INT(1, publishReceive2Test);

  mqttClient.removeOnMessage(onMessageCbId);
  mqttClient.removeOnSubscribe(onSubscribeCbId);
}


/*

- client unsibscribes from topic

*/

void test_unsubscribe() {
  std::atomic<bool> unsubscribeTest(false);
  mqttClient.onUnsubscribe([&](uint16_t packetId) mutable {
    (void) packetId;
    unsubscribeTest = true;
  }, onUnsubscribeCbId);
  mqttClient.unsubscribe("test/test");
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (unsubscribeTest) {
      break;
    }
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_TRUE(unsubscribeTest);

  mqttClient.removeOnUnsubscribe(onUnsubscribeCbId);
}

/*

- client disconnects cleanly

*/

void test_disconnect() {
  std::atomic<bool> onDisconnectCalled(false);
  espMqttClientTypes::DisconnectReason reasonTest = espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED;
  mqttClient.onDisconnect([&](espMqttClientTypes::DisconnectReason reason) mutable {
    reasonTest = reason;
    onDisconnectCalled = true;
  }, onDisconnectCbId);
  mqttClient.disconnect();
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (onDisconnectCalled) {
      break;
    }
    std::this_thread::yield();
  }

  TEST_ASSERT_TRUE(onDisconnectCalled);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::DisconnectReason::USER_OK, reasonTest);
  TEST_ASSERT_TRUE(mqttClient.disconnected());

  mqttClient.removeOnDisconnect(onDisconnectCbId);
}

void test_pub_before_connect() {
  std::atomic<bool> onConnectCalledTest(false);
  std::atomic<int> publishSendTest(0);
  bool sessionPresentTest = true;
  mqttClient.setServer(broker, broker_port)
            .setCleanSession(true)
            .setKeepAlive(5)
            .onConnect([&](bool sessionPresent) mutable {
              sessionPresentTest = sessionPresent;
              onConnectCalledTest = true;
            }, onConnectCbId)
            .onPublish([&](uint16_t packetId) mutable {
              (void) packetId;
              publishSendTest++;
            }, onPublishCbId);
  uint16_t sendQos0Test = mqttClient.publish("test/test", 0, false, "test0");
  uint16_t sendQos1Test = mqttClient.publish("test/test", 1, false, "test1");
  uint16_t sendQos2Test = mqttClient.publish("test/test", 2, false, "test2");
  mqttClient.connect();
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (onConnectCalledTest) {
      break;
    }
    std::this_thread::yield();
  }
  TEST_ASSERT_TRUE(mqttClient.connected());
  TEST_ASSERT_TRUE(onConnectCalledTest);
  TEST_ASSERT_FALSE(sessionPresentTest);
  start = millis();
  while (millis() - start < 10000) {
    std::this_thread::yield();
  }

  TEST_ASSERT_EQUAL_UINT16(1, sendQos0Test);
  TEST_ASSERT_GREATER_THAN_UINT16(0, sendQos1Test);
  TEST_ASSERT_GREATER_THAN_UINT16(0, sendQos2Test);
  TEST_ASSERT_EQUAL_INT(2, publishSendTest);

  mqttClient.removeOnConnect(onConnectCbId);
  mqttClient.removeOnPublish(onPublishCbId);
}

void final_disconnect() {
  std::atomic<bool> onDisconnectCalled(false);
  mqttClient.onDisconnect([&](espMqttClientTypes::DisconnectReason reason) mutable {
    (void) reason;
    onDisconnectCalled = true;
  }, onDisconnectCbId);
  mqttClient.disconnect();
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (onDisconnectCalled) {
      break;
    }
    std::this_thread::yield();
  }
  if (mqttClient.connected()) {
    mqttClient.disconnect(true);
  }
  mqttClient.removeOnDisconnect(onDisconnectCbId);
}

int main() {
  UNITY_BEGIN();
  t = std::thread([] {
    while (1) {
      mqttClient.loop();
      if (exitProgram) break;
    }
  });
  RUN_TEST(test_connect);
  RUN_TEST(test_ping);
  RUN_TEST(test_subscribe);
  RUN_TEST(test_publish);
  RUN_TEST(test_publish_empty);
  RUN_TEST(test_receive1);
  RUN_TEST(test_receive2);
  RUN_TEST(test_unsubscribe);
  RUN_TEST(test_disconnect);
  RUN_TEST(test_pub_before_connect);
  final_disconnect();
  exitProgram = true;
  t.join();
  return UNITY_END();
}
