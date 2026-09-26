#pragma once

#include <cstdint>

namespace homepoint::core {

enum class TileGesture : std::uint8_t {
  ShortPress,
  LongPress,
};

enum class TileAction : std::uint8_t {
  None,
  Toggle,
  Activate,
  OpenDetail,
  SetValue,
};

struct TileBehavior {
  TileAction shortPress = TileAction::None;
  TileAction longPress = TileAction::None;
};

constexpr TileAction resolveTileAction(
    const TileBehavior& behavior,
    TileGesture gesture) {
  return gesture == TileGesture::ShortPress ? behavior.shortPress
                                             : behavior.longPress;
}

}  // namespace homepoint::core
