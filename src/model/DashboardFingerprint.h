#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

#include "Model.h"

namespace homepoint::model {
namespace dashboard_fingerprint_detail {

inline void addByte(std::uint64_t& hash, std::uint8_t value) {
  hash ^= value;
  hash *= 1099511628211ULL;
}

inline void addU32(std::uint64_t& hash, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    addByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xffu));
  }
}

inline void addString(std::uint64_t& hash, const String& value) {
  addU32(hash, static_cast<std::uint32_t>(value.length()));
  for (std::size_t i = 0; i < value.length(); ++i) {
    addByte(hash, static_cast<std::uint8_t>(value[i]));
  }
}

inline void addSwitch(std::uint64_t& hash, const SwitchDevice& device) {
  addString(hash, device.name);
  addString(hash, device.getTopic);
  addString(hash, device.setTopic);
  addString(hash, device.onValue);
  addString(hash, device.offValue);
  addString(hash, device.icon);
}

inline void addSensor(std::uint64_t& hash, const SensorDevice& device) {
  addString(hash, device.name);
  addByte(hash, static_cast<std::uint8_t>(device.type));
  addByte(hash, device.jsonData ? 1u : 0u);
  addString(hash, device.getTopic);
  addString(hash, device.firstKey);
  addString(hash, device.secondKey);
  addString(hash, device.firstIcon);
  addString(hash, device.secondIcon);
}

}  // namespace dashboard_fingerprint_detail

// Stable identity of the normalized dashboard configuration. Runtime-only
// values (known/active/value/age) are intentionally excluded so MQTT updates do
// not make a saved dashboard appear to have changed.
inline String dashboardFingerprint(const AppConfig& config) {
  using namespace dashboard_fingerprint_detail;
  std::uint64_t hash = 14695981039346656037ULL;  // FNV-1a 64-bit offset basis

  addU32(hash, static_cast<std::uint32_t>(config.tiles.size()));
  for (const auto& tile : config.tiles) {
    addByte(hash, static_cast<std::uint8_t>(tile.type));
    addString(hash, tile.key);
    addString(hash, tile.name);
    addString(hash, tile.icon);
    addU32(hash, static_cast<std::uint32_t>(tile.items.size()));

    for (const auto& item : tile.items) {
      addByte(hash, static_cast<std::uint8_t>(item.type));
      if (item.type == TileItemType::Switch) {
        addSwitch(hash, item.switchDevice);
      } else {
        addSensor(hash, item.sensorDevice);
      }
    }
  }

  static const char hex[] = "0123456789abcdef";
  char output[17];
  for (int i = 0; i < 16; ++i) {
    const int shift = (15 - i) * 4;
    output[i] = hex[(hash >> shift) & 0x0fULL];
  }
  output[16] = '\0';
  return String(output);
}

}  // namespace homepoint::model
