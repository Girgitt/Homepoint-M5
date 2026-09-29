#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

#include "../model/Model.h"

namespace homepoint::config {

struct LayoutDocument {
  static constexpr int kSchemaVersion = 1;
  static constexpr std::size_t kMaxNameLength = 96;

  String name;
  std::vector<model::Tile> tiles;
};

class LayoutCodec {
 public:
  static bool validFilename(const String& filename);

  static bool parse(
      const char* json,
      std::size_t length,
      LayoutDocument& layout,
      String& error);

  static bool serialize(
      const LayoutDocument& layout,
      String& output,
      String& error);
};

}  // namespace homepoint::config
