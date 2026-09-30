#include <unity.h>

#include <ArduinoJson.h>

#include "../../src/model/DashboardFingerprint.h"
#include "../../src/model/RuntimeStateCodec.h"

using homepoint::model::AppConfig;
using homepoint::model::RuntimeStateCodec;
using homepoint::model::SensorType;
using homepoint::model::Tile;
using homepoint::model::TileItem;
using homepoint::model::TileItemType;
using homepoint::model::TileType;

namespace {

Tile switchTile(const char* id, const char* name, bool active, bool known,
                std::uint32_t updatedAtMs) {
  Tile tile;
  tile.id = 0;
  tile.key = id;
  tile.name = name;
  tile.type = TileType::Switch;
  TileItem item;
  item.id = 0;
  item.type = TileItemType::Switch;
  item.switchDevice.id = 0;
  item.switchDevice.name = name;
  item.switchDevice.active = active;
  item.switchDevice.stateKnown = known;
  item.switchDevice.lastUpdateMs = updatedAtMs;
  tile.items.push_back(item);
  return tile;
}

}  // namespace

void test_runtime_state_codec_serializes_known_switch_state_and_age() {
  AppConfig config;
  config.externalLayout = true;
  config.layoutFile = "layout_ground_floor.json";
  config.layoutName = "Ground Floor";
  config.tiles.push_back(switchTile("lamp", "Lamp", true, true, 900));

  String body;
  String error;
  TEST_ASSERT_TRUE(RuntimeStateCodec::serialize(
      config, "ONLINE", true, "ONLINE", true, 1000, body, error));
  TEST_ASSERT_TRUE(error.isEmpty());

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, body.c_str()));
  TEST_ASSERT_EQUAL(1, doc["schemaVersion"].as<int>());
  TEST_ASSERT_EQUAL_STRING(
      homepoint::model::dashboardFingerprint(config).c_str(),
      doc["dashboardFingerprint"].as<const char*>());
  TEST_ASSERT_TRUE(doc["wifi"]["connected"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("ONLINE", doc["wifi"]["status"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("file", doc["source"]["type"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING(
      "layout_ground_floor.json", doc["source"]["file"].as<const char*>());
  TEST_ASSERT_TRUE(doc["mqtt"]["connected"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("ONLINE", doc["mqtt"]["status"].as<const char*>());

  JsonObjectConst tile = doc["tiles"][0].as<JsonObjectConst>();
  TEST_ASSERT_TRUE(tile["anySwitchOn"].as<bool>());
  TEST_ASSERT_TRUE(tile["allSwitchesOn"].as<bool>());
  JsonObjectConst item = tile["items"][0].as<JsonObjectConst>();
  TEST_ASSERT_TRUE(item["known"].as<bool>());
  TEST_ASSERT_TRUE(item["active"].as<bool>());
  TEST_ASSERT_EQUAL_UINT32(100, item["ageMs"].as<std::uint32_t>());
}

void test_runtime_state_codec_preserves_unknown_sensor_state() {
  AppConfig config;
  Tile tile;
  tile.id = 0;
  tile.key = "climate";
  tile.name = "Climate";
  tile.type = TileType::Sensor;
  TileItem item;
  item.id = 0;
  item.type = TileItemType::Sensor;
  item.sensorDevice.id = 0;
  item.sensorDevice.name = "Climate";
  item.sensorDevice.type = SensorType::CombinedValues;
  item.sensorDevice.firstValue = "-";
  item.sensorDevice.secondValue = "-";
  item.sensorDevice.valueKnown = false;
  tile.items.push_back(item);
  config.tiles.push_back(tile);

  String body;
  String error;
  TEST_ASSERT_TRUE(RuntimeStateCodec::serialize(
      config, "ONLINE", true, "OFFLINE", false, 5000, body, error));

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, body.c_str()));
  JsonObjectConst state = doc["tiles"][0]["items"][0].as<JsonObjectConst>();
  TEST_ASSERT_FALSE(state["known"].as<bool>());
  TEST_ASSERT_FALSE(state.containsKey("ageMs"));
  TEST_ASSERT_EQUAL_STRING("-", state["firstValue"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("-", state["secondValue"].as<const char*>());
}


void test_runtime_state_codec_aggregates_scene_switches_and_member_values() {
  AppConfig config;
  Tile scene;
  scene.id = 0;
  scene.key = "living";
  scene.name = "Living Room";
  scene.type = TileType::Scene;

  TileItem onSwitch;
  onSwitch.id = 0;
  onSwitch.type = TileItemType::Switch;
  onSwitch.switchDevice.id = 0;
  onSwitch.switchDevice.name = "Ceiling";
  onSwitch.switchDevice.active = true;
  onSwitch.switchDevice.stateKnown = true;
  onSwitch.switchDevice.lastUpdateMs = 1800;
  scene.items.push_back(onSwitch);

  TileItem unknownSwitch;
  unknownSwitch.id = 1;
  unknownSwitch.type = TileItemType::Switch;
  unknownSwitch.switchDevice.id = 1;
  unknownSwitch.switchDevice.name = "Floor";
  unknownSwitch.switchDevice.active = false;
  unknownSwitch.switchDevice.stateKnown = false;
  scene.items.push_back(unknownSwitch);

  TileItem sensor;
  sensor.id = 2;
  sensor.type = TileItemType::Sensor;
  sensor.sensorDevice.id = 2;
  sensor.sensorDevice.name = "Temperature";
  sensor.sensorDevice.type = SensorType::SingleValue;
  sensor.sensorDevice.firstValue = "21.5";
  sensor.sensorDevice.valueKnown = true;
  sensor.sensorDevice.lastUpdateMs = 1750;
  scene.items.push_back(sensor);
  config.tiles.push_back(scene);

  String body;
  String error;
  TEST_ASSERT_TRUE(RuntimeStateCodec::serialize(
      config, "ONLINE", true, "ONLINE", true, 2000, body, error));

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, body.c_str()));
  JsonObjectConst state = doc["tiles"][0].as<JsonObjectConst>();
  TEST_ASSERT_EQUAL(2u, state["switchCount"].as<unsigned>());
  TEST_ASSERT_EQUAL(1u, state["switchesKnown"].as<unsigned>());
  TEST_ASSERT_EQUAL(1u, state["switchesOn"].as<unsigned>());
  TEST_ASSERT_TRUE(state["anySwitchOn"].as<bool>());
  TEST_ASSERT_FALSE(state["allSwitchesOn"].as<bool>());

  JsonObjectConst first = state["items"][0].as<JsonObjectConst>();
  TEST_ASSERT_TRUE(first["known"].as<bool>());
  TEST_ASSERT_TRUE(first["active"].as<bool>());
  TEST_ASSERT_EQUAL_UINT32(200u, first["ageMs"].as<std::uint32_t>());

  JsonObjectConst second = state["items"][1].as<JsonObjectConst>();
  TEST_ASSERT_FALSE(second["known"].as<bool>());
  TEST_ASSERT_FALSE(second.containsKey("ageMs"));

  JsonObjectConst third = state["items"][2].as<JsonObjectConst>();
  TEST_ASSERT_TRUE(third["known"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("21.5", third["firstValue"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(250u, third["ageMs"].as<std::uint32_t>());
}


void test_dashboard_fingerprint_ignores_runtime_values_but_tracks_configuration() {
  AppConfig config;
  Tile tile = switchTile("lamp", "Lamp", false, false, 0);
  tile.items[0].switchDevice.getTopic = "lamp/state";
  tile.items[0].switchDevice.setTopic = "lamp/set";
  tile.items[0].switchDevice.onValue = "ON";
  tile.items[0].switchDevice.offValue = "OFF";
  tile.items[0].switchDevice.icon = "lamp";
  tile.icon = "lamp";
  config.tiles.push_back(tile);

  const String before = homepoint::model::dashboardFingerprint(config);
  config.tiles[0].items[0].switchDevice.active = true;
  config.tiles[0].items[0].switchDevice.stateKnown = true;
  config.tiles[0].items[0].switchDevice.lastUpdateMs = 12345;
  const String afterRuntimeUpdate = homepoint::model::dashboardFingerprint(config);
  TEST_ASSERT_EQUAL_STRING(before.c_str(), afterRuntimeUpdate.c_str());

  config.tiles[0].items[0].switchDevice.getTopic = "lamp/new-state";
  const String afterConfigUpdate = homepoint::model::dashboardFingerprint(config);
  TEST_ASSERT_NOT_EQUAL(0, std::strcmp(before.c_str(), afterConfigUpdate.c_str()));
}

void test_runtime_state_codec_age_survives_millis_wraparound() {
  AppConfig config;
  config.tiles.push_back(
      switchTile("lamp", "Lamp", false, true, 0xfffffff0u));

  String body;
  String error;
  TEST_ASSERT_TRUE(RuntimeStateCodec::serialize(
      config, "ONLINE", true, "ONLINE", true, 0x00000014u, body, error));

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, body.c_str()));
  TEST_ASSERT_EQUAL_UINT32(
      36u, doc["tiles"][0]["items"][0]["ageMs"].as<std::uint32_t>());
}
