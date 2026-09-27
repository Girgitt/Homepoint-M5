#pragma once

#include <cstddef>
#include <cstdint>

#include "TileInteraction.h"

namespace homepoint::core {

enum class UiIntentKind : std::uint8_t {
  TilePressStarted,
  TileGesture,
  ToggleTileItem,
  Navigate,
};

enum class UiNavigationAction : std::uint8_t {
  Previous,
  Home,
  Back,
  Next,
};

struct UiIntent {
  UiIntentKind kind = UiIntentKind::Navigate;
  std::size_t tileIndex = 0;
  std::size_t itemIndex = 0;
  TileGesture tileGesture = TileGesture::ShortPress;
  UiNavigationAction navigation = UiNavigationAction::Home;

  static UiIntent tilePressStarted(std::size_t tileIndex) {
    UiIntent intent;
    intent.kind = UiIntentKind::TilePressStarted;
    intent.tileIndex = tileIndex;
    return intent;
  }

  static UiIntent tileGestureIntent(
      std::size_t tileIndex,
      TileGesture gesture) {
    UiIntent intent;
    intent.kind = UiIntentKind::TileGesture;
    intent.tileIndex = tileIndex;
    intent.tileGesture = gesture;
    return intent;
  }

  static UiIntent toggleTileItem(
      std::size_t tileIndex,
      std::size_t itemIndex) {
    UiIntent intent;
    intent.kind = UiIntentKind::ToggleTileItem;
    intent.tileIndex = tileIndex;
    intent.itemIndex = itemIndex;
    return intent;
  }

  static UiIntent navigate(UiNavigationAction action) {
    UiIntent intent;
    intent.kind = UiIntentKind::Navigate;
    intent.navigation = action;
    return intent;
  }
};

}  // namespace homepoint::core
