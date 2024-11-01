#include <iostream>

#include <unity.h>

#include <Packets/StringUtil.h>

void setUp() {}
void tearDown() {}

void test_encodeString() {
  const char test[] = "abcd";
  uint8_t buffer[6];
  const uint8_t check[] = {0x00, 0x04, 'a', 'b', 'c', 'd'};
  const uint32_t length = 6;

  TEST_ASSERT_EQUAL_UINT32(length, espMqttClientInternals::encodeString(test, buffer));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, buffer, length);
}

void test_emtpyString() {
  const char test[] = "";
  uint8_t buffer[2];
  const uint8_t check[] = {0x00, 0x00};
  const uint32_t length = 2;

  TEST_ASSERT_EQUAL_UINT32(length, espMqttClientInternals::encodeString(test, buffer));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, buffer, length);
}

void test_longString() {
  const size_t maxSize = 65535;
  char test[maxSize + 1];
  test[maxSize] = '\0';
  memset(test, 'a', maxSize);
  uint8_t buffer[maxSize + 3];
  uint8_t check[maxSize + 2];
  check[0] = 0xFF;
  check[1] = 0xFF;
  memset(&check[2], 'a', maxSize);
  const uint32_t length = 2 + maxSize;

  TEST_ASSERT_EQUAL_UINT32(length, espMqttClientInternals::encodeString(test, buffer));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(check, buffer, length);
}

void test_tooLongString() {
  const size_t maxSize = 65535;
  char test[maxSize + 2];
  test[maxSize + 1] = '\0';
  memset(test, 'a', maxSize + 1);
  uint8_t buffer[maxSize + 4];  // extra 4 bytes for headroom: test progam, don't test test
  const uint32_t length = 0;

  TEST_ASSERT_EQUAL_UINT32(length, espMqttClientInternals::encodeString(test, buffer));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_encodeString);
  RUN_TEST(test_emtpyString);
  RUN_TEST(test_longString);
  RUN_TEST(test_tooLongString);
  return UNITY_END();
}
