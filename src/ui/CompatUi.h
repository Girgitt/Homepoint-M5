#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <atomic>
#include <vector>

#include "DeadlineScheduler.h"
#include "HapticFeedback.h"
#include "PressGesture.h"
#include "ScreenPowerFsm.h"
#include "StatusCenterCycle.h"
#include "TileInteraction.h"
#include "UiChange.h"
#include "../hardware/M5HapticMotor.h"
#include "../model/Model.h"
#include "../network/MqttManager.h"
#include "../network/WifiManager.h"

namespace homepoint::ui {

class CompatUi {
 public:
  void begin(
      model::AppConfig* config,
      network::WifiManager* wifi,
      network::MqttManager* mqtt,
      bool debugMode,
      core::DeadlineScheduler* timing);

  void setConfig(model::AppConfig* config);
  void setDebugMode(bool enabled);
  bool debugMode() const { return debugMode_; }
  void tick();

  // Renderer-independent semantic invalidation entry point. Application and
  // model/network code report what changed; this backend decides which
  // physical screen regions need repainting. A later LVGL backend can consume
  // the same UiChange values and invalidate/update LVGL objects instead.
  void applyChange(const core::UiChange& change);
  void invalidate() { applyChange(core::UiChange::full()); }

  void showMessage(const String& message);
  void clearMessage();

 private:
  enum class Screen {
    Home,
    TileDetail,
  };

  enum class FooterSlot {
    Left,
    Center,
    Right,
  };

  enum class FooterAction {
    None,
    Previous,
    Home,
    Back,
    Next,
  };

  model::AppConfig* config_ = nullptr;
  network::WifiManager* wifi_ = nullptr;
  network::MqttManager* mqtt_ = nullptr;
  bool debugMode_ = false;

  Screen screen_ = Screen::Home;
  std::size_t selectedTile_ = 0;
  std::size_t homePage_ = 0;
  std::size_t detailPage_ = 0;

  bool fullDirty_ = true;
  bool contentDirty_ = true;
  bool statusLeftDirty_ = true;
  bool statusCenterDirty_ = true;
  bool statusRightDirty_ = true;
  bool footerDirty_ = true;
  std::vector<bool> homeTileDirty_;
  std::vector<bool> detailItemDirty_;

  core::ScreenPowerFsm screenPowerFsm_;
  core::PressGestureClassifier tilePressGesture_;
  core::HapticFeedback hapticFeedback_;  // polling fallback if timing service is unavailable
  hardware::M5HapticMotor hapticMotor_;
  core::DeadlineScheduler* timing_ = nullptr;
  core::DeadlineHandle hapticOnDeadline_ = core::kInvalidDeadlineHandle;
  core::DeadlineHandle hapticOffDeadline_ = core::kInvalidDeadlineHandle;
  core::DeadlineHandle screenTimeoutDeadline_ = core::kInvalidDeadlineHandle;
  core::DeadlineHandle statusCenterDeadline_ = core::kInvalidDeadlineHandle;
  std::atomic<bool> screenTimeoutPending_{false};
  std::atomic<bool> statusCenterDeadlinePending_{false};
  core::StatusCenterCycle statusCenterCycle_;
  bool homeTilePressActive_ = false;
  std::size_t homeTilePressTile_ = 0;
  int homeTilePressStartX_ = 0;
  int homeTilePressStartY_ = 0;
  String message_;
  std::uint32_t lastInteractionAt_ = 0;  // fallback only; normal timeout uses DeadlineScheduler
  int lastClockMinute_ = -1;
  bool lastTimeAvailable_ = false;
  bool visualStatusSnapshotReady_ = false;
  bool lastWifiOnline_ = false;
  bool lastMqttOnline_ = false;
  String lastIpAddress_;
  String lastWifiStatusText_;

  void draw();
  void drawContent();
  void flushInvalidations(std::uint32_t now);
  bool hasPendingInvalidations() const;
  void clearPendingInvalidations();
  void ensureDirtyStorage();
  void markVisibleModelItemDirty(std::size_t tileIndex, std::size_t itemIndex);
  void updateTimedStatusInvalidation(std::uint32_t now);
  void resetStatusCenterCycle(std::uint32_t now);
  void scheduleStatusCenterDeadline();
  void armScreenTimeout();
  void cancelScreenTimeout();
  void registerTimingCallbacks();
  static void hapticOnDeadlineCallback(void* context);
  static void hapticOffDeadlineCallback(void* context);
  static void screenTimeoutDeadlineCallback(void* context);
  static void statusCenterDeadlineCallback(void* context);
  void markNavigationDirty();

  void drawStatusBar();
  void drawStatusBarDebug();
  void drawStatusBarUser();
  void drawStatusLeftUser();
  void drawStatusCenterUser();
  void drawStatusRightUser();
  String userStatusCenterText() const;

  void drawHome();
  void drawHomeDebug();
  void drawHomeUser();
  void drawHomeTileDebug(std::size_t tileIndex);
  void drawHomeTileUser(std::size_t tileIndex);
  void drawHomeUserGrid();
  void drawDirtyHomeTiles();

  void drawTileDetail();
  void drawTileDetailDebug();
  void drawTileDetailUser();
  void drawDetailItemDebug(std::size_t itemIndex);
  void drawDetailItemUser(std::size_t itemIndex);
  void drawDetailUserGrid();
  void drawDirtyDetailItems();

  void drawFooter();
  void drawFooterDebug();
  void drawFooterUser();
  void drawUserTileIcon(const model::Tile& tile, int centerX, int centerY, std::uint16_t color);
  void drawUserItemIcon(const model::TileItem& item, int centerX, int centerY, std::uint16_t color, const String& fallbackIcon);
  void drawNamedIcon(const String& icon, model::TileType fallbackType, int centerX, int centerY, std::uint16_t color, bool active);
  void drawWifiIcon(int centerX, int centerY, std::uint16_t color);
  void drawCloudIcon(int centerX, int centerY, std::uint16_t color);
  void drawFooterSymbol(FooterAction action, int centerX, int centerY, std::uint16_t color);
  bool footerVisible() const;
  std::size_t homePageCount() const;
  void handleTouch(const m5::touch_detail_t& touch);
  void handleHomeTouch(int x, int y);
  void handleDetailTouch(int x, int y);
  void handleDetailTouchDebug(int x, int y);
  void handleDetailTouchUser(int x, int y);
  void handleFooterTouch(int x);

  FooterSlot footerSlotForX(int x) const;
  FooterAction footerActionForSlot(FooterSlot slot) const;
  bool footerActionEnabled(FooterAction action) const;
  void executeFooterAction(FooterAction action);
  static const char* footerActionLabel(FooterAction action);

  bool beginHomeTilePress(const m5::touch_detail_t& touch, std::uint32_t now);
  void updateHomeTilePress(const m5::touch_detail_t& touch, std::uint32_t now);
  void dispatchHomeTileGesture(core::PressGesture gesture);
  void resetHomeTilePress();
  void triggerHaptic(core::HapticCue cue, std::uint32_t now);
  void updateHaptics(std::uint32_t now);
  void stopHaptics();
  bool hitTestHomeTile(int x, int y, std::size_t& tileIndex) const;
  core::TileBehavior tileBehaviorForTile(const model::Tile& tile) const;
  void executeTileAction(
      std::size_t tileIndex,
      core::TileGesture gesture,
      core::TileAction action);

  int statusHeight() const;
  std::size_t detailItemsPerPage() const;
  bool fallbackScreenTimeoutExpired(std::uint32_t now) const;
  void noteUserActivity(std::uint32_t now);
  void setDisplayPowered(bool powered);

  static String truncate(const String& value, std::size_t maxChars);
};

}  // namespace homepoint::ui
