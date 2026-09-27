#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <atomic>
#include <functional>
#include <vector>

#include "DeadlineScheduler.h"
#include "HapticFeedback.h"
#include "PressGesture.h"
#include "ScreenPowerFsm.h"
#include "StatusCenterCycle.h"
#include "TileInteraction.h"
#include "UiChange.h"
#include "UiIntent.h"
#include "../hardware/M5HapticMotor.h"
#include "../model/Model.h"
#include "../network/MqttManager.h"
#include "../network/WifiManager.h"
#include "UiBackend.h"
#include "UiController.h"

namespace homepoint::ui {

class CompatUi final : public UiBackend {
 public:
  using CommandHandler = std::function<void(const UiCommand&)>;

  void begin(
      model::AppConfig* config,
      network::WifiManager* wifi,
      network::MqttManager* mqtt,
      UiController* controller,
      CommandHandler commandHandler,
      core::DeadlineScheduler* timing);

  UiLayoutMetrics layoutMetrics(bool debugMode) const override;
  void setConfig(model::AppConfig* config) override;
  void onDisplayModeChanged() override;
  bool debugMode() const { return controller_ && controller_->debugMode(); }
  void tick() override;

  // Renderer-independent semantic invalidation entry point. Application and
  // model/network code report what changed; this backend decides which
  // physical screen regions need repainting. A later LVGL backend can consume
  // the same UiChange values and invalidate/update LVGL objects instead.
  void applyChange(const core::UiChange& change) override;
  void invalidate() { applyChange(core::UiChange::full()); }

 private:
  enum class FooterSlot {
    Left,
    Center,
    Right,
  };

  model::AppConfig* config_ = nullptr;
  network::WifiManager* wifi_ = nullptr;
  network::MqttManager* mqtt_ = nullptr;
  UiController* controller_ = nullptr;
  CommandHandler commandHandler_;

  bool fullDirty_ = true;
  bool contentDirty_ = true;
  bool statusLeftDirty_ = true;
  bool statusCenterDirty_ = true;
  bool statusRightDirty_ = true;
  bool footerDirty_ = true;
  std::vector<bool> homeTileDirty_;
  std::vector<bool> detailItemDirty_;

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
  void drawFooterSymbol(core::UiNavigationAction action, int centerX, int centerY, std::uint16_t color);
  bool footerVisible() const;
  void handleTouch(const m5::touch_detail_t& touch);
  void handleHomeTouch(int x, int y);
  void handleDetailTouch(int x, int y);
  void handleDetailTouchDebug(int x, int y);
  void handleDetailTouchUser(int x, int y);
  void handleFooterTouch(int x);

  FooterSlot footerSlotForX(int x) const;
  core::UiNavigationAction footerActionForSlot(FooterSlot slot) const;
  bool footerActionEnabled(core::UiNavigationAction action) const;
  void executeFooterAction(core::UiNavigationAction action);
  static const char* footerActionLabel(core::UiNavigationAction action);

  bool beginHomeTilePress(const m5::touch_detail_t& touch, std::uint32_t now);
  void updateHomeTilePress(const m5::touch_detail_t& touch, std::uint32_t now);
  void dispatchHomeTileGesture(core::PressGesture gesture);
  void resetHomeTilePress();
  void triggerHaptic(core::HapticCue cue, std::uint32_t now);
  void updateHaptics(std::uint32_t now);
  void stopHaptics();
  bool hitTestHomeTile(int x, int y, std::size_t& tileIndex) const;
  UiIntentResult dispatchIntent(const core::UiIntent& intent, std::uint32_t now);

  int statusHeight() const;
  std::size_t detailItemsPerPage() const;
  bool fallbackScreenTimeoutExpired(std::uint32_t now) const;
  void noteUserActivity(std::uint32_t now);
  void setDisplayPowered(bool powered);

  static String truncate(const String& value, std::size_t maxChars);
};

}  // namespace homepoint::ui
