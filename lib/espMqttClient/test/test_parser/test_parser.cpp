#include <unity.h>

#include <Packets/Parser.h>

using espMqttClientInternals::Parser;
using espMqttClientInternals::ParserResult;
using espMqttClientInternals::IncomingPacket;

void setUp() {}
void tearDown() {}

Parser parser;

void test_Connack() {
  const uint8_t stream[] = {
    0b00100000,  // header
    0b00000010,  // flags
    0b00000001,  // session present
    0b00000000   // reserved
  };
  const size_t length = 4;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(4, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT8(1, parser.getPacket().variableHeader.fixed.connackVarHeader.sessionPresent);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().variableHeader.fixed.connackVarHeader.returnCode);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_Empty() {
  const uint8_t stream[] = {
    0x00
  };
  const size_t length = 0;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_UINT8(ParserResult::awaitData, result);
  TEST_ASSERT_EQUAL_INT32(0, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_Header() {
  const uint8_t stream[] = {
    0x12,
    0x13,
    0x14
  };
  const size_t length = 3;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::protocolError, result);
  TEST_ASSERT_EQUAL_UINT32(1, bytesRead);
}

void test_Publish() {
  uint8_t stream[] = {
    0b00110010,                 // header
    0x0B,                       // remaining length
    0x00, 0x03, 'a', '/', 'b',  // topic
    0x00, 0x0A,                 // packet id
    0x01, 0x02                  // payload
  };
  size_t length = 11;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PUBLISH, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_STRING("a/b", parser.getPacket().variableHeader.topic);
  TEST_ASSERT_EQUAL_UINT16(10, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT32(0, parser.getPacket().payload.index);
  TEST_ASSERT_EQUAL_UINT32(2, parser.getPacket().payload.length);
  TEST_ASSERT_EQUAL_UINT32(4, parser.getPacket().payload.total);
  TEST_ASSERT_EQUAL_UINT8(1, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());

  stream[0] = 0x03;
  stream[1] = 0x04;
  length = 2;

  bytesRead = 0;
  result = parser.parse(stream, length, &bytesRead);
  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_STRING("a/b", parser.getPacket().variableHeader.topic);
  TEST_ASSERT_EQUAL_UINT16(10, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT32(2, parser.getPacket().payload.index);
  TEST_ASSERT_EQUAL_UINT32(2, parser.getPacket().payload.length);
  TEST_ASSERT_EQUAL_UINT32(4, parser.getPacket().payload.total);
  TEST_ASSERT_EQUAL_UINT8(1, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_Publish_empty() {
  uint8_t stream0[] = {
    0b00110000,                 // header
    0x05,                       // remaining length
    0x00, 0x03, 'a', '/', 'b',  // topic
  };
  size_t length0 = 7;

  size_t bytesRead0 = 0;
  ParserResult result0 = parser.parse(stream0, length0, &bytesRead0);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result0);
  TEST_ASSERT_EQUAL_UINT32(length0, bytesRead0);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PUBLISH, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_STRING("a/b", parser.getPacket().variableHeader.topic);
  TEST_ASSERT_EQUAL_UINT32(0, parser.getPacket().payload.index);
  TEST_ASSERT_EQUAL_UINT32(0, parser.getPacket().payload.length);
  TEST_ASSERT_EQUAL_UINT32(0, parser.getPacket().payload.total);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());

  uint8_t stream1[] = {
    0b00110000,                 // header
    0x05,                       // remaining length
    0x00, 0x03, 'a', '/', 'b',  // topic
  };
  size_t length1 = 7;

  size_t bytesRead1 = 0;
  ParserResult result1 = parser.parse(stream1, length1, &bytesRead1);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result1);
  TEST_ASSERT_EQUAL_UINT32(length1, bytesRead1);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PUBLISH, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_STRING("a/b", parser.getPacket().variableHeader.topic);
  TEST_ASSERT_EQUAL_UINT32(0, parser.getPacket().payload.index);
  TEST_ASSERT_EQUAL_UINT32(0, parser.getPacket().payload.length);
  TEST_ASSERT_EQUAL_UINT32(0, parser.getPacket().payload.total);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());

}

void test_PubAck() {
  const uint8_t stream[] = {
    0b01000000,
    0b00000010,
    0x12,
    0x34
  };
  const size_t length = 4;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PUBACK, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT16(4660, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_PubRec() {
  const uint8_t stream[] = {
    0b01010000,
    0b00000010,
    0x56,
    0x78
  };
  const size_t length = 4;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_BITS(0xF0, espMqttClientInternals::PacketType.PUBREC, parser.getPacket().fixedHeader.packetType);
  TEST_ASSERT_EQUAL_UINT16(22136, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_PubRel() {
  const uint8_t stream[] = {
    0b01100010,
    0b00000010,
    0x9A,
    0xBC
  };
  const size_t length = 4;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PUBREL, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT16(0x9ABC, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_PubComp() {
  const uint8_t stream[] = {
    0b01110000,
    0b00000010,
    0xDE,
    0xF0
  };
  const size_t length = 4;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PUBCOMP, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT16(0xDEF0, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_SubAck() {
  const uint8_t stream[] = {
    0b10010000,
    0b00000100,
    0x00,
    0x0A,
    0x02,
    0x01
  };
  const size_t length = 6;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.SUBACK, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT16(10, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&stream[4], parser.getPacket().payload.data,2);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_UnsubAck() {
  const uint8_t stream[] = {
    0b10110000,
    0b00000010,
    0x00,
    0x0A
  };
  const size_t length = 4;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.UNSUBACK, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT16(10, parser.getPacket().variableHeader.fixed.packetId);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}


void test_PingResp() {
  const uint8_t stream[] = {
    0b11010000,
    0x00
  };
  const size_t length = 2;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(stream, length, &bytesRead);

  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT32(length, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PINGRESP, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

void test_longStream() {
  const uint8_t stream[] = {
    0x90, 0x03, 0x00, 0x01, 0x00, 0x31, 0x0F, 0x00, 0x09, 0x66, 0x6F, 0x6F, 0x2F, 0x62, 0x61, 0x72,
    0x2F, 0x30, 0x74, 0x65, 0x73, 0x74, 0x90, 0x03, 0x00, 0x02, 0x01, 0x33, 0x11, 0x00, 0x09, 0x66,
    0x6F, 0x6F, 0x2F, 0x62, 0x61, 0x72, 0x2F, 0x31, 0x00, 0x01, 0x74, 0x65, 0x73, 0x74, 0x90, 0x03,
    0x00, 0x03, 0x02, 0x30, 0x0F, 0x00, 0x09, 0x66, 0x6F, 0x6F, 0x2F, 0x62, 0x61, 0x72, 0x2F, 0x30,
    0x74, 0x65, 0x73, 0x74, 0x32, 0x11, 0x00, 0x09, 0x66, 0x6F, 0x6F, 0x2F, 0x62, 0x61, 0x72, 0x2F,
    0x31, 0x00, 0x02, 0x74, 0x65, 0x73, 0x74, 0x40, 0x02, 0x00, 0x04, 0x50, 0x02, 0x00, 0x05
  };
  const size_t length = 94;

  size_t bytesRead = 0;
  ParserResult result = parser.parse(&stream[bytesRead], length - bytesRead, &bytesRead);
  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.SUBACK, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT32(5, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());

  result = parser.parse(&stream[bytesRead], length - bytesRead, &bytesRead);
  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.PUBLISH, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT32(5 + 17, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_TRUE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());

  result = parser.parse(&stream[bytesRead], length - bytesRead, &bytesRead);
  TEST_ASSERT_EQUAL_INT32(ParserResult::packet, result);
  TEST_ASSERT_EQUAL_UINT8(espMqttClientInternals::PacketType.SUBACK, parser.getPacket().fixedHeader.packetType & 0xF0);
  TEST_ASSERT_EQUAL_UINT32(5 + 17 + 5, bytesRead);
  TEST_ASSERT_EQUAL_UINT8(0, parser.getPacket().qos());
  TEST_ASSERT_FALSE(parser.getPacket().retain());
  TEST_ASSERT_FALSE(parser.getPacket().dup());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_Connack);
  RUN_TEST(test_Empty);
  RUN_TEST(test_Header);
  RUN_TEST(test_Publish);
  RUN_TEST(test_Publish_empty);
  RUN_TEST(test_PubAck);
  RUN_TEST(test_PubRec);
  RUN_TEST(test_PubRel);
  RUN_TEST(test_PubComp);
  RUN_TEST(test_SubAck);
  RUN_TEST(test_UnsubAck);
  RUN_TEST(test_PingResp);
  RUN_TEST(test_longStream);
  return UNITY_END();
}
