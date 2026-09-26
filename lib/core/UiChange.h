#pragma once

#include <cstddef>

namespace homepoint::core {

enum class UiChangeKind {
  Full,
  ModelItem,
  WifiStatus,
  MqttStatus,
  CenterStatus,
  Navigation,
  Message,
};

struct UiChange {
  UiChangeKind kind = UiChangeKind::Full;
  std::size_t tileIndex = 0;
  std::size_t itemIndex = 0;

  static UiChange full() { return {UiChangeKind::Full, 0, 0}; }
  static UiChange modelItem(std::size_t tileIndex, std::size_t itemIndex) {
    return {UiChangeKind::ModelItem, tileIndex, itemIndex};
  }
  static UiChange wifiStatus() { return {UiChangeKind::WifiStatus, 0, 0}; }
  static UiChange mqttStatus() { return {UiChangeKind::MqttStatus, 0, 0}; }
  static UiChange centerStatus() { return {UiChangeKind::CenterStatus, 0, 0}; }
  static UiChange navigation() { return {UiChangeKind::Navigation, 0, 0}; }
  static UiChange message() { return {UiChangeKind::Message, 0, 0}; }
};

}  // namespace homepoint::core
