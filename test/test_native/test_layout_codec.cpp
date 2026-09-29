#include <unity.h>

#include <cstring>

#include <ArduinoJson.h>

#include "../../src/config/LayoutCodec.h"

using homepoint::config::LayoutCodec;
using homepoint::config::LayoutDocument;

void test_layout_codec_accepts_named_layout_with_spaces() {
  const char* json = R"json({
    "kind":"homepoint-layout",
    "schemaVersion":1,
    "name":"  Ground Floor / Office  ",
    "tiles":[
      {"id":"lamp","type":"switch","name":"Desk Lamp","getTopic":"lamp/state","setTopic":"lamp/set"}
    ]
  })json";
  LayoutDocument layout;
  String error;
  TEST_ASSERT_TRUE_MESSAGE(
      LayoutCodec::parse(json, std::strlen(json), layout, error), error.c_str());
  TEST_ASSERT_EQUAL_STRING("Ground Floor / Office", layout.name.c_str());
  TEST_ASSERT_EQUAL_UINT32(1, layout.tiles.size());

  String encoded;
  TEST_ASSERT_TRUE_MESSAGE(LayoutCodec::serialize(layout, encoded, error), error.c_str());
  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, encoded));
  TEST_ASSERT_EQUAL_STRING("homepoint-layout", document["kind"].as<const char*>());
  TEST_ASSERT_EQUAL(1, document["schemaVersion"].as<int>());
  TEST_ASSERT_EQUAL_STRING("Ground Floor / Office", document["name"].as<const char*>());
}

void test_layout_codec_rejects_invalid_identity_and_name() {
  LayoutDocument layout;
  String error;
  const char* missingName = R"json({"kind":"homepoint-layout","schemaVersion":1,"tiles":[]})json";
  TEST_ASSERT_FALSE(LayoutCodec::parse(missingName, std::strlen(missingName), layout, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), "name"));

  const char* wrongKind = R"json({"kind":"layout","schemaVersion":1,"name":"X","tiles":[]})json";
  TEST_ASSERT_FALSE(LayoutCodec::parse(wrongKind, std::strlen(wrongKind), layout, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), "kind"));
}

void test_layout_filename_contract() {
  TEST_ASSERT_TRUE(LayoutCodec::validFilename("layout_ground_floor.json"));
  TEST_ASSERT_TRUE(LayoutCodec::validFilename("layout_Office-2.json"));
  TEST_ASSERT_FALSE(LayoutCodec::validFilename("layout_ground floor.json"));
  TEST_ASSERT_FALSE(LayoutCodec::validFilename("ground_floor.json"));
  TEST_ASSERT_FALSE(LayoutCodec::validFilename("layout_../secret.json"));
  TEST_ASSERT_FALSE(LayoutCodec::validFilename("layout_x.lastgood.json"));
}
