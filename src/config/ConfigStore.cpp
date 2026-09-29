#include "ConfigStore.h"

#include <cstring>
#include <utility>

namespace homepoint::config {
namespace {

constexpr const char* kLittleFsMountPoint = "/littlefs";
constexpr const char* kLittleFsPartitionLabel = "littlefs";
constexpr std::uint8_t kLittleFsMaxOpenFiles = 10;

String jsonScalarToString(JsonVariantConst value) {
  if (value.isNull()) return "";
  if (value.is<const char*>()) return String(value.as<const char*>());
  String out;
  serializeJson(value, out);
  return out;
}

model::SensorType parseSensorType(const String& value) {
  return value == "combinedValues" ? model::SensorType::CombinedValues
                                   : model::SensorType::SingleValue;
}

}  // namespace

bool ConfigStore::begin(bool allowInitialFormat) {
  Serial.printf(
      "[FS] init: partition='%s' mount='%s' auto-format=%s\n",
      kLittleFsPartitionLabel,
      kLittleFsMountPoint,
      allowInitialFormat ? "allowed" : "disabled");

  if (mount()) return true;

  Serial.printf("[FS] initial mount failed: %s\n", lastError_.c_str());
  if (!allowInitialFormat) {
    Serial.println(
        "[FS] automatic format is disabled because bootstrap settings are already configured");
    Serial.println(
        "[FS] recovery: use Web UI > Recovery > Format LittleFS, or upload the filesystem image");
    return false;
  }

  Serial.println("[FS] fresh/unconfigured device: formatting LittleFS after mount failure");
  if (!formatPartition()) return false;

  if (!mount()) {
    Serial.printf("[FS] mount after format failed: %s\n", lastError_.c_str());
    return false;
  }

  Serial.println("[FS] recovery format and mount completed successfully");
  return true;
}

bool ConfigStore::mount() {
  if (mounted_) {
    LittleFS.end();
    mounted_ = false;
  }

  Serial.printf("[FS] mounting partition '%s'...\n", kLittleFsPartitionLabel);
  mounted_ = LittleFS.begin(
      false,
      kLittleFsMountPoint,
      kLittleFsMaxOpenFiles,
      kLittleFsPartitionLabel);

  if (!mounted_) {
    lastError_ = "mount failed (unformatted or corrupt filesystem)";
    Serial.println("[FS] mount FAILED");
    return false;
  }

  lastError_ = "";
  Serial.println("[FS] mount OK");
  logUsage();
  return true;
}

bool ConfigStore::formatPartition() {
  if (mounted_) {
    Serial.println("[FS] unmounting before format");
    LittleFS.end();
    mounted_ = false;
  }

  // Arduino-ESP32 LittleFS::format() formats the partition label remembered
  // by the most recent begin() call. Probe/select our explicit partition first.
  Serial.printf("[FS] selecting partition '%s' for format\n", kLittleFsPartitionLabel);
  const bool probeMounted = LittleFS.begin(
      false,
      kLittleFsMountPoint,
      kLittleFsMaxOpenFiles,
      kLittleFsPartitionLabel);
  if (probeMounted) {
    Serial.println("[FS] partition mounted during format probe; unmounting");
  } else {
    Serial.println("[FS] format probe could not mount partition; continuing with explicit format");
  }
  LittleFS.end();

  Serial.printf("[FS] formatting partition '%s'...\n", kLittleFsPartitionLabel);
  if (!LittleFS.format()) {
    lastError_ = "format failed";
    Serial.println("[FS] format FAILED");
    return false;
  }

  lastError_ = "";
  Serial.println("[FS] format OK");
  return true;
}

void ConfigStore::logUsage() const {
  if (!mounted_) return;
  const auto total = LittleFS.totalBytes();
  const auto used = LittleFS.usedBytes();
  Serial.printf(
      "[FS] capacity: total=%u bytes (%u KiB), used=%u bytes (%u KiB)\n",
      static_cast<unsigned>(total),
      static_cast<unsigned>(total / 1024u),
      static_cast<unsigned>(used),
      static_cast<unsigned>(used / 1024u));
}

String ConfigStore::statusText() const {
  if (!mounted_) {
    return String("UNAVAILABLE (") + lastError_ + ")";
  }

  const auto totalKiB = LittleFS.totalBytes() / 1024u;
  const auto usedKiB = LittleFS.usedBytes() / 1024u;
  return String("OK (") + usedKiB + "/" + totalKiB + " KiB used)";
}

bool ConfigStore::format() {
  Serial.println("[FS] manual format requested");
  if (!formatPartition()) return false;

  if (!mount()) {
    Serial.printf("[FS] mount after manual format failed: %s\n", lastError_.c_str());
    return false;
  }

  if (!ensureDefaultConfig()) {
    lastError_ = "mounted, but could not create default config.json";
    Serial.printf("[FS] %s\n", lastError_.c_str());
    return false;
  }

  Serial.println("[FS] manual format completed; default config.json is present");
  logUsage();
  return true;
}

bool ConfigStore::ensureDefaultConfig() {
  if (!mounted_) return false;
  if (LittleFS.exists(kConfigPath)) return true;

  const String initial = R"json({
  "mqttbroker": "",
  "mqttusername": "",
  "mqttpasswd": "",
  "timezone": "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00",
  "schemaVersion": 3,
  "ui": {
    "longPressMs": 600
  },
  "screenSaverMinutes": 10,
  "screenSaverPowerSaveEnabled": true,
  "powerFrom5vRailNotUsb": true,
  "tiles": []
}
)json";
  return writeFile(kConfigPath, initial);
}

bool ConfigStore::load(model::AppConfig& config, String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return false;
  }

  if (loadFromPath(kConfigPath, config, error)) return true;

  String lastGoodError;
  model::AppConfig lastGood;
  if (LittleFS.exists(kLastGoodPath) &&
      loadFromPath(kLastGoodPath, lastGood, lastGoodError)) {
    config = std::move(lastGood);
    error = "Active config invalid; loaded config.lastgood.json";
    return true;
  }

  return false;
}

bool ConfigStore::loadFromPath(
    const char* path, model::AppConfig& config, String& error) const {
  if (!LittleFS.exists(path)) {
    error = String(path) + " does not exist";
    return false;
  }
  return validateAndParse(readFile(path), config, error);
}

bool ConfigStore::validateAndParse(
    const String& json, model::AppConfig& config, String& error) const {
  return validateAndParse(json.c_str(), json.length(), config, error);
}

bool ConfigStore::validateAndParse(
    const char* json, std::size_t length,
    model::AppConfig& config, String& error) const {
  JsonDocument document;
  const auto result = deserializeJson(document, json, length);
  if (result) {
    if (result == DeserializationError::NoMemory) {
      error = "Not enough memory to parse configuration JSON";
    } else {
      error = String("JSON parse error: ") + result.c_str();
    }
    return false;
  }
  if (document.overflowed()) {
    error = "Not enough memory to parse configuration JSON";
    return false;
  }
  if (!document.is<JsonObject>()) {
    error = "Top-level JSON value must be an object";
    return false;
  }
  return parseDocument(document, config, error);
}

bool ConfigStore::parseDocument(
    JsonDocument& document, model::AppConfig& config, String& error) const {
  model::AppConfig parsed;
  JsonObjectConst root = document.as<JsonObjectConst>();

  JsonVariantConst hostnameValue = root["hostname"];
  if (!hostnameValue.isNull()) {
    parsed.hostname = hostnameValue | "";
    if (parsed.hostname.isEmpty()) {
      error = "'hostname' must not be empty when present";
      return false;
    }
    if (parsed.hostname.length() > 32u) {
      error = "'hostname' must be no longer than 32 characters";
      return false;
    }
  }

  parsed.mqtt.uri = root["mqttbroker"] | "";
  parsed.mqtt.username = root["mqttusername"] | "";
  parsed.mqtt.password = root["mqttpasswd"] | "";
  parsed.timezone = root["timezone"] | "";

  JsonVariantConst uiValue = root["ui"];
  if (!uiValue.isNull()) {
    if (!uiValue.is<JsonObjectConst>()) {
      error = "'ui' must be an object";
      return false;
    }
    JsonObjectConst ui = uiValue.as<JsonObjectConst>();
    JsonVariantConst longPressValue = ui["longPressMs"];
    if (!longPressValue.isNull()) {
      if (!longPressValue.is<long>()) {
        error = "'ui.longPressMs' must be an integer";
        return false;
      }
      const long longPressMs = longPressValue.as<long>();
      if (longPressMs < 200 || longPressMs > 5000) {
        error = "'ui.longPressMs' must be between 200 and 5000 ms";
        return false;
      }
      parsed.ui.longPressMs = static_cast<std::uint32_t>(longPressMs);
    }
  }

  parsed.hardware.screenSaverMinutes = root["screenSaverMinutes"] | 10;
  parsed.hardware.screenSaverPowerSaveEnabled =
      root["screenSaverPowerSaveEnabled"] | true;
  parsed.hardware.powerFrom5vRailNotUsb =
      root["powerFrom5vRailNotUsb"] | true;
  parsed.hardware.screenRotationAngle = root["screenRotationAngle"] | 1;
  parsed.hardware.displayColorInverted =
      root["displayColorInverted"] | false;
  parsed.hardware.powerSaveMHz = root["powerSaveMHz"] | 80;
  parsed.hardware.ledPinPullup = root["ledPinPullup"] | false;
  parsed.hardware.touchXAxisInverted =
      root["touchXAxisInverted"] | false;
  parsed.hardware.touchYAxisInverted =
      root["touchYAxisInverted"] | true;

  auto parseSwitch = [&](JsonObjectConst source,
                         std::uint16_t itemId,
                         model::TileItem& item,
                         const String& context) -> bool {
    item.id = itemId;
    item.type = model::TileItemType::Switch;
    auto& device = item.switchDevice;
    device.id = itemId;
    device.name = source["name"] | "Unnamed";
    device.getTopic = source["getTopic"] | "";
    device.setTopic = source["setTopic"] | "";
    device.onValue = source["onValue"] | "true";
    device.offValue = source["offValue"] | "false";
    device.icon = source["icon"] | "";
    if (device.getTopic.isEmpty() || device.setTopic.isEmpty()) {
      error = context + " requires getTopic and setTopic";
      return false;
    }
    return true;
  };

  auto parseSensor = [&](JsonObjectConst source,
                         std::uint16_t itemId,
                         model::TileItem& item,
                         const String& context) -> bool {
    item.id = itemId;
    item.type = model::TileItemType::Sensor;
    auto& device = item.sensorDevice;
    device.id = itemId;
    device.name = source["name"] | "Unnamed";
    // In schema v2 "type" identifies the tile/item itself. Sensor value
    // shape therefore has its own field instead of overloading "type".
    const String sensorType = source["sensorType"] | "singleValue";
    device.type = parseSensorType(sensorType);
    device.jsonData = source["jsondata"] | false;
    device.getTopic = source["getTopic"] | "";
    device.firstKey = source["firstKey"] | "";
    device.secondKey = source["secondKey"] | "";
    device.firstIcon = source["firstIcon"] | "";
    device.secondIcon = source["secondIcon"] | "";
    if (device.getTopic.isEmpty()) {
      error = context + " requires getTopic";
      return false;
    }
    if (device.jsonData && device.firstKey.isEmpty()) {
      error = context + " JSON sensor requires firstKey";
      return false;
    }
    if (device.type == model::SensorType::CombinedValues &&
        device.jsonData && device.secondKey.isEmpty()) {
      error = context + " combinedValues JSON sensor requires secondKey";
      return false;
    }
    return true;
  };

  auto addTile = [&](model::Tile&& tile) -> bool {
    if (!tile.key.isEmpty()) {
      for (const auto& existing : parsed.tiles) {
        if (existing.key == tile.key) {
          error = String("Duplicate tile id: ") + tile.key;
          return false;
        }
      }
    }
    tile.id = static_cast<std::uint16_t>(parsed.tiles.size());
    parsed.tiles.push_back(std::move(tile));
    return true;
  };

  auto parseLegacyScenes = [&]() -> bool {
    JsonVariantConst scenesValue = root["scenes"];
    if (scenesValue.isNull()) return true;
    if (!scenesValue.is<JsonArrayConst>()) {
      error = "'scenes' must be an array";
      return false;
    }

    for (JsonVariantConst sceneValue : scenesValue.as<JsonArrayConst>()) {
      if (!sceneValue.is<JsonObjectConst>()) {
        error = "Each legacy scene must be an object";
        return false;
      }
      JsonObjectConst sourceScene = sceneValue.as<JsonObjectConst>();
      model::Tile tile;
      tile.type = model::TileType::Scene;
      tile.name = sourceScene["name"] | "Unnamed";
      tile.icon = sourceScene["icon"] | "";
      const String sceneType = sourceScene["type"] | "";

      JsonVariantConst devicesValue = sourceScene["devices"];
      if (!devicesValue.is<JsonArrayConst>()) {
        error = "Legacy scene requires a devices array";
        return false;
      }

      std::uint16_t itemId = 0;
      for (JsonVariantConst deviceValue : devicesValue.as<JsonArrayConst>()) {
        if (!deviceValue.is<JsonObjectConst>()) {
          error = "Each legacy scene device must be an object";
          return false;
        }
        JsonObjectConst sourceDevice = deviceValue.as<JsonObjectConst>();
        model::TileItem item;
        if (sceneType == "Light" || sceneType == "Switch") {
          if (!parseSwitch(sourceDevice, itemId++, item, "Legacy switch device")) {
            return false;
          }
        } else if (sceneType == "Sensor") {
          item.id = itemId;
          item.type = model::TileItemType::Sensor;
          auto& device = item.sensorDevice;
          device.id = itemId++;
          device.name = sourceDevice["name"] | "Unnamed";
          device.type = parseSensorType(String(sourceDevice["type"] | "singleValue"));
          device.jsonData = sourceDevice["jsondata"] | false;
          device.getTopic = sourceDevice["getTopic"] | "";
          device.firstKey = sourceDevice["firstKey"] | "";
          device.secondKey = sourceDevice["secondKey"] | "";
          device.firstIcon = sourceDevice["firstIcon"] | "";
          device.secondIcon = sourceDevice["secondIcon"] | "";
          if (device.getTopic.isEmpty()) {
            error = "Legacy sensor device requires getTopic";
            return false;
          }
          if (device.jsonData && device.firstKey.isEmpty()) {
            error = "Legacy JSON sensor requires firstKey";
            return false;
          }
          if (device.type == model::SensorType::CombinedValues &&
              device.jsonData && device.secondKey.isEmpty()) {
            error = "Legacy combinedValues JSON sensor requires secondKey";
            return false;
          }
        } else {
          error = String("Unknown legacy scene type: ") + sceneType;
          return false;
        }
        tile.items.push_back(std::move(item));
      }

      if (!addTile(std::move(tile))) return false;
    }
    return true;
  };

  auto parseTileArray = [&](JsonArrayConst tiles) -> bool {
    for (JsonVariantConst tileValue : tiles) {
      if (!tileValue.is<JsonObjectConst>()) {
        error = "Each tile must be an object";
        return false;
      }

      JsonObjectConst sourceTile = tileValue.as<JsonObjectConst>();
      model::Tile tile;
      tile.key = sourceTile["id"] | "";
      tile.name = sourceTile["name"] | "Unnamed";
      tile.icon = sourceTile["icon"] | "";
      const String tileType = sourceTile["type"] | "";

      if (tileType == "switch") {
        tile.type = model::TileType::Switch;
        model::TileItem item;
        if (!parseSwitch(sourceTile, 0, item, "Switch tile")) return false;
        tile.items.push_back(std::move(item));
      } else if (tileType == "sensor") {
        tile.type = model::TileType::Sensor;
        model::TileItem item;
        if (!parseSensor(sourceTile, 0, item, "Sensor tile")) return false;
        tile.items.push_back(std::move(item));
      } else if (tileType == "scene") {
        tile.type = model::TileType::Scene;
        JsonVariantConst itemsValue = sourceTile["items"];
        if (!itemsValue.is<JsonArrayConst>()) {
          error = "Scene tile requires an items array";
          return false;
        }
        JsonArrayConst items = itemsValue.as<JsonArrayConst>();
        if (items.size() < 2) {
          error = "Scene tile requires at least two items; use a direct device tile for one item";
          return false;
        }

        std::uint16_t itemId = 0;
        for (JsonVariantConst itemValue : items) {
          if (!itemValue.is<JsonObjectConst>()) {
            error = "Each scene item must be an object";
            return false;
          }
          JsonObjectConst sourceItem = itemValue.as<JsonObjectConst>();
          const String itemType = sourceItem["type"] | "";
          model::TileItem item;
          if (itemType == "switch") {
            if (!parseSwitch(sourceItem, itemId++, item, "Scene switch item")) {
              return false;
            }
          } else if (itemType == "sensor") {
            if (!parseSensor(sourceItem, itemId++, item, "Scene sensor item")) {
              return false;
            }
          } else {
            error = String("Unknown scene item type: ") + itemType;
            return false;
          }
          tile.items.push_back(std::move(item));
        }
      } else {
        error = String("Unknown tile type: ") + tileType;
        return false;
      }

      if (!addTile(std::move(tile))) return false;
    }
    return true;
  };

  int schemaVersion = 1;
  JsonVariantConst schemaValue = root["schemaVersion"];
  if (!schemaValue.isNull()) {
    if (!schemaValue.is<int>()) {
      error = "'schemaVersion' must be an integer";
      return false;
    }
    schemaVersion = schemaValue.as<int>();
    if (schemaVersion < 1) {
      error = "'schemaVersion' must be >= 1";
      return false;
    }
  }

  if (schemaVersion > model::AppConfig::kCurrentSchemaVersion) {
    error = String("Unsupported config schemaVersion: ") + schemaVersion;
    return false;
  }

  if (schemaVersion == 3) {
    parsed.schemaVersion = 3;
    parsed.loadedFromLegacyScenes = false;
    JsonVariantConst tilesValue = root["tiles"];
    if (tilesValue.isNull()) {
      Serial.println("[CONFIG] schema: 3; no tiles configured");
    } else if (tilesValue.is<JsonArrayConst>()) {
      if (!parseTileArray(tilesValue.as<JsonArrayConst>())) return false;
      Serial.printf(
          "[CONFIG] schema: 3 inline; loaded %u tiles\n",
          static_cast<unsigned>(parsed.tiles.size()));
    } else if (tilesValue.is<const char*>()) {
      const String filename = tilesValue.as<const char*>();
      if (!validLayoutFilename(filename)) {
        error = "Schema-v3 'tiles' layout reference must match layout_[A-Za-z0-9_-]+.json";
        return false;
      }
      LayoutDocument layout;
      bool recoveredLayout = false;
      if (!loadLayoutDocument(filename, layout, recoveredLayout, error)) return false;
      parsed.tiles = std::move(layout.tiles);
      parsed.externalLayout = true;
      parsed.layoutFile = filename;
      parsed.layoutName = layout.name;
      parsed.layoutRecoveredFromLastGood = recoveredLayout;
      Serial.printf(
          "[CONFIG] schema: 3 layout '%s' (%s); loaded %u tiles\n",
          filename.c_str(),
          recoveredLayout ? "last-good" : "active",
          static_cast<unsigned>(parsed.tiles.size()));
    } else {
      error = "Schema-v3 'tiles' must be an array or a layout_*.json filename";
      return false;
    }
  } else if (schemaVersion == 2) {
    parsed.schemaVersion = 2;
    if (!root["tiles"].isNull()) {
      JsonVariantConst tilesValue = root["tiles"];
      if (!tilesValue.is<JsonArrayConst>()) {
        error = "Schema-v2 'tiles' must be an array";
        return false;
      }
      parsed.loadedFromLegacyScenes = false;
      if (!parseTileArray(tilesValue.as<JsonArrayConst>())) return false;
      Serial.printf(
          "[CONFIG] schema: 2; loaded %u tiles\n",
          static_cast<unsigned>(parsed.tiles.size()));
    } else if (!root["scenes"].isNull()) {
      parsed.loadedFromLegacyScenes = true;
      if (!parseLegacyScenes()) return false;
      Serial.printf(
          "[CONFIG] schema: 2 compatibility fallback; loaded %u legacy scenes as runtime tiles\n",
          static_cast<unsigned>(parsed.tiles.size()));
    } else {
      parsed.loadedFromLegacyScenes = false;
      Serial.println("[CONFIG] schema: 2; no tiles configured");
    }
  } else {
    parsed.schemaVersion = schemaVersion;
    parsed.loadedFromLegacyScenes = true;
    if (!parseLegacyScenes()) return false;
    if (schemaValue.isNull()) {
      Serial.printf(
          "[CONFIG] schema: legacy (implicit v1); loaded %u scenes as runtime tiles\n",
          static_cast<unsigned>(parsed.tiles.size()));
    } else {
      Serial.printf(
          "[CONFIG] schema: %d legacy; loaded %u scenes as runtime tiles\n",
          schemaVersion,
          static_cast<unsigned>(parsed.tiles.size()));
    }
  }

  config = std::move(parsed);
  error = "";
  return true;
}

bool ConfigStore::saveConfigAtomically(const String& json, String& error) {
  return saveConfigAtomically(json.c_str(), json.length(), error);
}

bool ConfigStore::saveConfigAtomically(
    const char* json, std::size_t length, String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return false;
  }

  model::AppConfig validationTarget;
  if (!validateAndParse(json, length, validationTarget, error)) return false;

  if (!writeFile(
          kNewPath,
          reinterpret_cast<const std::uint8_t*>(json),
          length)) {
    error = "Could not write config.new.json";
    return false;
  }

  if (LittleFS.exists(kConfigPath)) {
    // Never replace a known-good backup with an already-corrupt active file.
    model::AppConfig currentConfig;
    String currentError;
    if (validateAndParse(readFile(kConfigPath), currentConfig, currentError)) {
      if (!copyFile(kConfigPath, kLastGoodPath)) {
        LittleFS.remove(kNewPath);
        error = "Could not create last-good backup";
        return false;
      }
    }
  }

  LittleFS.remove(kConfigPath);
  if (!LittleFS.rename(kNewPath, kConfigPath)) {
    error = "Could not activate new config";
    return false;
  }

  error = "";
  return true;
}

bool ConfigStore::loadEditableDocument(
    JsonDocument& document,
    model::AppConfig& config,
    bool& recoveredFromLastGood,
    String& error) const {
  recoveredFromLastGood = false;
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return false;
  }

  auto loadPath = [&](const char* path, String& pathError) -> bool {
    document.clear();
    if (!LittleFS.exists(path)) {
      pathError = String(path) + " does not exist";
      return false;
    }

    const String text = readFile(path);
    const auto result = deserializeJson(document, text);
    if (result) {
      if (result == DeserializationError::NoMemory) {
        pathError = String("Not enough memory to parse ") + path;
      } else {
        pathError = String("JSON parse error in ") + path + ": " + result.c_str();
      }
      document.clear();
      return false;
    }
    if (document.overflowed()) {
      pathError = String("Not enough memory to parse ") + path;
      document.clear();
      return false;
    }
    if (!document.is<JsonObject>()) {
      pathError = String(path) + " top-level JSON value must be an object";
      document.clear();
      return false;
    }
    if (!parseDocument(document, config, pathError)) {
      document.clear();
      return false;
    }
    return true;
  };

  String activeError;
  if (loadPath(kConfigPath, activeError)) {
    error = "";
    return true;
  }

  String lastGoodError;
  if (loadPath(kLastGoodPath, lastGoodError)) {
    recoveredFromLastGood = true;
    error = "";
    return true;
  }

  error = String("No valid base configuration: ") + activeError;
  if (!lastGoodError.isEmpty()) {
    error += String("; last-good: ") + lastGoodError;
  }
  return false;
}

bool ConfigStore::looksLikeMemoryError(const String& error) {
  return std::strstr(error.c_str(), "Not enough memory") != nullptr;
}


bool ConfigStore::loadLayoutFromPath(
    const String& path, LayoutDocument& layout, String& error) const {
  if (!LittleFS.exists(path)) {
    error = path + " does not exist";
    return false;
  }
  const String text = readFile(path.c_str());
  return LayoutCodec::parse(text.c_str(), text.length(), layout, error);
}

bool ConfigStore::loadLayoutDocument(
    const String& filename, LayoutDocument& layout,
    bool& recoveredFromLastGood, String& error) const {
  recoveredFromLastGood = false;
  if (!validLayoutFilename(filename)) {
    error = "Layout filename must match layout_[A-Za-z0-9_-]+.json";
    return false;
  }
  String activeError;
  if (loadLayoutFromPath(layoutPath(filename), layout, activeError)) {
    error = "";
    return true;
  }
  String backupError;
  if (loadLayoutFromPath(layoutLastGoodPath(filename), layout, backupError)) {
    recoveredFromLastGood = true;
    error = "";
    return true;
  }
  error = "No valid layout '" + filename + "': " + activeError;
  if (!backupError.isEmpty()) error += "; last-good: " + backupError;
  return false;
}

bool ConfigStore::saveLayoutDocumentAtomically(
    const String& filename, const LayoutDocument& layout, String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return false;
  }
  if (!validLayoutFilename(filename)) {
    error = "Layout filename must match layout_[A-Za-z0-9_-]+.json";
    return false;
  }

  String serialized;
  if (!LayoutCodec::serialize(layout, serialized, error)) return false;

  const String activePath = layoutPath(filename);
  const String newPath = layoutNewPath(filename);
  const String backupPath = layoutLastGoodPath(filename);
  LittleFS.remove(newPath);
  if (!writeFile(newPath.c_str(), serialized)) {
    error = "Could not write temporary layout file";
    LittleFS.remove(newPath);
    return false;
  }

  LayoutDocument staged;
  String stagedError;
  if (!loadLayoutFromPath(newPath, staged, stagedError)) {
    error = "Temporary layout validation failed: " + stagedError;
    LittleFS.remove(newPath);
    return false;
  }

  const bool hadActive = LittleFS.exists(activePath);
  if (hadActive) {
    LayoutDocument current;
    String currentError;
    if (loadLayoutFromPath(activePath, current, currentError)) {
      if (!copyFile(activePath.c_str(), backupPath.c_str())) {
        error = "Could not create layout last-good backup";
        LittleFS.remove(newPath);
        return false;
      }
    }
  } else {
    // A newly-created layout must not inherit a stale backup left behind by an
    // older file that happened to use the same name.
    LittleFS.remove(backupPath);
  }

  LittleFS.remove(activePath);
  if (!LittleFS.rename(newPath, activePath)) {
    error = "Could not activate new layout file";
    LittleFS.remove(newPath);
    return false;
  }
  error = "";
  return true;
}

bool ConfigStore::writeConfigDocumentAtomically(
    JsonDocument& document, String& error) {
  if (document.overflowed()) {
    error = "Not enough memory to build configuration";
    return false;
  }
  String json;
  const std::size_t bytes = measureJsonPretty(document);
  if (!json.reserve(bytes + 1u)) {
    error = "Not enough memory to allocate configuration JSON";
    return false;
  }
  serializeJsonPretty(document, json);
  return saveConfigAtomically(json.c_str(), json.length(), error);
}

DashboardResult ConfigStore::getDashboardJson(String& json, String& error) const {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return DashboardResult::StorageUnavailable;
  }

  JsonDocument source;
  model::AppConfig config;
  bool recoveredFromLastGood = false;
  if (!loadEditableDocument(source, config, recoveredFromLastGood, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }
  if (!DashboardCodec::serialize(
          config,
          recoveredFromLastGood ? DashboardSource::LastGood : DashboardSource::Active,
          json, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }
  return DashboardResult::Ok;
}

DashboardResult ConfigStore::validateDashboard(
    const char* json,
    std::size_t length,
    String& normalizedJson,
    String& error) const {
  model::AppConfig dashboardConfig;
  if (!DashboardCodec::parseRequest(json, length, dashboardConfig, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  if (!DashboardCodec::serialize(dashboardConfig, DashboardSource::Draft, normalizedJson, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  return DashboardResult::Ok;
}

DashboardResult ConfigStore::saveDashboardAtomically(
    const char* json,
    std::size_t length,
    String& normalizedJson,
    String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return DashboardResult::StorageUnavailable;
  }

  model::AppConfig dashboardConfig;
  if (!DashboardCodec::parseRequest(json, length, dashboardConfig, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }

  JsonDocument currentDocument;
  model::AppConfig baseConfig;
  bool recoveredFromLastGood = false;
  if (!loadEditableDocument(
          currentDocument, baseConfig, recoveredFromLastGood, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }

  if (baseConfig.externalLayout) {
    LayoutDocument layout;
    layout.name = baseConfig.layoutName;
    layout.tiles = dashboardConfig.tiles;
    if (!saveLayoutDocumentAtomically(baseConfig.layoutFile, layout, error)) {
      return looksLikeMemoryError(error)
          ? DashboardResult::InsufficientMemory
          : DashboardResult::IoError;
    }
    dashboardConfig.schemaVersion = 3;
    dashboardConfig.externalLayout = true;
    dashboardConfig.layoutFile = baseConfig.layoutFile;
    dashboardConfig.layoutName = baseConfig.layoutName;
    dashboardConfig.layoutRecoveredFromLastGood = false;
  } else {
    // Editing a v1/v2 inline dashboard must not silently opt into schema v3.
    // Explicit schema migration is owned by /api/dashboard/upgrade.
    const int targetSchema = baseConfig.schemaVersion >= 3 ? 3 : 2;
    if (!DashboardCodec::replaceTiles(
            currentDocument, dashboardConfig.tiles, error, targetSchema)) {
      return looksLikeMemoryError(error)
          ? DashboardResult::InsufficientMemory
          : DashboardResult::Conflict;
    }
    {
      model::AppConfig validatedMergedConfig;
      if (!parseDocument(currentDocument, validatedMergedConfig, error)) {
        return DashboardResult::Conflict;
      }
    }
    if (!writeConfigDocumentAtomically(currentDocument, error)) {
      return looksLikeMemoryError(error)
          ? DashboardResult::InsufficientMemory
          : DashboardResult::IoError;
    }
    dashboardConfig.schemaVersion = targetSchema;
    dashboardConfig.externalLayout = false;
  }

  if (!DashboardCodec::serialize(
          dashboardConfig, DashboardSource::Active, normalizedJson, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }
  error = "";
  return DashboardResult::Ok;
}

DashboardResult ConfigStore::getLayoutsJson(String& json, String& error) const {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return DashboardResult::StorageUnavailable;
  }

  JsonDocument configDocument;
  model::AppConfig config;
  bool recoveredConfig = false;
  if (!loadEditableDocument(configDocument, config, recoveredConfig, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }

  JsonDocument response;
  response["configSchemaVersion"] = config.schemaVersion;
  response["currentSchemaVersion"] = model::AppConfig::kCurrentSchemaVersion;
  response["upgradeAvailable"] = config.schemaVersion < 3;
  JsonObject active = response["active"].to<JsonObject>();
  active["type"] = config.externalLayout ? "file" : "inline";
  if (config.externalLayout) {
    active["file"] = config.layoutFile.c_str();
    active["name"] = config.layoutName.c_str();
    active["recovered"] = config.layoutRecoveredFromLastGood;
  }
  active["configurationRecovered"] = recoveredConfig;

  JsonArray layouts = response["layouts"].to<JsonArray>();
  bool activeListed = false;
  File root = LittleFS.open("/");
  if (root && root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String path = file.path() ? String(file.path()) : String();
        String filename = path.startsWith("/") ? path.substring(1) : path;
        if (validLayoutFilename(filename)) {
          JsonObject item = layouts.add<JsonObject>();
          item["file"] = filename.c_str();
          const bool isActive = config.externalLayout && filename == config.layoutFile;
          item["active"] = isActive;
          if (isActive) activeListed = true;
          LayoutDocument layout;
          bool recovered = false;
          String layoutError;
          if (loadLayoutDocument(filename, layout, recovered, layoutError)) {
            item["name"] = layout.name.c_str();
            item["valid"] = true;
            item["recovered"] = recovered;
          } else {
            item["name"] = filename.c_str();
            item["valid"] = false;
            item["error"] = layoutError.c_str();
          }
        }
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }
  if (config.externalLayout && !activeListed) {
    JsonObject item = layouts.add<JsonObject>();
    item["file"] = config.layoutFile.c_str();
    item["name"] = config.layoutName.c_str();
    item["active"] = true;
    item["valid"] = true;
    item["recovered"] = config.layoutRecoveredFromLastGood;
  }

  JsonObject capabilities = response["capabilities"].to<JsonObject>();
  capabilities["filePattern"] = "layout_[A-Za-z0-9_-]+.json";
  capabilities["layoutSchemaVersion"] = LayoutDocument::kSchemaVersion;
  capabilities["maxNameLength"] = LayoutDocument::kMaxNameLength;

  if (response.overflowed()) {
    error = "Not enough memory to list layouts";
    return DashboardResult::InsufficientMemory;
  }
  json = "";
  const std::size_t bytes = measureJson(response);
  if (!json.reserve(bytes + 1u)) {
    error = "Not enough memory to allocate layout list";
    return DashboardResult::InsufficientMemory;
  }
  serializeJson(response, json);
  error = "";
  return DashboardResult::Ok;
}

DashboardResult ConfigStore::getLayoutJson(
    const String& filename, String& json, String& error) const {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return DashboardResult::StorageUnavailable;
  }
  if (!validLayoutFilename(filename)) {
    error = "Invalid layout filename";
    return DashboardResult::InvalidRequest;
  }
  LayoutDocument layout;
  bool recovered = false;
  if (!loadLayoutDocument(filename, layout, recovered, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }
  model::AppConfig config;
  config.schemaVersion = 3;
  config.externalLayout = true;
  config.layoutFile = filename;
  config.layoutName = layout.name;
  config.layoutRecoveredFromLastGood = recovered;
  config.tiles = std::move(layout.tiles);
  if (!DashboardCodec::serialize(config, DashboardSource::Draft, json, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }
  return DashboardResult::Ok;
}

DashboardResult ConfigStore::validateLayout(
    const String& filename,
    const String& name,
    const char* json,
    std::size_t length,
    String& normalizedJson,
    String& error) const {
  if (!validLayoutFilename(filename)) {
    error = "Layout filename must match layout_[A-Za-z0-9_-]+.json";
    return DashboardResult::InvalidRequest;
  }
  model::AppConfig config;
  if (!DashboardCodec::parseRequest(json, length, config, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  LayoutDocument layout;
  layout.name = name;
  layout.name.trim();
  layout.tiles = config.tiles;
  String ignored;
  if (!LayoutCodec::serialize(layout, ignored, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  config.schemaVersion = 3;
  config.externalLayout = true;
  config.layoutFile = filename;
  config.layoutName = layout.name;
  if (!DashboardCodec::serialize(config, DashboardSource::Draft, normalizedJson, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  return DashboardResult::Ok;
}

DashboardResult ConfigStore::saveLayoutAtomically(
    const String& filename,
    const String& name,
    const char* json,
    std::size_t length,
    String& normalizedJson,
    bool& activeLayout,
    String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return DashboardResult::StorageUnavailable;
  }
  if (!validLayoutFilename(filename)) {
    error = "Layout filename must match layout_[A-Za-z0-9_-]+.json";
    return DashboardResult::InvalidRequest;
  }
  model::AppConfig config;
  if (!DashboardCodec::parseRequest(json, length, config, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  LayoutDocument layout;
  layout.name = name;
  layout.name.trim();
  layout.tiles = config.tiles;
  String validationJson;
  if (!LayoutCodec::serialize(layout, validationJson, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  if (!saveLayoutDocumentAtomically(filename, layout, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::IoError;
  }
  activeLayout = isLayoutActive(filename);
  config.schemaVersion = 3;
  config.externalLayout = true;
  config.layoutFile = filename;
  config.layoutName = layout.name;
  if (!DashboardCodec::serialize(
          config, activeLayout ? DashboardSource::Active : DashboardSource::Draft,
          normalizedJson, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }
  return DashboardResult::Ok;
}

bool ConfigStore::isLayoutActive(const String& filename) const {
  JsonDocument document;
  model::AppConfig config;
  bool recovered = false;
  String error;
  return loadEditableDocument(document, config, recovered, error) &&
         config.externalLayout && config.layoutFile == filename;
}

DashboardResult ConfigStore::setDashboardSource(
    const char* json,
    std::size_t length,
    String& responseJson,
    String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return DashboardResult::StorageUnavailable;
  }
  JsonDocument request;
  const auto parsedRequest = deserializeJson(request, json, length);
  if (parsedRequest || !request.is<JsonObjectConst>()) {
    error = parsedRequest
        ? String("JSON parse error: ") + parsedRequest.c_str()
        : "Dashboard source request must be an object";
    return parsedRequest == DeserializationError::NoMemory
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  JsonVariantConst typeValue = request["type"];
  if (!typeValue.is<const char*>()) {
    error = "Dashboard source 'type' must be 'file' or 'inline'";
    return DashboardResult::InvalidRequest;
  }
  const String type = typeValue.as<const char*>();

  JsonDocument configDocument;
  model::AppConfig baseConfig;
  bool recoveredConfig = false;
  if (!loadEditableDocument(configDocument, baseConfig, recoveredConfig, error)) {
    return DashboardResult::Conflict;
  }
  if (recoveredConfig) {
    error = "Active config.json is invalid; repair it before changing dashboard source";
    return DashboardResult::Conflict;
  }
  JsonObject root = configDocument.as<JsonObject>();
  root["schemaVersion"] = 3;
  root.remove("scenes");

  if (type == "file") {
    JsonVariantConst fileValue = request["file"];
    if (!fileValue.is<const char*>()) {
      error = "File dashboard source requires 'file'";
      return DashboardResult::InvalidRequest;
    }
    const String filename = fileValue.as<const char*>();
    LayoutDocument layout;
    bool recoveredLayout = false;
    if (!loadLayoutDocument(filename, layout, recoveredLayout, error)) {
      return DashboardResult::Conflict;
    }
    if (recoveredLayout) {
      error = "Layout active file is invalid; save the recovered layout before activating it";
      return DashboardResult::Conflict;
    }
    root["tiles"] = filename.c_str();
  } else if (type == "inline") {
    std::vector<model::Tile> tiles = baseConfig.tiles;
    JsonVariantConst fileValue = request["file"];
    if (!fileValue.isNull()) {
      if (!fileValue.is<const char*>()) {
        error = "Inline source 'file' must be a layout filename when present";
        return DashboardResult::InvalidRequest;
      }
      LayoutDocument layout;
      bool recoveredLayout = false;
      if (!loadLayoutDocument(
              String(fileValue.as<const char*>()), layout, recoveredLayout, error)) {
        return DashboardResult::Conflict;
      }
      tiles = std::move(layout.tiles);
    }
    root.remove("tiles");
    if (!DashboardCodec::appendCanonicalTiles(
            root["tiles"].to<JsonArray>(), tiles, error)) {
      return looksLikeMemoryError(error)
          ? DashboardResult::InsufficientMemory
          : DashboardResult::Conflict;
    }
  } else {
    error = "Dashboard source 'type' must be 'file' or 'inline'";
    return DashboardResult::InvalidRequest;
  }

  {
    model::AppConfig validation;
    if (!parseDocument(configDocument, validation, error)) {
      return DashboardResult::Conflict;
    }
  }
  if (!writeConfigDocumentAtomically(configDocument, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::IoError;
  }
  return getDashboardJson(responseJson, error);
}

DashboardResult ConfigStore::upgradeDashboardSchema(
    const char* json,
    std::size_t length,
    String& responseJson,
    String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return DashboardResult::StorageUnavailable;
  }
  JsonDocument request;
  const auto parsedRequest = deserializeJson(request, json, length);
  if (parsedRequest || !request.is<JsonObjectConst>()) {
    error = parsedRequest
        ? String("JSON parse error: ") + parsedRequest.c_str()
        : "Upgrade request must be an object";
    return parsedRequest == DeserializationError::NoMemory
        ? DashboardResult::InsufficientMemory
        : DashboardResult::InvalidRequest;
  }
  JsonVariantConst fileValue = request["file"];
  JsonVariantConst nameValue = request["name"];
  if (!fileValue.is<const char*>() || !nameValue.is<const char*>()) {
    error = "Upgrade requires string fields 'file' and 'name'";
    return DashboardResult::InvalidRequest;
  }
  const String filename = fileValue.as<const char*>();
  String name = nameValue.as<const char*>();
  name.trim();
  if (!validLayoutFilename(filename)) {
    error = "Layout filename must match layout_[A-Za-z0-9_-]+.json";
    return DashboardResult::InvalidRequest;
  }
  if (LittleFS.exists(layoutPath(filename))) {
    error = "Layout file already exists; choose a different filename";
    return DashboardResult::Conflict;
  }

  JsonDocument configDocument;
  model::AppConfig baseConfig;
  bool recoveredConfig = false;
  if (!loadEditableDocument(configDocument, baseConfig, recoveredConfig, error)) {
    return DashboardResult::Conflict;
  }
  if (recoveredConfig) {
    error = "Active config.json is invalid; repair it before schema upgrade";
    return DashboardResult::Conflict;
  }
  if (baseConfig.schemaVersion >= 3) {
    error = "Configuration is already schema v3";
    return DashboardResult::Conflict;
  }

  std::vector<model::Tile> upgradeTiles;
  if (!DashboardCodec::prepareExplicitUpgradeTiles(
          baseConfig, upgradeTiles, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }

  LayoutDocument layout;
  layout.name = name;
  layout.tiles = std::move(upgradeTiles);
  String validationJson;
  if (!LayoutCodec::serialize(layout, validationJson, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::Conflict;
  }
  if (!saveLayoutDocumentAtomically(filename, layout, error)) {
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::IoError;
  }

  auto cleanupCreatedLayout = [&]() {
    LittleFS.remove(layoutPath(filename));
    LittleFS.remove(layoutLastGoodPath(filename));
    LittleFS.remove(layoutNewPath(filename));
  };

  JsonObject root = configDocument.as<JsonObject>();
  root["schemaVersion"] = 3;
  root.remove("scenes");
  root["tiles"] = filename.c_str();
  {
    model::AppConfig validation;
    if (!parseDocument(configDocument, validation, error)) {
      cleanupCreatedLayout();
      return looksLikeMemoryError(error)
          ? DashboardResult::InsufficientMemory
          : DashboardResult::Conflict;
    }
  }
  if (!writeConfigDocumentAtomically(configDocument, error)) {
    // The upgrade created this filename specifically for this transaction. If
    // config activation fails, do not leave an orphaned managed layout that
    // looks like a completed upgrade.
    cleanupCreatedLayout();
    return looksLikeMemoryError(error)
        ? DashboardResult::InsufficientMemory
        : DashboardResult::IoError;
  }
  return getDashboardJson(responseJson, error);
}

bool ConfigStore::saveTextFileAtomically(
    const String& path, const String& text, String& error) {
  return saveTextFileAtomically(path, text.c_str(), text.length(), error);
}

bool ConfigStore::saveTextFileAtomically(
    const String& path, const char* text, std::size_t length, String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return false;
  }
  if (!safePath(path) || path == "/" || !editableTextPath(path)) {
    error = "File is read-only or not an editable text file";
    return false;
  }
  if (path == kConfigPath) {
    return saveConfigAtomically(text, length, error);
  }

  String lower = path;
  lower.toLowerCase();
  if (lower.endsWith(".json")) {
    JsonDocument document;
    const auto result = deserializeJson(document, text, length);
    if (result) {
      error = String("JSON parse error: ") + result.c_str();
      return false;
    }
  }

  const String stagedPath = transactionPath(path, ".new");
  LittleFS.remove(stagedPath);
  if (!writeFile(
          stagedPath.c_str(),
          reinterpret_cast<const std::uint8_t*>(text),
          length)) {
    error = "Could not write temporary file";
    LittleFS.remove(stagedPath);
    return false;
  }

  if (!replaceStagedFileAtomically(stagedPath, path, error)) {
    LittleFS.remove(stagedPath);
    return false;
  }

  error = "";
  return true;
}

bool ConfigStore::installUploadedFileAtomically(
    const String& stagedPath, const String& path, String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return false;
  }
  if (!internalPath(stagedPath) || !LittleFS.exists(stagedPath)) {
    error = "Upload staging file is unavailable";
    return false;
  }
  if (!safePath(path) || path == "/") {
    error = "Invalid upload destination";
    return false;
  }
  if (!uploadablePath(path)) {
    error = path == kConfigPath
        ? "Upload to config.json is disabled; use the JSON editor so the configuration can be validated"
        : "Destination is a managed read-only file";
    return false;
  }

  String lower = path;
  lower.toLowerCase();
  if (lower.endsWith(".json")) {
    File staged = LittleFS.open(stagedPath, "r");
    if (!staged) {
      error = "Could not reopen staged JSON file";
      return false;
    }
    JsonDocument document;
    const auto result = deserializeJson(document, staged);
    staged.close();
    if (result) {
      error = String("JSON parse error: ") + result.c_str();
      return false;
    }
  }

  return replaceStagedFileAtomically(stagedPath, path, error);
}

bool ConfigStore::replaceStagedFileAtomically(
    const String& stagedPath, const String& path, String& error) {
  File existing = LittleFS.open(path, "r");
  if (existing && existing.isDirectory()) {
    existing.close();
    error = "Destination is a directory";
    return false;
  }
  existing.close();

  const String backupPath = transactionPath(path, ".bak");
  LittleFS.remove(backupPath);

  const bool hadOriginal = LittleFS.exists(path);
  if (hadOriginal && !LittleFS.rename(path, backupPath)) {
    error = "Could not create temporary backup";
    return false;
  }

  if (!LittleFS.rename(stagedPath, path)) {
    if (hadOriginal) {
      if (!LittleFS.rename(backupPath, path)) {
        error = "Could not activate replacement and could not restore the original file";
        return false;
      }
    }
    error = "Could not activate replacement file";
    return false;
  }

  if (hadOriginal) LittleFS.remove(backupPath);
  error = "";
  return true;
}

String ConfigStore::readConfigText() const {
  if (!mounted_) return "{}";
  return readFile(kConfigPath);
}

bool ConfigStore::setHostname(const String& hostname, String& error) {
  if (!mounted_) {
    error = "LittleFS is not mounted";
    return false;
  }
  if (hostname.isEmpty()) {
    error = "Hostname must not be empty";
    return false;
  }
  if (hostname.length() > 32u) {
    error = "Hostname must be no longer than 32 characters";
    return false;
  }

  JsonDocument document;
  const auto result = deserializeJson(document, readFile(kConfigPath));
  if (result || !document.is<JsonObject>()) {
    error = result ? String("JSON parse error: ") + result.c_str()
                   : String("Top-level JSON value must be an object");
    return false;
  }

  document["hostname"] = hostname;
  String updated;
  serializeJsonPretty(document, updated);
  return saveConfigAtomically(updated, error);
}

bool ConfigStore::importLegacyBootstrap(BootstrapSettings& settings) const {
  if (!mounted_ || !LittleFS.exists(kConfigPath)) return false;

  JsonDocument document;
  if (deserializeJson(document, readFile(kConfigPath))) return false;
  JsonObjectConst root = document.as<JsonObjectConst>();

  const String ssid = root["wifi"] | "";
  if (ssid.isEmpty() || ssid == " ") return false;

  settings.configured = true;
  settings.wifiSsid = ssid;
  settings.wifiPassword = root["password"] | "";
  settings.hostname = root["hostname"] | "homepoint-m5";
  settings.webUsername = root["login"] | "admin";
  settings.webPassword = root["webpass"] | "";
  return true;
}

bool ConfigStore::fileExists(const String& path) const {
  return mounted_ && safePath(path) && LittleFS.exists(path);
}

bool ConfigStore::removeFile(const String& path) {
  if (!mounted_ || !deletablePath(path)) return false;
  return LittleFS.remove(path);
}

String ConfigStore::listFilesJson() const {
  JsonDocument document;
  JsonArray files = document["files"].to<JsonArray>();

  if (mounted_) {
    appendFiles(files, "/", 0);
  }

  String out;
  serializeJson(document, out);
  return out;
}

void ConfigStore::appendFiles(
    JsonArray files, const char* dirname, std::uint8_t depth) const {
  constexpr std::uint8_t kMaxDirectoryDepth = 8;
  if (depth > kMaxDirectoryDepth) return;

  File root = LittleFS.open(dirname, "r");
  if (!root || !root.isDirectory()) {
    root.close();
    return;
  }

  File file = root.openNextFile();
  while (file) {
    const String path = file.path() ? String(file.path()) : String();
    const bool isDirectory = file.isDirectory();

    if (!path.isEmpty() && !internalPath(path)) {
      JsonObject entry = files.add<JsonObject>();
      entry["name"] = path;
      entry["size"] = static_cast<std::uint32_t>(file.size());
      entry["directory"] = isDirectory;
      if (!isDirectory) {
        entry["editable"] = editableTextPath(path);
        entry["deletable"] = deletablePath(path);
        entry["uploadable"] = uploadablePath(path);
      }
    }

    // Close the current entry before descending so recursive enumeration uses
    // at most one directory handle per level. ConfigStore mounts LittleFS with
    // a deliberately small max-open-files limit.
    file.close();
    if (isDirectory && !path.isEmpty() && !internalPath(path) &&
        depth < kMaxDirectoryDepth) {
      appendFiles(files, path.c_str(), depth + 1);
    }
    file = root.openNextFile();
  }
  root.close();
}

bool ConfigStore::internalPath(const String& path) {
  if (path.startsWith("/.__hpm5_") || path == kNewPath) return true;
  if (!path.startsWith("/layout_")) return false;
  return path.endsWith(".new.json") || path.endsWith(".lastgood.json");
}

std::uint32_t ConfigStore::pathHash(const String& path) {
  std::uint32_t hash = 2166136261u;
  for (std::size_t i = 0; i < path.length(); ++i) {
    hash ^= static_cast<std::uint8_t>(path[i]);
    hash *= 16777619u;
  }
  return hash;
}

String ConfigStore::transactionPath(const String& path, const char* suffix) {
  String out = "/.__hpm5_";
  out += String(pathHash(path), HEX);
  out += suffix;
  return out;
}


String ConfigStore::layoutPath(const String& filename) {
  return String("/") + filename;
}

String ConfigStore::layoutLastGoodPath(const String& filename) {
  String stem = filename;
  if (stem.endsWith(".json")) stem.remove(stem.length() - 5u);
  return String("/") + stem + ".lastgood.json";
}

String ConfigStore::layoutNewPath(const String& filename) {
  String stem = filename;
  if (stem.endsWith(".json")) stem.remove(stem.length() - 5u);
  return String("/") + stem + ".new.json";
}

bool ConfigStore::safePath(const String& path) {
  return path.startsWith("/") && path.indexOf("..") < 0 &&
         path.indexOf('\\') < 0 && !internalPath(path);
}

bool ConfigStore::validLayoutFilename(const String& filename) {
  return LayoutCodec::validFilename(filename);
}

bool ConfigStore::editableTextPath(const String& path) {
  if (!safePath(path) || path == "/" || path == kLastGoodPath) return false;
  const String filename = path.startsWith("/") ? path.substring(1) : path;
  if (validLayoutFilename(filename)) return false;
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".json") || lower.endsWith(".txt") ||
         lower.endsWith(".md") || lower.endsWith(".css") ||
         lower.endsWith(".js") || lower.endsWith(".html") ||
         lower.endsWith(".htm") || lower.endsWith(".csv") ||
         lower.endsWith(".log");
}

bool ConfigStore::deletablePath(const String& path) {
  const String filename = path.startsWith("/") ? path.substring(1) : path;
  return safePath(path) && path != "/" && path != kConfigPath &&
         path != kLastGoodPath && !validLayoutFilename(filename);
}

bool ConfigStore::uploadablePath(const String& path) {
  const String filename = path.startsWith("/") ? path.substring(1) : path;
  return safePath(path) && path != "/" && path != kConfigPath &&
         path != kLastGoodPath && !validLayoutFilename(filename);
}

String ConfigStore::readFile(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) return "";
  String out;
  out.reserve(file.size() + 1);
  while (file.available()) {
    out += static_cast<char>(file.read());
  }
  return out;
}

bool ConfigStore::writeFile(const char* path, const String& content) {
  return writeFile(
      path,
      reinterpret_cast<const std::uint8_t*>(content.c_str()),
      content.length());
}

bool ConfigStore::writeFile(
    const char* path, const std::uint8_t* content, std::size_t length) {
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  const auto written = length ? file.write(content, length) : 0u;
  file.flush();
  file.close();
  return length == 0 || written == length;
}

bool ConfigStore::copyFile(const char* from, const char* to) {
  File src = LittleFS.open(from, "r");
  if (!src) return false;
  File dst = LittleFS.open(to, "w");
  if (!dst) {
    src.close();
    return false;
  }

  std::uint8_t buffer[512];
  bool ok = true;
  while (src.available()) {
    const auto count = src.read(buffer, sizeof(buffer));
    if (count && dst.write(buffer, count) != count) {
      ok = false;
      break;
    }
  }
  dst.flush();
  dst.close();
  src.close();
  return ok;
}

}  // namespace homepoint::config
