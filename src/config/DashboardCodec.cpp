#include "DashboardCodec.h"

#include <string>
#include <utility>

namespace homepoint::config {
namespace {

void setError(String& error, const char* text) {
  error = text;
}

void setError(String& error, const std::string& text) {
  error = text.c_str();
}

bool requireString(
    JsonObjectConst object,
    const char* key,
    bool required,
    String& value,
    String& error,
    const std::string& context) {
  JsonVariantConst v = object[key];
  if (v.isNull()) {
    if (!required) return true;
    setError(error, context + " requires string field '" + key + "'");
    return false;
  }
  if (!v.is<const char*>()) {
    setError(error, context + "." + key + " must be a string");
    return false;
  }
  value = v.as<const char*>();
  if (required && value.isEmpty()) {
    setError(error, context + "." + key + " must not be empty");
    return false;
  }
  return true;
}

bool optionalString(
    JsonObjectConst object,
    const char* key,
    String& value,
    const char* fallback,
    String& error,
    const std::string& context) {
  JsonVariantConst v = object[key];
  if (v.isNull()) {
    value = fallback;
    return true;
  }
  if (!v.is<const char*>()) {
    setError(error, context + "." + key + " must be a string");
    return false;
  }
  value = v.as<const char*>();
  return true;
}

bool optionalBool(
    JsonObjectConst object,
    const char* key,
    bool fallback,
    bool& value,
    String& error,
    const std::string& context) {
  JsonVariantConst v = object[key];
  if (v.isNull()) {
    value = fallback;
    return true;
  }
  if (!v.is<bool>()) {
    setError(error, context + "." + key + " must be a boolean");
    return false;
  }
  value = v.as<bool>();
  return true;
}

bool parseSwitch(
    JsonObjectConst source,
    std::uint16_t itemId,
    model::TileItem& item,
    String& error,
    const std::string& context) {
  item.id = itemId;
  item.type = model::TileItemType::Switch;
  auto& device = item.switchDevice;
  device.id = itemId;

  if (!optionalString(source, "name", device.name, "Unnamed", error, context) ||
      !requireString(source, "getTopic", true, device.getTopic, error, context) ||
      !requireString(source, "setTopic", true, device.setTopic, error, context) ||
      !optionalString(source, "onValue", device.onValue, "true", error, context) ||
      !optionalString(source, "offValue", device.offValue, "false", error, context) ||
      !optionalString(source, "icon", device.icon, "", error, context)) {
    return false;
  }
  return true;
}

bool parseSensor(
    JsonObjectConst source,
    std::uint16_t itemId,
    model::TileItem& item,
    String& error,
    const std::string& context) {
  item.id = itemId;
  item.type = model::TileItemType::Sensor;
  auto& device = item.sensorDevice;
  device.id = itemId;

  if (!optionalString(source, "name", device.name, "Unnamed", error, context) ||
      !requireString(source, "getTopic", true, device.getTopic, error, context) ||
      !optionalString(source, "firstKey", device.firstKey, "", error, context) ||
      !optionalString(source, "secondKey", device.secondKey, "", error, context) ||
      !optionalString(source, "firstIcon", device.firstIcon, "", error, context) ||
      !optionalString(source, "secondIcon", device.secondIcon, "", error, context)) {
    return false;
  }

  String sensorType;
  if (!optionalString(
          source, "sensorType", sensorType, "singleValue", error, context)) {
    return false;
  }
  if (sensorType == "singleValue") {
    device.type = model::SensorType::SingleValue;
  } else if (sensorType == "combinedValues") {
    device.type = model::SensorType::CombinedValues;
  } else {
    setError(error, context + ".sensorType must be 'singleValue' or 'combinedValues'");
    return false;
  }

  if (!optionalBool(source, "jsondata", false, device.jsonData, error, context)) {
    return false;
  }
  if (device.jsonData && device.firstKey.isEmpty()) {
    setError(error, context + " JSON sensor requires firstKey");
    return false;
  }
  if (device.type == model::SensorType::CombinedValues &&
      device.jsonData && device.secondKey.isEmpty()) {
    setError(error, context + " combinedValues JSON sensor requires secondKey");
    return false;
  }
  return true;
}

const char* sensorTypeName(model::SensorType value) {
  return value == model::SensorType::CombinedValues
      ? "combinedValues"
      : "singleValue";
}

void appendSwitchFields(JsonObject target, const model::SwitchDevice& device) {
  target["name"] = device.name.c_str();
  target["getTopic"] = device.getTopic.c_str();
  target["setTopic"] = device.setTopic.c_str();
  target["onValue"] = device.onValue.c_str();
  target["offValue"] = device.offValue.c_str();
  target["icon"] = device.icon.c_str();
}

void appendSensorFields(JsonObject target, const model::SensorDevice& device) {
  target["name"] = device.name.c_str();
  target["sensorType"] = sensorTypeName(device.type);
  target["jsondata"] = device.jsonData;
  target["getTopic"] = device.getTopic.c_str();
  target["firstKey"] = device.firstKey.c_str();
  target["secondKey"] = device.secondKey.c_str();
  target["firstIcon"] = device.firstIcon.c_str();
  target["secondIcon"] = device.secondIcon.c_str();
}

void appendItem(JsonObject target, const model::TileItem& item) {
  if (item.type == model::TileItemType::Switch) {
    target["type"] = "switch";
    appendSwitchFields(target, item.switchDevice);
  } else {
    target["type"] = "sensor";
    appendSensorFields(target, item.sensorDevice);
  }
}

bool legacyRepresentable(const model::AppConfig& config, String& error) {
  if (!config.loadedFromLegacyScenes) return true;
  for (std::size_t i = 0; i < config.tiles.size(); ++i) {
    const auto& tile = config.tiles[i];
    if (tile.type != model::TileType::Scene) continue;
    if (tile.items.size() < DashboardCodec::kMinSceneItems) {
      setError(
          error,
          std::string("Legacy scene #") + std::to_string(i + 1) +
              " cannot be represented losslessly by schema v2 because it contains " +
              std::to_string(tile.items.size()) +
              " item(s); edit config.json or add another scene member before using the dashboard editor");
      return false;
    }
  }
  return true;
}

}  // namespace

bool DashboardCodec::parseRequest(
    const char* json,
    std::size_t length,
    model::AppConfig& config,
    String& error) {
  JsonDocument request;
  const auto result = deserializeJson(request, json, length);
  if (result) {
    if (result == DeserializationError::NoMemory) {
      setError(error, "Not enough memory to parse dashboard JSON");
    } else {
      setError(error, std::string("JSON parse error: ") + result.c_str());
    }
    return false;
  }
  if (request.overflowed()) {
    setError(error, "Not enough memory to parse dashboard JSON");
    return false;
  }
  if (!request.is<JsonObjectConst>()) {
    setError(error, "Dashboard request must be a JSON object");
    return false;
  }

  JsonObjectConst root = request.as<JsonObjectConst>();
  JsonVariantConst schema = root["schemaVersion"];
  if (!schema.is<int>()) {
    setError(error, "Dashboard API requires an integer schemaVersion");
    return false;
  }
  const int schemaVersion = schema.as<int>();
  if (schemaVersion < 2 || schemaVersion > model::AppConfig::kCurrentSchemaVersion) {
    setError(error, "Dashboard API supports schemaVersion 2 or 3");
    return false;
  }

  JsonVariantConst dashboardValue = root["dashboard"];
  if (!dashboardValue.is<JsonObjectConst>()) {
    setError(error, "'dashboard' must be an object");
    return false;
  }
  JsonVariantConst tilesValue = dashboardValue.as<JsonObjectConst>()["tiles"];
  if (!tilesValue.is<JsonArrayConst>()) {
    setError(error, "'dashboard.tiles' must be an array");
    return false;
  }

  model::AppConfig parsed;
  parsed.schemaVersion = schemaVersion;
  parsed.loadedFromLegacyScenes = false;
  if (!parseTiles(
          tilesValue.as<JsonArrayConst>(), parsed.tiles, error, "dashboard.tiles")) {
    return false;
  }

  config = std::move(parsed);
  error = "";
  return true;
}

bool DashboardCodec::parseTiles(
    JsonArrayConst tiles,
    std::vector<model::Tile>& parsedTiles,
    String& error,
    const char* contextPrefix) {
  parsedTiles.clear();
  std::size_t tileIndex = 0;
  for (JsonVariantConst tileValue : tiles) {
    const std::string context =
        std::string(contextPrefix ? contextPrefix : "tiles") +
        "[" + std::to_string(tileIndex) + "]";
    if (!tileValue.is<JsonObjectConst>()) {
      setError(error, context + " must be an object");
      return false;
    }
    JsonObjectConst source = tileValue.as<JsonObjectConst>();

    String type;
    if (!requireString(source, "type", true, type, error, context)) return false;

    model::Tile tile;
    if (!optionalString(source, "id", tile.key, "", error, context) ||
        !optionalString(source, "name", tile.name, "Unnamed", error, context) ||
        !optionalString(source, "icon", tile.icon, "", error, context)) {
      return false;
    }

    if (!tile.key.isEmpty()) {
      for (const auto& existing : parsedTiles) {
        if (existing.key == tile.key) {
          setError(error, std::string("Duplicate tile id: ") + tile.key.c_str());
          return false;
        }
      }
    }

    if (type == "switch") {
      tile.type = model::TileType::Switch;
      model::TileItem item;
      if (!parseSwitch(source, 0, item, error, context)) return false;
      tile.items.push_back(std::move(item));
    } else if (type == "sensor") {
      tile.type = model::TileType::Sensor;
      model::TileItem item;
      if (!parseSensor(source, 0, item, error, context)) return false;
      tile.items.push_back(std::move(item));
    } else if (type == "scene") {
      tile.type = model::TileType::Scene;
      JsonVariantConst itemsValue = source["items"];
      if (!itemsValue.is<JsonArrayConst>()) {
        setError(error, context + ".items must be an array");
        return false;
      }
      JsonArrayConst items = itemsValue.as<JsonArrayConst>();
      if (items.size() < kMinSceneItems) {
        setError(error, context + " requires at least two scene items");
        return false;
      }

      std::uint16_t itemId = 0;
      for (JsonVariantConst itemValue : items) {
        const std::string itemContext =
            context + ".items[" + std::to_string(itemId) + "]";
        if (!itemValue.is<JsonObjectConst>()) {
          setError(error, itemContext + " must be an object");
          return false;
        }
        JsonObjectConst sourceItem = itemValue.as<JsonObjectConst>();
        String itemType;
        if (!requireString(sourceItem, "type", true, itemType, error, itemContext)) {
          return false;
        }
        model::TileItem item;
        if (itemType == "switch") {
          if (!parseSwitch(sourceItem, itemId, item, error, itemContext)) return false;
        } else if (itemType == "sensor") {
          if (!parseSensor(sourceItem, itemId, item, error, itemContext)) return false;
        } else {
          setError(error, itemContext + ".type must be 'switch' or 'sensor'");
          return false;
        }
        ++itemId;
        tile.items.push_back(std::move(item));
      }
    } else {
      setError(error, context + ".type must be 'switch', 'sensor' or 'scene'");
      return false;
    }

    tile.id = static_cast<std::uint16_t>(parsedTiles.size());
    parsedTiles.push_back(std::move(tile));
    ++tileIndex;
  }
  error = "";
  return true;
}

bool DashboardCodec::prepareExplicitUpgradeTiles(
    const model::AppConfig& config,
    std::vector<model::Tile>& tiles,
    String& error) {
  tiles.clear();
  tiles.reserve(config.tiles.size());

  for (const auto& sourceTile : config.tiles) {
    model::Tile tile = sourceTile;

    if (config.loadedFromLegacyScenes &&
        sourceTile.type == model::TileType::Scene) {
      if (sourceTile.items.empty()) {
        setError(
            error,
            "Legacy scene has no devices and cannot be migrated to a dashboard tile");
        tiles.clear();
        return false;
      }

      if (sourceTile.items.size() == 1u) {
        const auto& sourceItem = sourceTile.items.front();
        tile.items.clear();
        tile.items.push_back(sourceItem);
        tile.type = sourceItem.type == model::TileItemType::Sensor
            ? model::TileType::Sensor
            : model::TileType::Switch;

        // A legacy one-device scene carries both scene-level (visible tile)
        // identity and device-level identity. A direct tile has only one
        // name/icon, so preserve the scene-level values: they are what the
        // dashboard showed before migration. MQTT/value/sensor fields still
        // come from the sole device.
        const String migratedName = sourceTile.name.isEmpty()
            ? (sourceItem.type == model::TileItemType::Switch
                   ? sourceItem.switchDevice.name
                   : sourceItem.sensorDevice.name)
            : sourceTile.name;
        if (sourceItem.type == model::TileItemType::Switch) {
          const String migratedIcon = sourceTile.icon.isEmpty()
              ? sourceItem.switchDevice.icon
              : sourceTile.icon;
          tile.name = migratedName;
          tile.icon = migratedIcon;
          tile.items.front().switchDevice.name = migratedName;
          tile.items.front().switchDevice.icon = migratedIcon;
        } else {
          tile.name = migratedName;
          tile.items.front().sensorDevice.name = migratedName;
        }
      }
    }

    tile.id = static_cast<std::uint16_t>(tiles.size());
    for (std::size_t itemIndex = 0; itemIndex < tile.items.size(); ++itemIndex) {
      auto& item = tile.items[itemIndex];
      item.id = static_cast<std::uint16_t>(itemIndex);
      if (item.type == model::TileItemType::Switch) {
        item.switchDevice.id = static_cast<std::uint16_t>(itemIndex);
      } else {
        item.sensorDevice.id = static_cast<std::uint16_t>(itemIndex);
      }
    }
    tiles.push_back(std::move(tile));
  }

  error = "";
  return true;
}

bool DashboardCodec::appendCanonicalTiles(
    JsonArray target,
    const std::vector<model::Tile>& tiles,
    String& error) {
  for (const auto& tile : tiles) {
    JsonObject output = target.add<JsonObject>();
    if (output.isNull()) {
      setError(error, "Not enough memory to serialize dashboard tiles");
      return false;
    }
    if (!tile.key.isEmpty()) output["id"] = tile.key.c_str();
    output["name"] = tile.name.c_str();
    if (!tile.icon.isEmpty()) output["icon"] = tile.icon.c_str();

    if (tile.type == model::TileType::Switch) {
      if (tile.items.empty()) {
        setError(error, "Switch tile has no runtime item");
        return false;
      }
      output["type"] = "switch";
      appendSwitchFields(output, tile.items.front().switchDevice);
    } else if (tile.type == model::TileType::Sensor) {
      if (tile.items.empty()) {
        setError(error, "Sensor tile has no runtime item");
        return false;
      }
      output["type"] = "sensor";
      appendSensorFields(output, tile.items.front().sensorDevice);
    } else {
      if (tile.items.size() < kMinSceneItems) {
        setError(error, "Scene tile has fewer than two items");
        return false;
      }
      output["type"] = "scene";
      JsonArray items = output["items"].to<JsonArray>();
      for (const auto& item : tile.items) {
        JsonObject itemObject = items.add<JsonObject>();
        if (itemObject.isNull()) {
          setError(error, "Not enough memory to serialize scene items");
          return false;
        }
        appendItem(itemObject, item);
      }
    }
  }
  return true;
}

bool DashboardCodec::serialize(
    const model::AppConfig& config,
    DashboardSource dashboardSource,
    String& output,
    String& error) {
  if (!legacyRepresentable(config, error)) return false;

  JsonDocument document;
  const int envelopeSchema = config.schemaVersion >= 2
      ? config.schemaVersion
      : 2;
  document["schemaVersion"] = envelopeSchema;
  JsonObject dashboard = document["dashboard"].to<JsonObject>();
  if (!appendCanonicalTiles(dashboard["tiles"].to<JsonArray>(), config.tiles, error)) {
    return false;
  }

  JsonObject capabilities = document["capabilities"].to<JsonObject>();
  JsonArray tileTypes = capabilities["tileTypes"].to<JsonArray>();
  tileTypes.add("switch");
  tileTypes.add("sensor");
  tileTypes.add("scene");
  JsonArray sceneItemTypes = capabilities["sceneItemTypes"].to<JsonArray>();
  sceneItemTypes.add("switch");
  sceneItemTypes.add("sensor");
  JsonArray sensorTypes = capabilities["sensorTypes"].to<JsonArray>();
  sensorTypes.add("singleValue");
  sensorTypes.add("combinedValues");
  capabilities["maxVisibleTiles"] = kMaxVisibleTiles;
  capabilities["minSceneItems"] = kMinSceneItems;

  JsonObject source = document["source"].to<JsonObject>();
  const char* sourceName = "draft";
  if (dashboardSource == DashboardSource::Active) sourceName = "active";
  if (dashboardSource == DashboardSource::LastGood) sourceName = "lastGood";
  source["configuration"] = sourceName;
  source["legacy"] = config.loadedFromLegacyScenes;
  source["layout"] = config.externalLayout ? "file" : "inline";
  if (config.externalLayout) {
    source["layoutFile"] = config.layoutFile.c_str();
    source["layoutName"] = config.layoutName.c_str();
    source["layoutRecovered"] = config.layoutRecoveredFromLastGood;
  }

  JsonArray warnings = document["warnings"].to<JsonArray>();
  if (dashboardSource == DashboardSource::LastGood) {
    warnings.add("Active config.json is invalid; editor content was recovered from config.lastgood.json");
  }
  if (config.loadedFromLegacyScenes) {
    warnings.add("Legacy scenes are shown as lossless schema-v2 scene equivalents; saving migrates the dashboard to schema v2");
  }
  if (config.schemaVersion < model::AppConfig::kCurrentSchemaVersion) {
    warnings.add("Configuration schema v3 is available; use Upgrade schema to externalize the current dashboard explicitly");
  }
  if (config.layoutRecoveredFromLastGood) {
    warnings.add("Active layout file is invalid; dashboard content was recovered from the layout last-good backup");
  }

  if (document.overflowed()) {
    setError(error, "Not enough memory to serialize dashboard response");
    return false;
  }

  output = "";
  const std::size_t outputBytes = measureJson(document);
  if (!output.reserve(outputBytes + 1u)) {
    setError(error, "Not enough memory to allocate dashboard response");
    return false;
  }
  serializeJson(document, output);
  error = "";
  return true;
}

bool DashboardCodec::replaceTiles(
    JsonDocument& document,
    const std::vector<model::Tile>& tiles,
    String& error,
    int targetSchemaVersion) {
  if (!document.is<JsonObject>()) {
    setError(error, "Base configuration must be a JSON object");
    return false;
  }
  JsonObject root = document.as<JsonObject>();
  root["schemaVersion"] = targetSchemaVersion;
  root.remove("scenes");
  root.remove("tiles");
  if (!appendCanonicalTiles(root["tiles"].to<JsonArray>(), tiles, error)) {
    return false;
  }
  if (document.overflowed()) {
    setError(error, "Not enough memory to merge dashboard into configuration");
    return false;
  }
  return true;
}

}  // namespace homepoint::config
