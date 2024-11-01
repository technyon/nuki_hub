#include <iostream>

#include <unity.h>

#include <Packets/RemainingLength.h>

void setUp() {}
void tearDown() {}

// Examples takes from MQTT specification
uint8_t bytes1[] = {0x40};
uint8_t size1 = 1;
uint32_t length1 = 64;

uint8_t bytes2[] = {193, 2};
uint8_t size2 = 2;
uint32_t length2 = 321;

uint8_t bytes3[] = {0xff, 0xff, 0xff, 0x7f};
uint8_t size3 = 4;
uint32_t length3 = 268435455;

void test_remainingLengthDecode() {
  TEST_ASSERT_EQUAL_INT32(length1, espMqttClientInternals::decodeRemainingLength(bytes1));
  TEST_ASSERT_EQUAL_INT32(length2, espMqttClientInternals::decodeRemainingLength(bytes2));

  uint8_t stream[] = {0x80, 0x80, 0x80, 0x01};
  TEST_ASSERT_EQUAL_INT32(2097152 , espMqttClientInternals::decodeRemainingLength(stream));

  TEST_ASSERT_EQUAL_INT32(length3, espMqttClientInternals::decodeRemainingLength(bytes3));
}

void test_remainingLengthEncode() {
  uint8_t bytes[4];

  TEST_ASSERT_EQUAL_UINT8(1, espMqttClientInternals::remainingLengthLength(0));

  TEST_ASSERT_EQUAL_UINT8(size1, espMqttClientInternals::remainingLengthLength(length1));
  TEST_ASSERT_EQUAL_UINT8(size1, espMqttClientInternals::encodeRemainingLength(length1, bytes));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes1, bytes, size1);
  TEST_ASSERT_EQUAL_UINT8(size2, espMqttClientInternals::remainingLengthLength(length2));
  TEST_ASSERT_EQUAL_UINT8(size2, espMqttClientInternals::encodeRemainingLength(length2, bytes));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes2, bytes, size2);
  TEST_ASSERT_EQUAL_UINT8(size3, espMqttClientInternals::remainingLengthLength(length3));
  TEST_ASSERT_EQUAL_UINT8(size3, espMqttClientInternals::encodeRemainingLength(length3, bytes));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes3, bytes, size3);
}

void test_remainingLengthError() {
  uint8_t bytes[] = {0xff, 0xff, 0xff, 0x80};  // high bit of last byte is 1
                                               // this indicates a next byte is coming
                                               // which is a violation of the spec
  TEST_ASSERT_EQUAL_UINT8(0, espMqttClientInternals::remainingLengthLength(268435456));
  TEST_ASSERT_EQUAL_INT32(-1, espMqttClientInternals::decodeRemainingLength(bytes));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_remainingLengthDecode);
  RUN_TEST(test_remainingLengthEncode);
  RUN_TEST(test_remainingLengthError);
  return UNITY_END();
}
