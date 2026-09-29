#include "LayoutCodec.h"

#include "DashboardCodec.h"

#include <utility>

namespace homepoint::config {
namespace {

void setError(String& error, const char* message) {
  error = message;
}

}  // namespace

bool LayoutCodec::validFilename(const String& filename) {
  if (!filename.startsWith("layout_") || !filename.endsWith(".json")) return false;
  if (filename.length() <= 12u || filename.indexOf('/') >= 0 ||
      filename.indexOf('\\') >= 0 || filename.indexOf("..") >= 0) {
    return false;
  }
  const std::size_t end = filename.length() - 5u;
  for (std::size_t i = 7u; i < end; ++i) {
    const char c = filename[i];
    const bool allowed = (c >= 'a' && c <= 'z') ||
                         (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!allowed) return false;
  }
  return end > 7u;
}

bool LayoutCodec::parse(
    const char* json,
    std::size_t length,
    LayoutDocument& layout,
    String& error) {
  JsonDocument document;
  const auto result = deserializeJson(document, json, length);
  if (result) {
    error = result == DeserializationError::NoMemory
        ? "Not enough memory to parse layout JSON"
        : String("JSON parse error: ") + result.c_str();
    return false;
  }
  if (document.overflowed()) {
    setError(error, "Not enough memory to parse layout JSON");
    return false;
  }
  if (!document.is<JsonObjectConst>()) {
    setError(error, "Layout document must be a JSON object");
    return false;
  }

  JsonObjectConst root = document.as<JsonObjectConst>();
  JsonVariantConst kind = root["kind"];
  if (!kind.is<const char*>() || String(kind.as<const char*>()) != "homepoint-layout") {
    setError(error, "Layout 'kind' must be 'homepoint-layout'");
    return false;
  }

  JsonVariantConst schema = root["schemaVersion"];
  if (!schema.is<int>() || schema.as<int>() != LayoutDocument::kSchemaVersion) {
    setError(error, "Layout schemaVersion must be 1");
    return false;
  }

  JsonVariantConst name = root["name"];
  if (!name.is<const char*>()) {
    setError(error, "Layout 'name' must be a string");
    return false;
  }
  String parsedName = name.as<const char*>();
  parsedName.trim();
  if (parsedName.isEmpty()) {
    setError(error, "Layout 'name' must not be empty");
    return false;
  }
  if (parsedName.length() > LayoutDocument::kMaxNameLength) {
    setError(error, "Layout 'name' is too long");
    return false;
  }

  JsonVariantConst tiles = root["tiles"];
  if (!tiles.is<JsonArrayConst>()) {
    setError(error, "Layout 'tiles' must be an array");
    return false;
  }

  std::vector<model::Tile> parsedTiles;
  if (!DashboardCodec::parseTiles(
          tiles.as<JsonArrayConst>(), parsedTiles, error, "tiles")) {
    return false;
  }

  layout.name = parsedName;
  layout.tiles = std::move(parsedTiles);
  error = "";
  return true;
}

bool LayoutCodec::serialize(
    const LayoutDocument& layout,
    String& output,
    String& error) {
  String name = layout.name;
  name.trim();
  if (name.isEmpty()) {
    setError(error, "Layout 'name' must not be empty");
    return false;
  }
  if (name.length() > LayoutDocument::kMaxNameLength) {
    setError(error, "Layout 'name' is too long");
    return false;
  }

  JsonDocument document;
  document["kind"] = "homepoint-layout";
  document["schemaVersion"] = LayoutDocument::kSchemaVersion;
  document["name"] = name.c_str();
  if (!DashboardCodec::appendCanonicalTiles(
          document["tiles"].to<JsonArray>(), layout.tiles, error)) {
    return false;
  }
  if (document.overflowed()) {
    setError(error, "Not enough memory to serialize layout");
    return false;
  }

  output = "";
  const std::size_t bytes = measureJsonPretty(document);
  if (!output.reserve(bytes + 1u)) {
    setError(error, "Not enough memory to allocate layout JSON");
    return false;
  }
  serializeJsonPretty(document, output);
  error = "";
  return true;
}

}  // namespace homepoint::config
