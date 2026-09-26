#include <Arduino.h>
#include <unity.h>
#include "collection.h"
static Collection collection;
static DynamicJsonDocument doc(2048);
static const char *packet = "41424a3100000000000000000000000000000000000000000000000000000000";
void setUp() { collection = Collection(); doc.clear(); }
void tearDown() {}
void test_public_total_produces_expected_square_without_key() {
  JsonObject m = doc.to<JsonArray>().createNestedObject();
  m["total"] = 256; m["packet"] = packet;
  TEST_ASSERT_EQUAL(1, collection.ingest(109, doc.as<JsonVariantConst>()));
  long square[16];
  const long expected[] = {63,66,71,56,70,57,62,67,58,73,64,61,65,60,59,72};
  TEST_ASSERT_TRUE(wafq(collection.messages[0].total, square));
  TEST_ASSERT_EQUAL_INT32_ARRAY(expected, square, 16);
  TEST_ASSERT_EQUAL_STRING(packet, collection.messages[0].packet.c_str());
}
void test_recollection_deduplicates_but_preserves_source_and_equal_totals() {
  JsonObject m = doc.to<JsonArray>().createNestedObject();
  m["total"] = 256; m["packet"] = packet;
  TEST_ASSERT_EQUAL(1, collection.ingest(109, doc.as<JsonVariantConst>()));
  TEST_ASSERT_EQUAL(0, collection.ingest(109, doc.as<JsonVariantConst>()));
  TEST_ASSERT_EQUAL(1, collection.ingest(108, doc.as<JsonVariantConst>()));
  String different(packet); different.setCharAt(63, '1'); m["packet"] = different;
  TEST_ASSERT_EQUAL(1, collection.ingest(109, doc.as<JsonVariantConst>()));
  TEST_ASSERT_EQUAL(3, collection.count);
}
void test_invalid_batch_does_not_partially_import() {
  JsonArray rows = doc.to<JsonArray>();
  JsonObject good = rows.createNestedObject();
  good["total"] = 256; good["packet"] = packet;
  rows.createNestedObject()["total"] = -1;
  TEST_ASSERT_EQUAL(-1, collection.ingest(109, doc.as<JsonVariantConst>()));
  TEST_ASSERT_EQUAL(0, collection.count);
}
void test_capacity_preserves_latest_in_order_and_counts_evictions() {
  JsonObject m = doc.to<JsonArray>().createNestedObject(); m["packet"] = packet;
  for (unsigned i = 0; i < 45; ++i) {
    m["total"] = 100 + i;
    TEST_ASSERT_EQUAL(1, collection.ingest(i + 1, doc.as<JsonVariantConst>()));
  }
  TEST_ASSERT_EQUAL(40, collection.count);
  TEST_ASSERT_EQUAL(5, collection.evicted);
  TEST_ASSERT_EQUAL(105, collection.messages[0].total);
  TEST_ASSERT_EQUAL(144, collection.messages[39].total);
}
void setup() {
  delay(2000); UNITY_BEGIN();
  RUN_TEST(test_public_total_produces_expected_square_without_key);
  RUN_TEST(test_recollection_deduplicates_but_preserves_source_and_equal_totals);
  RUN_TEST(test_invalid_batch_does_not_partially_import);
  RUN_TEST(test_capacity_preserves_latest_in_order_and_counts_evictions);
  UNITY_END();
}
void loop() {}
