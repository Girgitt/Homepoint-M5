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
  if (!schema.is<int>() ||
      schema.as<int>() != model::AppConfig::kCurrentSchemaVersion) {
    setError(error, "Dashboard API requires schemaVersion 2");
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
  parsed.schemaVersion = model::AppConfig::kCurrentSchemaVersion;
  parsed.loadedFromLegacyScenes = false;

  std::size_t tileIndex = 0;
  for (JsonVariantConst tileValue : tilesValue.as<JsonArrayConst>()) {
    const std::string context = "dashboard.tiles[" + std::to_string(tileIndex) + "]";
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
      for (const auto& existing : parsed.tiles) {
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
        if (!requireString(
                sourceItem, "type", true, itemType, error, itemContext)) {
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

    tile.id = static_cast<std::uint16_t>(parsed.tiles.size());
    parsed.tiles.push_back(std::move(tile));
    ++tileIndex;
  }

  config = std::move(parsed);
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
  document["schemaVersion"] = model::AppConfig::kCurrentSchemaVersion;
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

  JsonArray warnings = document["warnings"].to<JsonArray>();
  if (dashboardSource == DashboardSource::LastGood) {
    warnings.add("Active config.json is invalid; editor content was recovered from config.lastgood.json");
  }
  if (config.loadedFromLegacyScenes) {
    warnings.add("Legacy scenes are shown as lossless schema-v2 scene equivalents; saving migrates the dashboard to schema v2");
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
    String& error) {
  if (!document.is<JsonObject>()) {
    setError(error, "Base configuration must be a JSON object");
    return false;
  }
  JsonObject root = document.as<JsonObject>();
  root["schemaVersion"] = model::AppConfig::kCurrentSchemaVersion;
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
