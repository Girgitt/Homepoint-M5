#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../model/Model.h"

namespace homepoint::config {

enum class DashboardSource {
  Draft,
  Active,
  LastGood,
};

// Pure dashboard-envelope codec used by ConfigStore and native tests.
// It is deliberately independent of LittleFS and web-server classes.
class DashboardCodec {
 public:
  static constexpr std::size_t kMaxVisibleTiles = 6;
  static constexpr std::size_t kMinSceneItems = 2;

  // Parse the editor envelope with strict JSON type/enum validation.
  // Capabilities/source metadata in the envelope are informational and ignored.
  static bool parseRequest(
      const char* json,
      std::size_t length,
      model::AppConfig& config,
      String& error);

  static bool parseTiles(
      JsonArrayConst tiles,
      std::vector<model::Tile>& parsedTiles,
      String& error,
      const char* contextPrefix = "dashboard.tiles");

  // Prepare runtime tiles for an explicit legacy -> structured-schema
  // migration. Unlike serialize(), this operation is intentionally allowed to
  // collapse a legacy one-device scene into a direct switch/sensor tile. The
  // conversion is only used by the user-triggered Upgrade schema action; it is
  // never performed implicitly while reading or editing legacy config.
  static bool prepareExplicitUpgradeTiles(
      const model::AppConfig& config,
      std::vector<model::Tile>& tiles,
      String& error);

  // Serialize a runtime dashboard to the editor envelope. Legacy dashboards are
  // accepted only when they can be represented without loss in schema v2.
  static bool serialize(
      const model::AppConfig& config,
      DashboardSource source,
      String& output,
      String& error);

  // Replace only the schema/dashboard part of an existing complete config JSON
  // document. Unknown/unrelated top-level fields are left untouched.
  static bool replaceTiles(
      JsonDocument& document,
      const std::vector<model::Tile>& tiles,
      String& error,
      int targetSchemaVersion = model::AppConfig::kCurrentSchemaVersion);

  static bool appendCanonicalTiles(
      JsonArray target,
      const std::vector<model::Tile>& tiles,
      String& error);
};

}  // namespace homepoint::config
