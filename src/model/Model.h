#pragma once

#include <Arduino.h>
#include <vector>

namespace homepoint::model {

enum class TileType {
  Switch,
  Sensor,
  Scene,
};

enum class TileItemType {
  Switch,
  Sensor,
};

enum class SensorType {
  SingleValue,
  CombinedValues,
};

struct SwitchDevice {
  std::uint16_t id = 0;
  String name;
  String getTopic;
  String setTopic;
  String onValue;
  String offValue;
  String icon;
  bool active = false;
};

struct SensorDevice {
  std::uint16_t id = 0;
  String name;
  SensorType type = SensorType::SingleValue;
  bool jsonData = false;
  String getTopic;
  String firstKey;
  String secondKey;
  String firstIcon;
  String secondIcon;
  String firstValue = "-";
  String secondValue = "-";
};

struct TileItem {
  std::uint16_t id = 0;
  TileItemType type = TileItemType::Switch;
  SwitchDevice switchDevice;
  SensorDevice sensorDevice;
};

struct Tile {
  std::uint16_t id = 0;
  String key;
  String name;
  String icon;
  TileType type = TileType::Scene;
  std::vector<TileItem> items;

  std::size_t switchCount() const {
    std::size_t count = 0;
    for (const auto& item : items) {
      if (item.type == TileItemType::Switch) ++count;
    }
    return count;
  }

  std::size_t sensorCount() const {
    std::size_t count = 0;
    for (const auto& item : items) {
      if (item.type == TileItemType::Sensor) ++count;
    }
    return count;
  }

  bool anySwitchOn() const {
    for (const auto& item : items) {
      if (item.type == TileItemType::Switch && item.switchDevice.active) {
        return true;
      }
    }
    return false;
  }

  bool allSwitchesOn() const {
    bool sawSwitch = false;
    for (const auto& item : items) {
      if (item.type != TileItemType::Switch) continue;
      sawSwitch = true;
      if (!item.switchDevice.active) return false;
    }
    return sawSwitch;
  }
};

struct UiConfig {
  // Short/long-press split used by dashboard tiles. This belongs to normal
  // application configuration rather than bootstrap storage.
  std::uint32_t longPressMs = 600;
};

struct MqttConfig {
  String uri;
  String username;
  String password;
};

struct HardwareConfig {
  int screenSaverMinutes = 10;
  bool screenSaverPowerSaveEnabled = true;
  bool powerFrom5vRailNotUsb = true;
  int screenRotationAngle = 1;
  bool displayColorInverted = false;

  // Retained for legacy config compatibility. They are not used to poke PMIC
  // rails directly in Homepoint-M5.
  int powerSaveMHz = 80;
  bool ledPinPullup = false;
  bool touchXAxisInverted = false;
  bool touchYAxisInverted = true;
};

struct AppConfig {
  static constexpr int kCurrentSchemaVersion = 2;

  int schemaVersion = kCurrentSchemaVersion;
  bool loadedFromLegacyScenes = false;
  MqttConfig mqtt;
  UiConfig ui;
  HardwareConfig hardware;
  String timezone;
  std::vector<Tile> tiles;
};

}  // namespace homepoint::model
