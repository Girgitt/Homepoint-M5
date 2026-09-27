#pragma once

#include <Arduino.h>
#include <cstddef>

#include "HapticFeedback.h"
#include "ScreenPowerFsm.h"
#include "TileInteraction.h"
#include "UiIntent.h"
#include "../model/Model.h"

namespace homepoint::ui {

enum class UiScreen {
  Home,
  TileDetail,
};

struct UiLayoutMetrics {
  std::size_t homeItemsPerPage = 6;
  std::size_t detailItemsPerPage = 6;
};

enum class UiCommandKind {
  None,
  SetTileSwitchState,
  SetTileItemSwitchState,
};

struct UiCommand {
  UiCommandKind kind = UiCommandKind::None;
  std::size_t tileIndex = 0;
  std::size_t itemIndex = 0;
  bool targetOn = false;
};

struct UiIntentResult {
  bool handled = false;
  bool navigationChanged = false;
  bool hasHapticCue = false;
  core::HapticCue hapticCue = core::HapticCue::Press;
  core::TileAction tileAction = core::TileAction::None;
  UiCommand command;
};

// Renderer-independent Homepoint interaction/navigation state.
//
// M5GFX and future LVGL backends translate their native input events into
// UiIntent values. This controller owns what those intents mean; it never
// draws pixels and never talks to MQTT directly. Commands are returned to the
// application layer for execution.
class UiController {
 public:
  void begin(
      model::AppConfig* config,
      bool debugMode,
      UiLayoutMetrics metrics);
  void setConfig(model::AppConfig* config, UiLayoutMetrics metrics);
  void setDebugMode(bool enabled, UiLayoutMetrics metrics);
  void setLayoutMetrics(UiLayoutMetrics metrics);

  bool debugMode() const { return debugMode_; }
  UiScreen screen() const { return screen_; }
  std::size_t selectedTile() const { return selectedTile_; }
  std::size_t homePage() const { return homePage_; }
  std::size_t detailPage() const { return detailPage_; }
  UiLayoutMetrics layoutMetrics() const { return metrics_; }

  const String& message() const { return message_; }
  void setMessage(const String& message) { message_ = message; }
  void clearMessage() { message_ = ""; }

  bool footerVisible() const;
  std::size_t homePageCount() const;
  bool navigationEnabled(core::UiNavigationAction action) const;

  UiIntentResult handleIntent(const core::UiIntent& intent);

  core::ScreenPowerState screenPowerState() const {
    return screenPowerFsm_.state();
  }
  bool acceptsUiInput() const { return screenPowerFsm_.acceptsUiInput(); }
  core::ScreenPowerStep stepScreenPower(
      core::ScreenTouchPhase touch,
      bool timeoutExpired) {
    return screenPowerFsm_.step(touch, timeoutExpired);
  }
  void forceScreenAwake() { screenPowerFsm_.forceAwake(); }

 private:
  model::AppConfig* config_ = nullptr;
  bool debugMode_ = false;
  UiLayoutMetrics metrics_;
  UiScreen screen_ = UiScreen::Home;
  std::size_t selectedTile_ = 0;
  std::size_t homePage_ = 0;
  std::size_t detailPage_ = 0;
  String message_;
  core::ScreenPowerFsm screenPowerFsm_;

  void resetNavigation();
  std::size_t detailPageCount() const;
  core::TileBehavior tileBehaviorForTile(const model::Tile& tile) const;
  UiIntentResult handleTileGesture(const core::UiIntent& intent);
  UiIntentResult handleToggleTileItem(const core::UiIntent& intent);
  UiIntentResult handleNavigation(core::UiNavigationAction action);
};

}  // namespace homepoint::ui
