#include <unity.h>

#include <Packets/Packet.h>

using espMqttClientInternals::Packet;
using espMqttClientInternals::PacketType;

void setUp() {}
void tearDown() {}

void test_encodeConnect0() {
  const uint8_t check[] = {
    0b00010000,                 // header
    0x0F,                       // remaining length
    0x00,0x04,'M','Q','T','T',  // protocol
    0b00000100,                 // protocol level
    0b00000010,                 // connect flags
    0x00,0x10,                  // keepalive (16)
    0x00,0x03,'c','l','i'       // client id
  };
  const uint32_t length = 17;

  bool cleanSession = true;
  const char* username = nullptr;
  const char* password = nullptr;
  const char* willTopic = nullptr;
  bool willRemain = false;
  uint8_t willQoS = 0;
  const uint8_t* willPayload = nullptr;
  uint16_t willPayloadLength = 0;
  uint16_t keepalive = 16;
  const char* clientId = "cli";
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error,
                cleanSession,
                username,
                password,
                willTopic,
                willRemain,
                willQoS,
                willPayload,
                willPayloadLength,
                keepalive,
                clientId);

  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.CONNECT, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(0, packet.packetId());
}

void test_encodeConnect1() {
  const uint8_t check[] = {
    0b00010000,                 // header
    0x20,                       // remaining length
    0x00,0x04,'M','Q','T','T',  // protocol
    0b00000100,                 // protocol level
    0b11101110,                 // connect flags
    0x00,0x10,                  // keepalive (16)
    0x00,0x03,'c','l','i',      // client id
    0x00,0x03,'t','o','p',      // will topic
    0x00,0x02,'p','l',          // will payload
    0x00,0x02,'u','n',          // username
    0x00,0x02,'p','a'           // password
  };
  const uint32_t length = 34;

  bool cleanSession = true;
  const char* username = "un";
  const char* password = "pa";
  const char* willTopic = "top";
  bool willRemain = true;
  uint8_t willQoS = 1;
  const uint8_t willPayload[] = {'p', 'l'};
  uint16_t willPayloadLength = 2;
  uint16_t keepalive = 16;
  const char* clientId = "cli";
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error,
                cleanSession,
                username,
                password,
                willTopic,
                willRemain,
                willQoS,
                willPayload,
                willPayloadLength,
                keepalive,
                clientId);

  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.CONNECT, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(0, packet.packetId());
}

void test_encodeConnect2() {
  const uint8_t check[] = {
    0b00010000,                 // header
    0x20,                       // remaining length
    0x00,0x04,'M','Q','T','T',  // protocol
    0b00000100,                 // protocol level
    0b11110110,                 // connect flags
    0x00,0x10,                  // keepalive (16)
    0x00,0x03,'c','l','i',      // client id
    0x00,0x03,'t','o','p',      // will topic
    0x00,0x02,'p','l',          // will payload
    0x00,0x02,'u','n',          // username
    0x00,0x02,'p','a'           // password
  };
  const uint32_t length = 34;

  bool cleanSession = true;
  const char* username = "un";
  const char* password = "pa";
  const char* willTopic = "top";
  bool willRemain = true;
  uint8_t willQoS = 2;
  const uint8_t willPayload[] = {'p', 'l', '\0'};
  uint16_t willPayloadLength = 0;
  uint16_t keepalive = 16;
  const char* clientId = "cli";
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error,
                cleanSession,
                username,
                password,
                willTopic,
                willRemain,
                willQoS,
                willPayload,
                willPayloadLength,
                keepalive,
                clientId);

  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.CONNECT, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(0, packet.packetId());
}

void test_encodeConnectFail0() {
  bool cleanSession = true;
  const char* username = nullptr;
  const char* password = nullptr;
  const char* willTopic = nullptr;
  bool willRemain = false;
  uint8_t willQoS = 0;
  const uint8_t* willPayload = nullptr;
  uint16_t willPayloadLength = 0;
  uint16_t keepalive = 16;
  const char* clientId = "";
  espMqttClientTypes::Error error = espMqttClientTypes::Error::SUCCESS;

  Packet packet(error,
                cleanSession,
                username,
                password,
                willTopic,
                willRemain,
                willQoS,
                willPayload,
                willPayloadLength,
                keepalive,
                clientId);

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::MALFORMED_PARAMETER, error);
}

void test_encodePublish0() {
  const uint8_t check[] = {
    0b00110000,                 // header, dup, qos, retain
    0x09,
    0x00,0x03,'t','o','p',      // topic
    0x01,0x02,0x03,0x04         // payload
  };
  const uint32_t length = 11;

  const char* topic = "top";
  uint8_t qos = 0;
  bool retain = false;
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  uint16_t payloadLength = 4;
  uint16_t packetId = 22; // any value except 0 for testing
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error,
                packetId,
                topic,
                payload,
                payloadLength,
                qos,
                retain);

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PUBLISH, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(0, packet.packetId());

  packet.setDup();
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
}

void test_encodePublish1() {
  const uint8_t check[] = {
    0b00110011,                 // header, dup, qos, retain
    0x0B,
    0x00,0x03,'t','o','p',      // topic
    0x00,0x16,                  // packet Id
    0x01,0x02,0x03,0x04         // payload
  };
  const uint32_t length = 13;

  const char* topic = "top";
  uint8_t qos = 1;
  bool retain = true;
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  uint16_t payloadLength = 4;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error,
                packetId,
                topic,
                payload,
                payloadLength,
                qos,
                retain);

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PUBLISH, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());

  const uint8_t checkDup[] = {
    0b00111011,                 // header, dup, qos, retain
    0x0B,
    0x00,0x03,'t','o','p',      // topic
    0x00,0x16,                  // packet Id
    0x01,0x02,0x03,0x04         // payload
  };

  packet.setDup();
  TEST_ASSERT_EQUAL_UINT8_ARRAY(checkDup, packet.data(0), length);
}

void test_encodePublish2() {
  const uint8_t check[] = {
    0b00110101,                 // header, dup, qos, retain
    0x0B,
    0x00,0x03,'t','o','p',      // topic
    0x00,0x16,                  // packet Id
    0x01,0x02,0x03,0x04         // payload
  };
  const uint32_t length = 13;

  const char* topic = "top";
  uint8_t qos = 2;
  bool retain = true;
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  uint16_t payloadLength = 4;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error,
                packetId,
                topic,
                payload,
                payloadLength,
                qos,
                retain);

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PUBLISH, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());

  const uint8_t checkDup[] = {
    0b00111101,                 // header, dup, qos, retain
    0x0B,
    0x00,0x03,'t','o','p',      // topic
    0x00,0x16,                  // packet Id
    0x01,0x02,0x03,0x04         // payload
  };

  packet.setDup();
  TEST_ASSERT_EQUAL_UINT8_ARRAY(checkDup, packet.data(0), length);
}

void test_encodePubAck() {
  const uint8_t check[] = {
    0b01000000,                 // header
    0x02,
    0x00,0x16,                  // packet Id
  };
  const uint32_t length = 4;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, PacketType.PUBACK, packetId);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PUBACK, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodePubRec() {
  const uint8_t check[] = {
    0b01010000,                 // header
    0x02,
    0x00,0x16,                  // packet Id
  };
  const uint32_t length = 4;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, PacketType.PUBREC, packetId);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PUBREC, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodePubRel() {
  const uint8_t check[] = {
    0b01100010,                 // header
    0x02,
    0x00,0x16,                  // packet Id
  };
  const uint32_t length = 4;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, PacketType.PUBREL, packetId);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PUBREL, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodePubComp() {
  const uint8_t check[] = {
    0b01110000,                 // header
    0x02,                       // remaining length
    0x00,0x16,                  // packet Id
  };
  const uint32_t length = 4;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, PacketType.PUBCOMP, packetId);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PUBCOMP, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodeSubscribe() {
  const uint8_t check[] = {
    0b10000010,                 // header
    0x08,                       // remaining length
    0x00,0x16,                  // packet Id
    0x00, 0x03, 'a', '/', 'b',  // topic
    0x02                        // qos
  };
  const uint32_t length = 10;
  const char* topic = "a/b";
  uint8_t qos = 2;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, packetId, topic, qos);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.SUBSCRIBE, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodeMultiSubscribe2() {
  const uint8_t check[] = {
    0b10000010,                 // header
    0x0E,                       // remaining length
    0x00,0x16,                  // packet Id
    0x00, 0x03, 'a', '/', 'b',  // topic1
    0x01,                       // qos1
    0x00, 0x03, 'c', '/', 'd',  // topic2
    0x02                        // qos2
  };
  const uint32_t length = 16;
  const char* topic1 = "a/b";
  const char* topic2 = "c/d";
  uint8_t qos1 = 1;
  uint8_t qos2 = 2;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, packetId, topic1, qos1, topic2, qos2);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.SUBSCRIBE, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodeMultiSubscribe3() {
  const uint8_t check[] = {
    0b10000010,                 // header
    0x14,                       // remaining length
    0x00,0x16,                  // packet Id
    0x00, 0x03, 'a', '/', 'b',  // topic1
    0x01,                       // qos1
    0x00, 0x03, 'c', '/', 'd',  // topic2
    0x02,                       // qos2
    0x00, 0x03, 'e', '/', 'f',  // topic3
    0x00                        // qos3
  };
  const uint32_t length = 22;
  const char* topic1 = "a/b";
  const char* topic2 = "c/d";
  const char* topic3 = "e/f";
  uint8_t qos1 = 1;
  uint8_t qos2 = 2;
  uint8_t qos3 = 0;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, packetId, topic1, qos1, topic2, qos2, topic3, qos3);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.SUBSCRIBE, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodeUnsubscribe() {
  const uint8_t check[] = {
    0b10100010,                 // header
    0x07,                       // remaining length
    0x00,0x16,                  // packet Id
    0x00, 0x03, 'a', '/', 'b',  // topic
  };
  const uint32_t length = 9;
  const char* topic = "a/b";
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, packetId, topic);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.UNSUBSCRIBE, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodeMultiUnsubscribe2() {
  const uint8_t check[] = {
    0b10100010,                 // header
    0x0C,                       // remaining length
    0x00,0x16,                  // packet Id
    0x00, 0x03, 'a', '/', 'b',  // topic1
    0x00, 0x03, 'c', '/', 'd'  // topic2
  };
  const uint32_t length = 14;
  const char* topic1 = "a/b";
  const char* topic2 = "c/d";
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, packetId, topic1, topic2);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.UNSUBSCRIBE, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodeMultiUnsubscribe3() {
  const uint8_t check[] = {
    0b10100010,                 // header
    0x11,                       // remaining length
    0x00,0x16,                  // packet Id
    0x00, 0x03, 'a', '/', 'b',  // topic1
    0x00, 0x03, 'c', '/', 'd',  // topic2
    0x00, 0x03, 'e', '/', 'f',  // topic3
  };
  const uint32_t length = 19;
  const char* topic1 = "a/b";
  const char* topic2 = "c/d";
  const char* topic3 = "e/f";
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, packetId, topic1, topic2, topic3);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.UNSUBSCRIBE, packet.packetType());
  TEST_ASSERT_FALSE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());
}

void test_encodePingReq() {
  const uint8_t check[] = {
    0b11000000,                 // header
    0x00
  };
  const uint32_t length = 2;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, PacketType.PINGREQ);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.PINGREQ, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(0, packet.packetId());
}

void test_encodeDisconnect() {
  const uint8_t check[] = {
    0b11100000,                 // header
    0x00
  };
  const uint32_t length = 2;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error, PacketType.DISCONNECT);
  packet.setDup();  // no effect

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(length, packet.size());
  TEST_ASSERT_EQUAL_UINT8(PacketType.DISCONNECT, packet.packetType());
  TEST_ASSERT_TRUE(packet.removable());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(0), length);
  TEST_ASSERT_EQUAL_UINT16(0, packet.packetId());
}

size_t getData(uint8_t* dest, size_t len, size_t index) {
  (void) index;
  static uint8_t i = 1;
  memset(dest, i, len);
  ++i;
  return len;
}

void test_encodeChunkedPublish() {
  const uint8_t check[] = {
    0b00110011,                 // header, dup, qos, retain
    0xCF, 0x01,                 // 7 + 200 = (0x4F * 1) & 0x40 + (0x01 * 128)
    0x00,0x03,'t','o','p',      // topic
    0x00,0x16                   // packet Id
  };
  uint8_t payloadChunk[EMC_TX_BUFFER_SIZE] = {};
  memset(payloadChunk, 0x01, EMC_TX_BUFFER_SIZE);
  const char* topic = "top";
  uint8_t qos = 1;
  bool retain = true;
  size_t headerLength = 10;
  size_t payloadLength = 200;
  size_t size = headerLength + payloadLength;
  uint16_t packetId = 22;
  espMqttClientTypes::Error error = espMqttClientTypes::Error::MISC_ERROR;

  Packet packet(error,
                packetId,
                topic,
                getData,
                payloadLength,
                qos,
                retain);

  TEST_ASSERT_EQUAL_UINT8(espMqttClientTypes::Error::SUCCESS, error);
  TEST_ASSERT_EQUAL_UINT32(size, packet.size());
  TEST_ASSERT_EQUAL_UINT16(packetId, packet.packetId());

  size_t available = 0;
  size_t index = 0;

  // call 'available' before 'data'
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(headerLength + EMC_TX_BUFFER_SIZE, available);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, packet.data(index), headerLength);

  // index == first payload byte
  index = headerLength;
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(EMC_TX_BUFFER_SIZE, available);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payloadChunk, packet.data(index), available);

  // index == first payload byte
  index = headerLength + 4;
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(EMC_TX_BUFFER_SIZE - 4, available);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payloadChunk, packet.data(index), available);

  // index == last payload byte in first chunk
  index = headerLength + EMC_TX_BUFFER_SIZE - 1;
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(1, available);

  // index == first payloadbyte in second chunk
  memset(payloadChunk, 0x02, EMC_TX_BUFFER_SIZE);
  index = headerLength + EMC_TX_BUFFER_SIZE;
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(EMC_TX_BUFFER_SIZE, available);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payloadChunk, packet.data(index), available);

  memset(payloadChunk, 0x03, EMC_TX_BUFFER_SIZE);
  index = headerLength + EMC_TX_BUFFER_SIZE + EMC_TX_BUFFER_SIZE + 10;
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(EMC_TX_BUFFER_SIZE, available);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payloadChunk, packet.data(index), available);

  const uint8_t checkDup[] = {
    0b00111011,                 // header, dup, qos, retain
    0xCF, 0x01,                 // 7 + 200 = (0x4F * 0) + (0x01 * 128)
    0x00,0x03,'t','o','p',      // topic
    0x00,0x16,                  // packet Id
  };

  index = 0;
  packet.setDup();
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(headerLength + EMC_TX_BUFFER_SIZE, available);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(checkDup, packet.data(index), headerLength);

  memset(payloadChunk, 0x04, EMC_TX_BUFFER_SIZE);
  index = headerLength;
  available = packet.available(index);
  TEST_ASSERT_EQUAL_UINT32(EMC_TX_BUFFER_SIZE, available);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payloadChunk, packet.data(index), available);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_encodeConnect0);
  RUN_TEST(test_encodeConnect1);
  RUN_TEST(test_encodeConnect2);
  RUN_TEST(test_encodeConnectFail0);
  RUN_TEST(test_encodePublish0);
  RUN_TEST(test_encodePublish1);
  RUN_TEST(test_encodePublish2);
  RUN_TEST(test_encodePubAck);
  RUN_TEST(test_encodePubRec);
  RUN_TEST(test_encodePubRel);
  RUN_TEST(test_encodePubComp);
  RUN_TEST(test_encodeSubscribe);
  RUN_TEST(test_encodeMultiSubscribe2);
  RUN_TEST(test_encodeMultiSubscribe3);
  RUN_TEST(test_encodeUnsubscribe);
  RUN_TEST(test_encodeMultiUnsubscribe2);
  RUN_TEST(test_encodeMultiUnsubscribe3);
  RUN_TEST(test_encodePingReq);
  RUN_TEST(test_encodeDisconnect);
  RUN_TEST(test_encodeChunkedPublish);
  return UNITY_END();
}
