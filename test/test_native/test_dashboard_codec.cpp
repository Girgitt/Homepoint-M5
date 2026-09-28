#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include <ArduinoJson.h>

#include "../../src/config/DashboardCodec.h"

using homepoint::config::DashboardCodec;
using homepoint::config::DashboardSource;
using homepoint::model::AppConfig;
using homepoint::model::Tile;
using homepoint::model::TileItem;
using homepoint::model::TileItemType;
using homepoint::model::TileType;

namespace {

bool parse(const char* json, AppConfig& config, String& error) {
  return DashboardCodec::parseRequest(json, std::strlen(json), config, error);
}

TileItem switchItem(const char* name, const char* prefix) {
  TileItem item;
  item.type = TileItemType::Switch;
  item.switchDevice.name = name;
  std::string get = std::string(prefix) + "/state";
  std::string set = std::string(prefix) + "/set";
  item.switchDevice.getTopic = get.c_str();
  item.switchDevice.setTopic = set.c_str();
  item.switchDevice.onValue = "ON";
  item.switchDevice.offValue = "OFF";
  return item;
}

}  // namespace

void test_dashboard_codec_accepts_and_normalizes_complete_draft() {
  const char* json = R"json({
    "schemaVersion": 2,
    "dashboard": {"tiles": [
      {"id":"lamp","type":"switch","name":"Lamp","getTopic":"lamp/state","setTopic":"lamp/set"},
      {"id":"temp","type":"sensor","name":"Temp","getTopic":"temp/state","jsondata":true,"sensorType":"combinedValues","firstKey":"t","secondKey":"h"},
      {"id":"room","type":"scene","name":"Room","items":[
        {"type":"switch","name":"A","getTopic":"a/state","setTopic":"a/set"},
        {"type":"switch","name":"B","getTopic":"b/state","setTopic":"b/set"}
      ]}
    ]},
    "capabilities": {"ignored": true},
    "source": {"ignored": true}
  })json";

  AppConfig config;
  String error;
  TEST_ASSERT_TRUE_MESSAGE(parse(json, config, error), error.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, config.tiles.size());
  TEST_ASSERT_EQUAL_STRING("lamp", config.tiles[0].key.c_str());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileType::Switch), static_cast<int>(config.tiles[0].type));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileType::Sensor), static_cast<int>(config.tiles[1].type));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileType::Scene), static_cast<int>(config.tiles[2].type));
  TEST_ASSERT_EQUAL_UINT32(2, config.tiles[2].items.size());

  String normalized;
  TEST_ASSERT_TRUE_MESSAGE(
      DashboardCodec::serialize(config, DashboardSource::Draft, normalized, error), error.c_str());
  AppConfig reparsed;
  TEST_ASSERT_TRUE_MESSAGE(
      DashboardCodec::parseRequest(
          normalized.c_str(), normalized.length(), reparsed, error),
      error.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, reparsed.tiles.size());
}

void test_dashboard_codec_rejects_unknown_sensor_type() {
  const char* json = R"json({"schemaVersion":2,"dashboard":{"tiles":[
    {"type":"sensor","name":"T","getTopic":"t","sensorType":"combinedValue"}
  ]}})json";
  AppConfig config;
  String error;
  TEST_ASSERT_FALSE(parse(json, config, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), "sensorType"));
}

void test_dashboard_codec_rejects_incompatible_json_types() {
  const char* badBool = R"json({"schemaVersion":2,"dashboard":{"tiles":[
    {"type":"sensor","getTopic":"t","jsondata":"true"}
  ]}})json";
  AppConfig config;
  String error;
  TEST_ASSERT_FALSE(parse(badBool, config, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), "boolean"));

  const char* badId = R"json({"schemaVersion":2,"dashboard":{"tiles":[
    {"id":12,"type":"switch","getTopic":"a","setTopic":"b"}
  ]}})json";
  TEST_ASSERT_FALSE(parse(badId, config, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), ".id must be a string"));
}

void test_dashboard_codec_rejects_duplicate_ids_and_short_scenes() {
  const char* duplicate = R"json({"schemaVersion":2,"dashboard":{"tiles":[
    {"id":"x","type":"switch","getTopic":"a","setTopic":"b"},
    {"id":"x","type":"switch","getTopic":"c","setTopic":"d"}
  ]}})json";
  AppConfig config;
  String error;
  TEST_ASSERT_FALSE(parse(duplicate, config, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), "Duplicate tile id"));

  const char* shortScene = R"json({"schemaVersion":2,"dashboard":{"tiles":[
    {"type":"scene","name":"Only one","items":[
      {"type":"switch","getTopic":"a","setTopic":"b"}
    ]}
  ]}})json";
  TEST_ASSERT_FALSE(parse(shortScene, config, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), "at least two"));
}

void test_dashboard_codec_refuses_lossy_legacy_scene() {
  AppConfig config;
  config.loadedFromLegacyScenes = true;
  Tile tile;
  tile.type = TileType::Scene;
  tile.name = "Desk";
  tile.icon = "room";
  tile.items.push_back(switchItem("Desk Lamp", "desk"));
  config.tiles.push_back(tile);

  String output;
  String error;
  TEST_ASSERT_FALSE(DashboardCodec::serialize(config, DashboardSource::Active, output, error));
  TEST_ASSERT_NOT_NULL(std::strstr(error.c_str(), "cannot be represented losslessly"));
}

void test_dashboard_codec_serializes_lossless_legacy_scene_with_warning() {
  AppConfig config;
  config.loadedFromLegacyScenes = true;
  Tile tile;
  tile.type = TileType::Scene;
  tile.name = "Living Room";
  tile.icon = "livingroom";
  tile.items.push_back(switchItem("Ceiling", "ceiling"));
  tile.items.push_back(switchItem("Floor", "floor"));
  config.tiles.push_back(tile);

  String output;
  String error;
  TEST_ASSERT_TRUE_MESSAGE(
      DashboardCodec::serialize(config, DashboardSource::Active, output, error), error.c_str());

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, output.c_str(), output.length()));
  TEST_ASSERT_TRUE(doc["source"]["legacy"].as<bool>());
  TEST_ASSERT_EQUAL_STRING(
      "active", doc["source"]["configuration"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(1, doc["warnings"].as<JsonArrayConst>().size());
  TEST_ASSERT_EQUAL_UINT32(
      2, doc["dashboard"]["tiles"][0]["items"].as<JsonArrayConst>().size());
}

void test_dashboard_codec_marks_last_good_recovery() {
  AppConfig config;
  String output;
  String error;
  TEST_ASSERT_TRUE_MESSAGE(
      DashboardCodec::serialize(config, DashboardSource::LastGood, output, error), error.c_str());
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, output.c_str(), output.length()));
  TEST_ASSERT_EQUAL_STRING(
      "lastGood", doc["source"]["configuration"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(1, doc["warnings"].as<JsonArrayConst>().size());
}

void test_dashboard_codec_replace_tiles_preserves_unrelated_config() {
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, R"json({
    "schemaVersion":1,
    "hostname":"homepoint",
    "mqttbroker":"mqtt://broker",
    "futureField":{"keep":42},
    "scenes":[{"name":"old","devices":[]}]
  })json"));

  Tile tile;
  tile.key = "lamp";
  tile.type = TileType::Switch;
  tile.name = "Lamp";
  tile.items.push_back(switchItem("Lamp", "lamp"));
  std::vector<Tile> tiles{tile};
  String error;
  TEST_ASSERT_TRUE_MESSAGE(DashboardCodec::replaceTiles(doc, tiles, error), error.c_str());

  TEST_ASSERT_EQUAL(2, doc["schemaVersion"].as<int>());
  TEST_ASSERT_EQUAL_STRING("homepoint", doc["hostname"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("mqtt://broker", doc["mqttbroker"].as<const char*>());
  TEST_ASSERT_EQUAL(42, doc["futureField"]["keep"].as<int>());
  TEST_ASSERT_TRUE(doc["scenes"].isNull());
  TEST_ASSERT_EQUAL_UINT32(1, doc["tiles"].as<JsonArrayConst>().size());
  TEST_ASSERT_EQUAL_STRING("lamp", doc["tiles"][0]["id"].as<const char*>());
}
