#include "RuntimeStateCodec.h"

#include <ArduinoJson.h>

#include "DashboardFingerprint.h"

namespace homepoint::model {
namespace {

const char* tileTypeName(TileType type) {
  switch (type) {
    case TileType::Switch: return "switch";
    case TileType::Sensor: return "sensor";
    case TileType::Scene: return "scene";
  }
  return "scene";
}

const char* itemTypeName(TileItemType type) {
  return type == TileItemType::Switch ? "switch" : "sensor";
}

const char* sensorTypeName(SensorType type) {
  return type == SensorType::CombinedValues ? "combinedValues" : "singleValue";
}

void appendAge(JsonObject object, bool known, std::uint32_t updatedAtMs,
               std::uint32_t capturedAtMs) {
  object["known"] = known;
  if (known) object["ageMs"] = static_cast<std::uint32_t>(capturedAtMs - updatedAtMs);
}

}  // namespace

bool RuntimeStateCodec::serialize(
    const AppConfig& config,
    const String& wifiStatus,
    bool wifiConnected,
    const String& mqttStatus,
    bool mqttConnected,
    std::uint32_t capturedAtMs,
    String& output,
    String& error) {
  JsonDocument document;
  document["schemaVersion"] = kSchemaVersion;
  document["dashboardFingerprint"] = dashboardFingerprint(config).c_str();
  document["capturedAtMs"] = capturedAtMs;

  JsonObject wifi = document["wifi"].to<JsonObject>();
  wifi["status"] = wifiStatus.c_str();
  wifi["connected"] = wifiConnected;

  JsonObject mqtt = document["mqtt"].to<JsonObject>();
  mqtt["status"] = mqttStatus.c_str();
  mqtt["connected"] = mqttConnected;

  JsonObject source = document["source"].to<JsonObject>();
  source["type"] = config.externalLayout ? "file" : "inline";
  if (config.externalLayout) {
    source["file"] = config.layoutFile.c_str();
    source["name"] = config.layoutName.c_str();
  }

  JsonArray tiles = document["tiles"].to<JsonArray>();
  for (std::size_t tileIndex = 0; tileIndex < config.tiles.size(); ++tileIndex) {
    const auto& tile = config.tiles[tileIndex];
    JsonObject tileObject = tiles.add<JsonObject>();
    if (tileObject.isNull()) {
      error = "Not enough memory to serialize runtime tile state";
      return false;
    }
    tileObject["index"] = tileIndex;
    if (!tile.key.isEmpty()) tileObject["id"] = tile.key.c_str();
    tileObject["type"] = tileTypeName(tile.type);
    tileObject["name"] = tile.name.c_str();

    std::size_t switchCount = 0;
    std::size_t switchesKnown = 0;
    std::size_t switchesOn = 0;

    JsonArray items = tileObject["items"].to<JsonArray>();
    for (std::size_t itemIndex = 0; itemIndex < tile.items.size(); ++itemIndex) {
      const auto& item = tile.items[itemIndex];
      JsonObject itemObject = items.add<JsonObject>();
      if (itemObject.isNull()) {
        error = "Not enough memory to serialize runtime item state";
        return false;
      }
      itemObject["index"] = itemIndex;
      itemObject["id"] = item.id;
      itemObject["type"] = itemTypeName(item.type);

      if (item.type == TileItemType::Switch) {
        const auto& device = item.switchDevice;
        itemObject["name"] = device.name.c_str();
        itemObject["active"] = device.active;
        appendAge(itemObject, device.stateKnown, device.lastUpdateMs, capturedAtMs);
        ++switchCount;
        if (device.stateKnown) {
          ++switchesKnown;
          if (device.active) ++switchesOn;
        }
      } else {
        const auto& device = item.sensorDevice;
        itemObject["name"] = device.name.c_str();
        itemObject["sensorType"] = sensorTypeName(device.type);
        itemObject["firstValue"] = device.firstValue.c_str();
        if (device.type == SensorType::CombinedValues) {
          itemObject["secondValue"] = device.secondValue.c_str();
        }
        appendAge(itemObject, device.valueKnown, device.lastUpdateMs, capturedAtMs);
      }
    }

    tileObject["switchCount"] = switchCount;
    if (switchCount > 0) {
      tileObject["switchesKnown"] = switchesKnown;
      tileObject["switchesOn"] = switchesOn;
      tileObject["anySwitchOn"] = switchesOn > 0;
      tileObject["allSwitchesOn"] = switchesKnown == switchCount && switchesOn == switchCount;
    }
  }

  if (document.overflowed()) {
    error = "Not enough memory to serialize runtime state";
    return false;
  }

  const std::size_t required = measureJson(document) + 1;
  String serialized;
  if (!serialized.reserve(required)) {
    error = "Not enough memory for runtime state output";
    return false;
  }
  if (serializeJson(document, serialized) == 0) {
    error = "Could not serialize runtime state";
    return false;
  }

  output = serialized;
  error = "";
  return true;
}

}  // namespace homepoint::model
