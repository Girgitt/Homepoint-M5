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
      String& error);

 private:
  static bool appendCanonicalTiles(
      JsonArray target,
      const std::vector<model::Tile>& tiles,
      String& error);
};

}  // namespace homepoint::config
