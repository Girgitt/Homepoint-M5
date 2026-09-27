#include "UiController.h"

#include <algorithm>

namespace homepoint::ui {

void UiController::begin(
    model::AppConfig* config,
    bool debugMode,
    UiLayoutMetrics metrics) {
  config_ = config;
  debugMode_ = debugMode;
  metrics_ = metrics;
  resetNavigation();
  screenPowerFsm_.forceAwake();
}

void UiController::setConfig(
    model::AppConfig* config,
    UiLayoutMetrics metrics) {
  config_ = config;
  metrics_ = metrics;
  resetNavigation();
  screenPowerFsm_.forceAwake();
}

void UiController::setDebugMode(bool enabled, UiLayoutMetrics metrics) {
  debugMode_ = enabled;
  metrics_ = metrics;
  resetNavigation();
}

void UiController::setLayoutMetrics(UiLayoutMetrics metrics) {
  metrics_ = metrics;
  if (homePage_ >= homePageCount()) homePage_ = 0;
  if (detailPage_ >= detailPageCount()) detailPage_ = 0;
}

void UiController::resetNavigation() {
  screen_ = UiScreen::Home;
  selectedTile_ = 0;
  homePage_ = 0;
  detailPage_ = 0;
}

std::size_t UiController::homePageCount() const {
  if (!config_ || config_->tiles.empty()) return 1;
  const std::size_t perPage = std::max<std::size_t>(1, metrics_.homeItemsPerPage);
  return (config_->tiles.size() + perPage - 1) / perPage;
}

std::size_t UiController::detailPageCount() const {
  if (!config_ || selectedTile_ >= config_->tiles.size()) return 1;
  const auto& tile = config_->tiles[selectedTile_];
  if (tile.items.empty()) return 1;
  const std::size_t perPage = std::max<std::size_t>(1, metrics_.detailItemsPerPage);
  return (tile.items.size() + perPage - 1) / perPage;
}

bool UiController::footerVisible() const {
  return screen_ == UiScreen::TileDetail || homePageCount() > 1;
}

bool UiController::navigationEnabled(core::UiNavigationAction action) const {
  if (!config_) return false;

  switch (action) {
    case core::UiNavigationAction::Home:
      return true;
    case core::UiNavigationAction::Back:
      return screen_ == UiScreen::TileDetail;
    case core::UiNavigationAction::Previous:
      return screen_ == UiScreen::Home ? homePage_ > 0 : detailPage_ > 0;
    case core::UiNavigationAction::Next:
      return screen_ == UiScreen::Home
          ? homePage_ + 1 < homePageCount()
          : detailPage_ + 1 < detailPageCount();
  }
  return false;
}

core::TileBehavior UiController::tileBehaviorForTile(
    const model::Tile& tile) const {
  if (tile.switchCount() > 0) {
    return {core::TileAction::Toggle, core::TileAction::OpenDetail};
  }
  return {core::TileAction::OpenDetail, core::TileAction::OpenDetail};
}

UiIntentResult UiController::handleTileGesture(const core::UiIntent& intent) {
  UiIntentResult result;
  if (!config_ || intent.tileIndex >= config_->tiles.size()) return result;

  const auto& tile = config_->tiles[intent.tileIndex];
  result.handled = true;
  if (intent.tileGesture == core::TileGesture::LongPress) {
    result.hasHapticCue = true;
    result.hapticCue = core::HapticCue::LongPress;
  }

  const auto behavior = tileBehaviorForTile(tile);
  result.tileAction = core::resolveTileAction(behavior, intent.tileGesture);
  switch (result.tileAction) {
    case core::TileAction::Toggle:
      result.command = {
          UiCommandKind::SetTileSwitchState,
          intent.tileIndex,
          0,
          !tile.allSwitchesOn()};
      break;

    case core::TileAction::OpenDetail:
      selectedTile_ = intent.tileIndex;
      detailPage_ = 0;
      screen_ = UiScreen::TileDetail;
      result.navigationChanged = true;
      break;

    case core::TileAction::None:
    case core::TileAction::Activate:
    case core::TileAction::SetValue:
      break;
  }
  return result;
}

UiIntentResult UiController::handleToggleTileItem(const core::UiIntent& intent) {
  UiIntentResult result;
  if (!config_ || intent.tileIndex >= config_->tiles.size()) return result;
  const auto& tile = config_->tiles[intent.tileIndex];
  if (intent.itemIndex >= tile.items.size()) return result;
  const auto& item = tile.items[intent.itemIndex];
  if (item.type != model::TileItemType::Switch) return result;

  result.handled = true;
  result.hasHapticCue = true;
  result.hapticCue = core::HapticCue::Press;
  result.command = {
      UiCommandKind::SetTileItemSwitchState,
      intent.tileIndex,
      intent.itemIndex,
      !item.switchDevice.active};
  return result;
}

UiIntentResult UiController::handleNavigation(core::UiNavigationAction action) {
  UiIntentResult result;
  if (!navigationEnabled(action)) return result;

  result.handled = true;
  result.hasHapticCue = true;
  result.hapticCue = core::HapticCue::Press;
  result.navigationChanged = true;

  switch (action) {
    case core::UiNavigationAction::Previous:
      if (screen_ == UiScreen::Home) {
        --homePage_;
      } else {
        --detailPage_;
      }
      break;

    case core::UiNavigationAction::Home:
      screen_ = UiScreen::Home;
      homePage_ = 0;
      detailPage_ = 0;
      break;

    case core::UiNavigationAction::Back:
      screen_ = UiScreen::Home;
      detailPage_ = 0;
      break;

    case core::UiNavigationAction::Next:
      if (screen_ == UiScreen::Home) {
        ++homePage_;
      } else {
        ++detailPage_;
      }
      break;
  }

  return result;
}

UiIntentResult UiController::handleIntent(const core::UiIntent& intent) {
  switch (intent.kind) {
    case core::UiIntentKind::TilePressStarted: {
      UiIntentResult result;
      if (!config_ || intent.tileIndex >= config_->tiles.size()) return result;
      result.handled = true;
      result.hasHapticCue = true;
      result.hapticCue = core::HapticCue::Press;
      return result;
    }

    case core::UiIntentKind::TileGesture:
      return handleTileGesture(intent);

    case core::UiIntentKind::ToggleTileItem:
      return handleToggleTileItem(intent);

    case core::UiIntentKind::Navigate:
      return handleNavigation(intent.navigation);
  }

  return {};
}

}  // namespace homepoint::ui
