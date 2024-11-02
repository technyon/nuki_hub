#include <unity.h>

#include <Outbox.h>

using espMqttClientInternals::Outbox;

void setUp() {}
void tearDown() {}

void test_outbox_create() {
  Outbox<uint32_t> outbox;
  Outbox<uint32_t>::Iterator it = outbox.front();
  TEST_ASSERT_NULL(outbox.getCurrent());
  TEST_ASSERT_NULL(it.get());
  TEST_ASSERT_TRUE(outbox.empty());
}

void test_outbox_emplace() {
  Outbox<uint32_t> outbox;
  outbox.emplace(523);
  // 523, current points to 523
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(523, *(outbox.getCurrent()));
  TEST_ASSERT_FALSE(outbox.empty());

  outbox.next();
  // 523, current points to nullptr
  TEST_ASSERT_NULL(outbox.getCurrent());

  outbox.emplace(286);
  // 523 286, current points to 286
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(286, *(outbox.getCurrent()));

  outbox.emplace(364);
  // 523 286 364, current points to 286
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(286, *(outbox.getCurrent()));
}

void test_outbox_emplaceFront() {
  Outbox<uint32_t> outbox;
  outbox.emplaceFront(1);
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(1, *(outbox.getCurrent()));

  outbox.emplaceFront(2);
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(2, *(outbox.getCurrent()));
}

void test_outbox_remove1() {
  Outbox<uint32_t> outbox;
  Outbox<uint32_t>::Iterator it;
  outbox.emplace(1);
  outbox.emplace(2);
  outbox.emplace(3);
  outbox.emplace(4);
  outbox.next();
  outbox.next();
  it = outbox.front();
  ++it;
  ++it;
  ++it;
  ++it;
  outbox.remove(it);
  // 1 2 3 4, it points to nullptr, current points to 3
  TEST_ASSERT_NULL(it.get());
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(3, *(outbox.getCurrent()));

  it = outbox.front();
  ++it;
  ++it;
  ++it;
  outbox.remove(it);
  // 1 2 3, it points to nullptr, current points to 3
  TEST_ASSERT_NULL(it.get());
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(3, *(outbox.getCurrent()));


  it = outbox.front();
  outbox.remove(it);
  // 2 3, it points to 2, current points to 3
  TEST_ASSERT_NOT_NULL(it.get());
  TEST_ASSERT_EQUAL_UINT32(2, *(it.get()));
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(3, *(outbox.getCurrent()));

  it = outbox.front();
  outbox.remove(it);
  // 3, it points to 3, current points to 3
  TEST_ASSERT_NOT_NULL(it.get());
  TEST_ASSERT_EQUAL_UINT32(3, *(it.get()));
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(3, *(outbox.getCurrent()));

  it = outbox.front();
  outbox.remove(it);
  TEST_ASSERT_NULL(it.get());
  TEST_ASSERT_NULL(outbox.getCurrent());
}

void test_outbox_remove2() {
  Outbox<uint32_t> outbox;
  Outbox<uint32_t>::Iterator it;
  outbox.emplace(1);
  outbox.emplace(2);
  outbox.next();
  outbox.next();
  it = outbox.front();
  // 1 2, current points to nullptr
  TEST_ASSERT_NULL(outbox.getCurrent());
  TEST_ASSERT_NOT_NULL(it.get());
  TEST_ASSERT_EQUAL_UINT32(1, *(it.get()));

  ++it;
  // 1 2, current points to nullptr
  TEST_ASSERT_NOT_NULL(it.get());
  TEST_ASSERT_EQUAL_UINT32(2, *(it.get()));

  outbox.remove(it);
  // 1, current points to nullptr
  TEST_ASSERT_NULL(outbox.getCurrent());
  TEST_ASSERT_NULL(it.get());

  it = outbox.front();
  TEST_ASSERT_NOT_NULL(it.get());
  TEST_ASSERT_EQUAL_UINT32(1, *(it.get()));

  outbox.remove(it);
  TEST_ASSERT_NULL(it.get());
  TEST_ASSERT_TRUE(outbox.empty());
}

void test_outbox_removeCurrent() {
  Outbox<uint32_t> outbox;
  outbox.emplace(1);
  outbox.emplace(2);
  outbox.emplace(3);
  outbox.emplace(4);
  outbox.removeCurrent();
  // 2 3 4, current points to 2
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(2, *(outbox.getCurrent()));

  outbox.next();
  outbox.removeCurrent();
  // 2 4, current points to 4
  TEST_ASSERT_NOT_NULL(outbox.getCurrent());
  TEST_ASSERT_EQUAL_UINT32(4, *(outbox.getCurrent()));

  outbox.removeCurrent();
  // 4, current points to nullptr
  TEST_ASSERT_NULL(outbox.getCurrent());

  // outbox will go out of scope and destructor will be called
  // Valgrind should not detect a leak here
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_outbox_create);
  RUN_TEST(test_outbox_emplace);
  RUN_TEST(test_outbox_emplaceFront);
  RUN_TEST(test_outbox_remove1);
  RUN_TEST(test_outbox_remove2);
  RUN_TEST(test_outbox_removeCurrent);
  return UNITY_END();
}
