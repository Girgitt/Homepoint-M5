#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#include "PressGesture.h"
#include "ScreenPowerFsm.h"
#include "TileInteraction.h"
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
      bool debugMode);

  void setConfig(model::AppConfig* config);
  void setDebugMode(bool enabled);
  bool debugMode() const { return debugMode_; }
  void tick();
  void invalidate() { dirty_ = true; }
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

  bool dirty_ = true;
  core::ScreenPowerFsm screenPowerFsm_;
  core::PressGestureClassifier tilePressGesture_;
  bool homeTilePressActive_ = false;
  std::size_t homeTilePressTile_ = 0;
  int homeTilePressStartX_ = 0;
  int homeTilePressStartY_ = 0;
  String message_;
  std::uint32_t lastInteractionAt_ = 0;
  std::uint32_t lastStatusDrawAt_ = 0;

  void draw();
  void drawStatusBar();
  void drawStatusBarDebug();
  void drawStatusBarUser();
  void drawHome();
  void drawHomeDebug();
  void drawHomeUser();
  void drawTileDetail();
  void drawTileDetailDebug();
  void drawTileDetailUser();
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
  bool hitTestHomeTile(int x, int y, std::size_t& tileIndex) const;
  core::TileBehavior tileBehaviorForTile(const model::Tile& tile) const;
  void executeTileAction(
      std::size_t tileIndex,
      core::TileGesture gesture,
      core::TileAction action);

  int statusHeight() const;
  std::size_t detailItemsPerPage() const;
  bool screenTimeoutExpired(std::uint32_t now) const;
  void setDisplayPowered(bool powered);

  static String truncate(const String& value, std::size_t maxChars);
};

}  // namespace homepoint::ui
