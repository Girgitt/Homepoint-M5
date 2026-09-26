#include <LittleFS.h>

#include "CompatUi.h"
#include <WiFi.h>
#include <ctime>

namespace homepoint::ui {
namespace {

constexpr int kDebugStatusHeight = 24;
constexpr int kUserStatusHeight = 20;
constexpr int kFooterHeight = 24;
constexpr int kColumns = 3;
constexpr int kRows = 2;
constexpr int kTilesPerPage = kColumns * kRows;
constexpr int kDebugDetailItemsPerPage = 4;
constexpr int kUserDetailItemsPerPage = 6;
constexpr int kTilePressMoveTolerance = 18;
constexpr std::uint32_t kDefaultLongPressMs = 600;
constexpr std::uint32_t kUserStatusAlternateMs = 4000;
constexpr int kUserTileVisualBottom = 72;
constexpr int kUserCaptionHeight = 8;

std::uint16_t originalGridColor() {
  return M5.Display.color565(79, 102, 106);
}

std::uint16_t originalActiveColor() {
  return M5.Display.color565(224, 211, 96);
}

core::PressPhase pressPhaseForTouch(const m5::touch_detail_t& touch) {
  if (touch.wasPressed()) return core::PressPhase::Pressed;
  if (touch.isPressed()) return core::PressPhase::Held;
  if (touch.wasReleased()) return core::PressPhase::Released;
  return core::PressPhase::None;
}

const char* tileGestureText(core::TileGesture gesture) {
  return gesture == core::TileGesture::ShortPress ? "short" : "long";
}

const char* tileActionText(core::TileAction action) {
  switch (action) {
    case core::TileAction::None: return "none";
    case core::TileAction::Toggle: return "toggle";
    case core::TileAction::Activate: return "activate";
    case core::TileAction::OpenDetail: return "open-detail";
    case core::TileAction::SetValue: return "set-value";
  }
  return "unknown";
}

String stateIconPath(const String& icon, bool active) {
  if (icon.isEmpty()) return "";
  String path = "/";
  path += icon;
  path += active ? "_active.jpg" : "_inactive.jpg";
  return path;
}

bool drawJpgAsset(
    const String& path,
    int x,
    int y,
    int maxWidth = 0,
    int maxHeight = 0) {
  if (path.isEmpty() || !LittleFS.exists(path)) return false;
  return M5.Display.drawJpgFile(
      LittleFS,
      path.c_str(),
      x,
      y,
      maxWidth,
      maxHeight,
      0,
      0,
      1.0f);
}

void drawCenteredText(
    const String& text,
    int centerX,
    int y,
    std::uint16_t color,
    std::uint16_t background = TFT_BLACK) {
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(color, background);
  const int width = M5.Display.textWidth(text.c_str());
  M5.Display.setCursor(centerX - width / 2, y);
  M5.Display.print(text);
}

int userTileCaptionY(int tileTop, int tileHeight) {
  const int remaining = tileHeight - kUserTileVisualBottom;
  if (remaining <= kUserCaptionHeight) {
    return tileTop + tileHeight - kUserCaptionHeight;
  }
  return tileTop + kUserTileVisualBottom
      + (remaining - kUserCaptionHeight) / 2;
}

}  // namespace

void CompatUi::begin(
    model::AppConfig* config,
    network::WifiManager* wifi,
    network::MqttManager* mqtt,
    bool debugMode) {
  config_ = config;
  wifi_ = wifi;
  mqtt_ = mqtt;
  debugMode_ = debugMode;
  lastInteractionAt_ = millis();

  M5.Display.setRotation(config_ ? config_->hardware.screenRotationAngle : 1);
  M5.Display.invertDisplay(config_ ? config_->hardware.displayColorInverted : false);
  M5.Display.setBrightness(96);
  M5.Display.setTextWrap(false);
  Serial.printf(
      "[UI] mode=%s long-press=%lu ms\n",
      debugMode_ ? "debug" : "user",
      static_cast<unsigned long>(config_ ? config_->ui.longPressMs
                                         : kDefaultLongPressMs));
  dirty_ = true;
}

void CompatUi::setConfig(model::AppConfig* config) {
  config_ = config;
  screen_ = Screen::Home;
  selectedTile_ = 0;
  homePage_ = 0;
  detailPage_ = 0;
  resetHomeTilePress();

  if (config_) {
    M5.Display.setRotation(config_->hardware.screenRotationAngle);
    M5.Display.invertDisplay(config_->hardware.displayColorInverted);
    Serial.printf(
        "[UI] gesture config applied: long-press=%lu ms\n",
        static_cast<unsigned long>(config_->ui.longPressMs));
  }
  screenPowerFsm_.forceAwake();
  lastInteractionAt_ = millis();
  setDisplayPowered(true);
  dirty_ = true;
}

void CompatUi::setDebugMode(bool enabled) {
  if (debugMode_ == enabled) return;
  debugMode_ = enabled;
  screen_ = Screen::Home;
  homePage_ = 0;
  detailPage_ = 0;
  resetHomeTilePress();
  dirty_ = true;
}

void CompatUi::tick() {
  const auto now = millis();

  const m5::touch_detail_t* touch = nullptr;
  core::ScreenTouchPhase touchPhase = core::ScreenTouchPhase::None;
  if (M5.Touch.getCount()) {
    touch = &M5.Touch.getDetail(0);
    touchPhase = touch->isPressed()
        ? core::ScreenTouchPhase::Pressed
        : core::ScreenTouchPhase::Released;
  }

  const auto previousPowerState = screenPowerFsm_.state();
  const auto powerStep =
      screenPowerFsm_.step(touchPhase, screenTimeoutExpired(now));

  if (powerStep.action == core::ScreenPowerAction::TurnOff) {
    Serial.println("[UI] screen FSM: AWAKE -> OFF (timeout)");
    resetHomeTilePress();
    setDisplayPowered(false);
  } else if (powerStep.action == core::ScreenPowerAction::TurnOn) {
    Serial.println("[UI] screen FSM: OFF -> WAKE_GUARD (wake touch consumed)");
    resetHomeTilePress();
    lastInteractionAt_ = now;
    setDisplayPowered(true);
  } else if (powerStep.stateChanged &&
             previousPowerState == core::ScreenPowerState::WakeGuard &&
             screenPowerFsm_.state() == core::ScreenPowerState::Awake) {
    Serial.println("[UI] screen FSM: WAKE_GUARD -> AWAKE (wake gesture released)");
  }

  if (powerStep.consumeTouch) {
    resetHomeTilePress();
    return;
  }
  if (!screenPowerFsm_.acceptsUiInput()) return;

  if (!message_.isEmpty()) {
    resetHomeTilePress();
  } else if (touch) {
    lastInteractionAt_ = now;

    if (homeTilePressActive_) {
      updateHomeTilePress(*touch, now);
    } else if (screen_ == Screen::Home && touch->wasPressed() &&
               beginHomeTilePress(*touch, now)) {
      // Tile press captured. Semantic action is emitted by the generic
      // short/long-press classifier.
    } else if (touch->wasClicked()) {
      handleTouch(*touch);
    }
  } else if (homeTilePressActive_) {
    const auto event = tilePressGesture_.step(
        core::PressPhase::None,
        now,
        config_ ? config_->ui.longPressMs : kDefaultLongPressMs);
    dispatchHomeTileGesture(event);
    resetHomeTilePress();
  }

  if (dirty_) {
    draw();
    dirty_ = false;
    lastStatusDrawAt_ = now;
  } else if (now - lastStatusDrawAt_ >= 1000u) {
    drawStatusBar();
    lastStatusDrawAt_ = now;
  }
}

bool CompatUi::screenTimeoutExpired(std::uint32_t now) const {
  if (!config_) return false;
  const int minutes = config_->hardware.screenSaverMinutes;
  if (minutes <= 0) return false;

  const std::uint32_t timeoutMs =
      static_cast<std::uint32_t>(minutes) * 60u * 1000u;
  return now - lastInteractionAt_ >= timeoutMs;
}

void CompatUi::setDisplayPowered(bool powered) {
  if (config_ && config_->hardware.screenSaverPowerSaveEnabled) {
    WiFi.setSleep(!powered);
  }
  M5.Display.setBrightness(powered ? 96 : 0);
  if (powered) dirty_ = true;
}

int CompatUi::statusHeight() const {
  return debugMode_ ? kDebugStatusHeight : kUserStatusHeight;
}

std::size_t CompatUi::detailItemsPerPage() const {
  return debugMode_ ? kDebugDetailItemsPerPage : kUserDetailItemsPerPage;
}

void CompatUi::draw() {
  M5.Display.fillScreen(TFT_BLACK);
  drawStatusBar();
  if (!message_.isEmpty()) {
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(8, statusHeight() + 16);
    M5.Display.print(message_);
    return;
  }
  if (!config_) {
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(8, statusHeight() + 16);
    M5.Display.print("No configuration");
    return;
  }

  if (screen_ == Screen::Home) {
    drawHome();
  } else {
    drawTileDetail();
  }
}

void CompatUi::drawStatusBar() {
  if (debugMode_) {
    drawStatusBarDebug();
  } else {
    drawStatusBarUser();
  }
}

void CompatUi::drawStatusBarDebug() {
  M5.Display.fillRect(0, 0, M5.Display.width(), kDebugStatusHeight, TFT_DARKGREY);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.setCursor(4, 7);

  String wifi = wifi_ ? wifi_->statusText() : "NO WIFI";
  String mqtt = mqtt_ ? mqtt_->statusText() : "NO MQTT";
  String text = "W:" + wifi + " M:" + mqtt;

  if (wifi_ && wifi_->stationConnected()) {
    text += " IP:";
    text += wifi_->ipAddress();
  }

  std::time_t now = std::time(nullptr);
  if (now > 100000) {
    std::tm local{};
    localtime_r(&now, &local);
    char clock[8];
    std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
    text += " ";
    text += clock;
  }

  M5.Display.print(truncate(text, 46));
}

void CompatUi::drawStatusBarUser() {
  const int width = M5.Display.width();
  M5.Display.fillRect(0, 0, width, kUserStatusHeight, TFT_BLACK);

  const bool wifiOnline = wifi_ && wifi_->stationConnected();
  const bool mqttOnline = mqtt_ && mqtt_->state() == network::MqttState::Connected;

  const String wifiPath = wifiOnline ? "/wifi_on.jpg" : "/wifi_off.jpg";
  if (!drawJpgAsset(wifiPath, 0, 0)) {
    drawWifiIcon(10, 10, wifiOnline ? TFT_WHITE : originalGridColor());
  }

  const String mqttPath = mqttOnline ? "/mqtt_on.jpg" : "/mqtt_off.jpg";
  if (!drawJpgAsset(mqttPath, width - 25, 0)) {
    drawCloudIcon(width - 12, 10, mqttOnline ? TFT_WHITE : originalGridColor());
  }

  String center;
  if (screen_ == Screen::TileDetail && config_ && selectedTile_ < config_->tiles.size()) {
    center = config_->tiles[selectedTile_].name;
  } else {
    std::time_t now = std::time(nullptr);
    const bool haveTime = now > 100000;
    const bool showIp = !haveTime || ((millis() / kUserStatusAlternateMs) % 2u == 0u);

    if (showIp && wifiOnline) {
      center = wifi_->ipAddress();
    } else if (haveTime) {
      std::tm local{};
      localtime_r(&now, &local);
      char clock[8];
      std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
      center = clock;
    } else if (wifi_) {
      center = wifi_->statusText();
    }
  }

  center = truncate(center, 30);
  drawCenteredText(center, width / 2, 6, TFT_WHITE, TFT_BLACK);
}

void CompatUi::drawHome() {
  if (debugMode_) {
    drawHomeDebug();
  } else {
    drawHomeUser();
  }
}

void CompatUi::drawHomeDebug() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int footerHeight = footerVisible() ? kFooterHeight : 0;
  const int contentHeight = height - kDebugStatusHeight - footerHeight;
  const int tileW = width / kColumns;
  const int tileH = contentHeight / kRows;

  const std::size_t first = homePage_ * kTilesPerPage;

  for (int slot = 0; slot < kTilesPerPage; ++slot) {
    const std::size_t tileIndex = first + slot;
    if (!config_ || tileIndex >= config_->tiles.size()) break;

    auto& tile = config_->tiles[tileIndex];
    const int col = slot % kColumns;
    const int row = slot / kColumns;
    const int x = col * tileW;
    const int y = kDebugStatusHeight + row * tileH;

    std::uint16_t fill = TFT_NAVY;
    if (tile.switchCount() > 0 && tile.anySwitchOn()) {
      fill = TFT_DARKGREEN;
    }

    M5.Display.fillRect(x + 2, y + 2, tileW - 4, tileH - 4, fill);
    M5.Display.drawRect(x + 2, y + 2, tileW - 4, tileH - 4, TFT_LIGHTGREY);

    M5.Display.setTextColor(TFT_WHITE, fill);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(x + 7, y + 10);
    M5.Display.print(truncate(tile.name, 14));

    M5.Display.setCursor(x + 7, y + 27);
    switch (tile.type) {
      case model::TileType::Switch:
        M5.Display.print("SWITCH ");
        M5.Display.print(tile.allSwitchesOn() ? "ON" : "OFF");
        break;

      case model::TileType::Sensor:
        M5.Display.print("SENSOR ");
        if (!tile.items.empty() &&
            tile.items.front().type == model::TileItemType::Sensor) {
          M5.Display.print(truncate(tile.items.front().sensorDevice.firstValue, 8));
        }
        break;

      case model::TileType::Scene:
        M5.Display.print("SCENE (");
        M5.Display.print(static_cast<unsigned>(tile.items.size()));
        M5.Display.print(")");
        break;
    }

    M5.Display.setCursor(x + 7, y + tileH - 28);
    if (tile.switchCount() > 0) {
      M5.Display.print("tap=toggle");
    } else {
      M5.Display.print("tap=detail");
    }
    M5.Display.setCursor(x + 7, y + tileH - 16);
    M5.Display.print("hold=detail");
  }

  if (footerVisible()) drawFooter();
}

void CompatUi::drawHomeUser() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int footerHeight = footerVisible() ? kFooterHeight : 0;
  const int contentTop = kUserStatusHeight;
  const int contentHeight = height - contentTop - footerHeight;
  const int tileW = width / kColumns;
  const int tileH = contentHeight / kRows;
  const std::size_t first = homePage_ * kTilesPerPage;
  const auto grid = originalGridColor();

  M5.Display.fillRect(0, contentTop, width, contentHeight, TFT_BLACK);

  for (int slot = 0; slot < kTilesPerPage; ++slot) {
    const std::size_t tileIndex = first + slot;
    if (!config_ || tileIndex >= config_->tiles.size()) continue;

    const auto& tile = config_->tiles[tileIndex];
    const int col = slot % kColumns;
    const int row = slot / kColumns;
    const int x = col * tileW;
    const int y = contentTop + row * tileH;
    const int centerX = x + tileW / 2;
    const bool activeText = tile.switchCount() > 0 && tile.anySwitchOn();
    const auto textColor = activeText ? originalActiveColor() : TFT_WHITE;

    if (tile.type == model::TileType::Sensor && !tile.items.empty() &&
        tile.items.front().type == model::TileItemType::Sensor) {
      const auto& sensor = tile.items.front().sensorDevice;
      const bool combined = sensor.type == model::SensorType::CombinedValues;
      const int valueY = combined ? y + 20 : y + tileH / 2 - 20;
      const int iconX = centerX - 34;
      String firstIconPath;
      if (!sensor.firstIcon.isEmpty()) {
        firstIconPath = "/";
        firstIconPath += sensor.firstIcon;
        firstIconPath += ".jpg";
      }
      if (!drawJpgAsset(firstIconPath, iconX, valueY - 4)) {
        drawNamedIcon("", model::TileType::Sensor, iconX + 12, valueY + 8, TFT_WHITE, false);
      }
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Display.setTextSize(2);
      M5.Display.setCursor(centerX - 5, valueY);
      M5.Display.print(truncate(sensor.firstValue, 7));

      if (combined) {
        const int secondY = valueY + 30;
        String secondIconPath = firstIconPath;
        if (!sensor.secondIcon.isEmpty()) {
          secondIconPath = "/";
          secondIconPath += sensor.secondIcon;
          secondIconPath += ".jpg";
        }
        if (!drawJpgAsset(secondIconPath, iconX, secondY - 4)) {
          drawNamedIcon("", model::TileType::Sensor, iconX + 12, secondY + 8, TFT_WHITE, false);
        }
        M5.Display.setCursor(centerX - 5, secondY);
        M5.Display.print(truncate(sensor.secondValue, 7));
      }
    } else {
      drawUserTileIcon(tile, centerX, y + 47, textColor);
    }

    drawCenteredText(truncate(tile.name, 15), centerX, userTileCaptionY(y, tileH), textColor);

    if (tile.type == model::TileType::Scene && tile.items.size() > 1) {
      M5.Display.setTextColor(textColor, TFT_BLACK);
      M5.Display.setTextSize(1);
      M5.Display.setCursor(x + tileW - 19, y + 10);
      M5.Display.print("...");
    }
  }

  for (int col = 1; col < kColumns; ++col) {
    M5.Display.drawFastVLine(col * tileW, contentTop, contentHeight, grid);
  }
  M5.Display.drawFastHLine(0, contentTop + tileH, width, grid);

  if (footerVisible()) drawFooter();
}

void CompatUi::drawTileDetail() {
  if (!config_ || selectedTile_ >= config_->tiles.size()) {
    screen_ = Screen::Home;
    dirty_ = true;
    return;
  }

  if (debugMode_) {
    drawTileDetailDebug();
  } else {
    drawTileDetailUser();
  }
}

void CompatUi::drawTileDetailDebug() {
  auto& tile = config_->tiles[selectedTile_];
  const int width = M5.Display.width();

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(5, kDebugStatusHeight + 6);
  M5.Display.print(truncate(tile.name, 28));

  constexpr int firstY = kDebugStatusHeight + 28;
  constexpr int rowHeight = 42;

  for (int row = 0; row < kDebugDetailItemsPerPage; ++row) {
    const std::size_t idx = detailPage_ * kDebugDetailItemsPerPage + row;
    if (idx >= tile.items.size()) break;
    const int y = firstY + row * rowHeight;
    auto& item = tile.items[idx];

    if (item.type == model::TileItemType::Switch) {
      auto& d = item.switchDevice;
      const auto fill = d.active ? TFT_DARKGREEN : TFT_DARKGREY;
      M5.Display.fillRect(5, y, width - 10, rowHeight - 4, fill);
      M5.Display.setTextColor(TFT_WHITE, fill);
      M5.Display.setCursor(10, y + 7);
      M5.Display.print(truncate(d.name, 28));
      M5.Display.setCursor(width - 50, y + 7);
      M5.Display.print(d.active ? "ON" : "OFF");
    } else {
      auto& d = item.sensorDevice;
      M5.Display.fillRect(5, y, width - 10, rowHeight - 4, TFT_NAVY);
      M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
      M5.Display.setCursor(10, y + 6);
      M5.Display.print(truncate(d.name, 24));
      M5.Display.setCursor(10, y + 20);
      M5.Display.print(truncate(d.firstValue, 16));
      if (d.type == model::SensorType::CombinedValues) {
        M5.Display.print(" / ");
        M5.Display.print(truncate(d.secondValue, 12));
      }
    }
  }

  drawFooter();
}

void CompatUi::drawTileDetailUser() {
  const auto& tile = config_->tiles[selectedTile_];
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int contentTop = kUserStatusHeight;
  const int contentBottom = height - kFooterHeight;
  const int contentHeight = contentBottom - contentTop;
  const auto grid = originalGridColor();

  M5.Display.fillRect(0, contentTop, width, contentHeight, TFT_BLACK);

  if (tile.items.size() == 1 && tile.type != model::TileType::Scene) {
    const auto& item = tile.items.front();
    const int centerX = width / 2;
    const int centerY = contentTop + contentHeight / 2 - 12;

    if (item.type == model::TileItemType::Switch) {
      const auto color = item.switchDevice.active ? originalActiveColor() : TFT_WHITE;
      drawUserItemIcon(item, centerX, centerY - 20, color, tile.icon);
      drawCenteredText(item.switchDevice.active ? "ON" : "OFF", centerX, centerY + 28, color);
      drawCenteredText(truncate(item.switchDevice.name, 28), centerX, centerY + 48, color);
    } else {
      const auto& sensor = item.sensorDevice;
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Display.setTextSize(3);
      const String first = truncate(sensor.firstValue, 10);
      M5.Display.setCursor(centerX - M5.Display.textWidth(first.c_str()) / 2, centerY - 20);
      M5.Display.print(first);
      if (sensor.type == model::SensorType::CombinedValues) {
        M5.Display.setTextSize(2);
        const String second = truncate(sensor.secondValue, 12);
        M5.Display.setCursor(centerX - M5.Display.textWidth(second.c_str()) / 2, centerY + 20);
        M5.Display.print(second);
      }
      drawCenteredText(truncate(sensor.name, 28), centerX, centerY + 54, TFT_WHITE);
    }

    drawFooter();
    return;
  }

  const int tileW = width / kColumns;
  const int tileH = contentHeight / kRows;
  const std::size_t first = detailPage_ * kUserDetailItemsPerPage;

  for (int slot = 0; slot < kUserDetailItemsPerPage; ++slot) {
    const std::size_t idx = first + slot;
    if (idx >= tile.items.size()) continue;
    const auto& item = tile.items[idx];
    const int col = slot % kColumns;
    const int row = slot / kColumns;
    const int x = col * tileW;
    const int y = contentTop + row * tileH;
    const int centerX = x + tileW / 2;

    std::uint16_t color = TFT_WHITE;
    if (item.type == model::TileItemType::Switch && item.switchDevice.active) {
      color = originalActiveColor();
    }

    drawUserItemIcon(item, centerX, y + 46, color, tile.icon);
    const String label = item.type == model::TileItemType::Switch
        ? item.switchDevice.name
        : item.sensorDevice.name;
    drawCenteredText(truncate(label, 15), centerX, userTileCaptionY(y, tileH), color);
  }

  for (int col = 1; col < kColumns; ++col) {
    M5.Display.drawFastVLine(col * tileW, contentTop, contentHeight, grid);
  }
  M5.Display.drawFastHLine(0, contentTop + tileH, width, grid);
  drawFooter();
}

void CompatUi::drawFooter() {
  if (!footerVisible()) return;
  if (debugMode_) {
    drawFooterDebug();
  } else {
    drawFooterUser();
  }
}

void CompatUi::drawFooterDebug() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int y = height - kFooterHeight;
  const int slotW = width / 3;

  M5.Display.fillRect(0, y, width, kFooterHeight, TFT_BLACK);
  M5.Display.drawFastHLine(0, y, width, TFT_DARKGREY);
  M5.Display.drawFastVLine(slotW, y, kFooterHeight, TFT_DARKGREY);
  M5.Display.drawFastVLine(slotW * 2, y, kFooterHeight, TFT_DARKGREY);
  M5.Display.setTextSize(1);

  const FooterSlot slots[] = {
      FooterSlot::Left,
      FooterSlot::Center,
      FooterSlot::Right,
  };

  for (int index = 0; index < 3; ++index) {
    const auto action = footerActionForSlot(slots[index]);
    const bool enabled = footerActionEnabled(action);
    const char* label = footerActionLabel(action);
    const int x0 = index * slotW;
    const int x1 = index == 2 ? width : x0 + slotW;
    const int labelWidth = M5.Display.textWidth(label);
    const int textX = x0 + ((x1 - x0) - labelWidth) / 2;
    const int textY = y + (kFooterHeight - 8) / 2;

    M5.Display.setTextColor(enabled ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(textX, textY);
    M5.Display.print(label);
  }
}

void CompatUi::drawFooterUser() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int y = height - kFooterHeight;
  const int slotW = width / 3;
  const auto grid = originalGridColor();

  M5.Display.fillRect(0, y, width, kFooterHeight, TFT_BLACK);
  M5.Display.drawFastHLine(0, y, width, grid);
  M5.Display.drawFastVLine(slotW, y, kFooterHeight, grid);
  M5.Display.drawFastVLine(slotW * 2, y, kFooterHeight, grid);

  const FooterSlot slots[] = {FooterSlot::Left, FooterSlot::Center, FooterSlot::Right};
  for (int index = 0; index < 3; ++index) {
    const auto action = footerActionForSlot(slots[index]);
    const bool enabled = footerActionEnabled(action);
    drawFooterSymbol(
        action,
        index * slotW + slotW / 2,
        y + kFooterHeight / 2,
        enabled ? TFT_WHITE : grid);
  }
}

void CompatUi::drawUserTileIcon(
    const model::Tile& tile,
    int centerX,
    int centerY,
    std::uint16_t color) {
  String icon = tile.icon;
  if (icon.isEmpty() && !tile.items.empty() &&
      tile.items.front().type == model::TileItemType::Switch) {
    icon = tile.items.front().switchDevice.icon;
  }
  drawNamedIcon(icon, tile.type, centerX, centerY, color, tile.allSwitchesOn());
}

void CompatUi::drawUserItemIcon(
    const model::TileItem& item,
    int centerX,
    int centerY,
    std::uint16_t color,
    const String& fallbackIcon) {
  if (item.type == model::TileItemType::Switch) {
    const auto& d = item.switchDevice;
    const String icon = d.icon.isEmpty() ? fallbackIcon : d.icon;
    if (!icon.isEmpty() &&
        drawJpgAsset(stateIconPath(icon, d.active), centerX - 25, centerY - 25, 50, 50)) {
      return;
    }
    drawNamedIcon("", model::TileType::Switch, centerX, centerY, color, d.active);
    return;
  }

  const auto& d = item.sensorDevice;
  String path;
  if (!d.firstIcon.isEmpty()) {
    path = "/";
    path += d.firstIcon;
    path += ".jpg";
  }
  if (drawJpgAsset(path, centerX - 12, centerY - 12, 25, 25)) return;

  if (!fallbackIcon.isEmpty() &&
      drawJpgAsset(stateIconPath(fallbackIcon, false), centerX - 25, centerY - 25, 50, 50)) {
    return;
  }
  drawNamedIcon("", model::TileType::Sensor, centerX, centerY, color, false);
}

void CompatUi::drawNamedIcon(
    const String& icon,
    model::TileType fallbackType,
    int centerX,
    int centerY,
    std::uint16_t color,
    bool active) {
  if (!icon.isEmpty() &&
      drawJpgAsset(stateIconPath(icon, active), centerX - 25, centerY - 25, 50, 50)) {
    return;
  }

  switch (fallbackType) {
    case model::TileType::Switch:
      M5.Display.drawRoundRect(centerX - 16, centerY - 23, 32, 46, 5, color);
      M5.Display.fillCircle(centerX + 7, centerY, 2, color);
      break;

    case model::TileType::Sensor:
      M5.Display.drawCircle(centerX, centerY + 12, 7, color);
      M5.Display.fillCircle(centerX, centerY + 12, 4, color);
      M5.Display.drawRoundRect(centerX - 3, centerY - 18, 6, 28, 3, color);
      break;

    case model::TileType::Scene:
      M5.Display.drawTriangle(
          centerX - 23, centerY - 4,
          centerX, centerY - 24,
          centerX + 23, centerY - 4,
          color);
      M5.Display.drawRect(centerX - 18, centerY - 4, 36, 27, color);
      M5.Display.drawRect(centerX - 5, centerY + 7, 10, 16, color);
      break;
  }
}

void CompatUi::drawWifiIcon(int centerX, int centerY, std::uint16_t color) {
  // Small dependency-free fallback glyph. The preferred path uses the
  // original /wifi_on.jpg and /wifi_off.jpg assets when present.
  M5.Display.drawLine(centerX - 9, centerY - 4, centerX, centerY - 8, color);
  M5.Display.drawLine(centerX, centerY - 8, centerX + 9, centerY - 4, color);
  M5.Display.drawLine(centerX - 6, centerY, centerX, centerY - 3, color);
  M5.Display.drawLine(centerX, centerY - 3, centerX + 6, centerY, color);
  M5.Display.drawLine(centerX - 3, centerY + 4, centerX, centerY + 2, color);
  M5.Display.drawLine(centerX, centerY + 2, centerX + 3, centerY + 4, color);
  M5.Display.fillCircle(centerX, centerY + 7, 1, color);
}

void CompatUi::drawCloudIcon(int centerX, int centerY, std::uint16_t color) {
  M5.Display.drawCircle(centerX - 5, centerY, 5, color);
  M5.Display.drawCircle(centerX + 1, centerY - 3, 6, color);
  M5.Display.drawCircle(centerX + 7, centerY + 1, 4, color);
  M5.Display.drawFastHLine(centerX - 8, centerY + 5, 18, color);
}

void CompatUi::drawFooterSymbol(
    FooterAction action,
    int centerX,
    int centerY,
    std::uint16_t color) {
  switch (action) {
    case FooterAction::Previous:
      M5.Display.drawLine(centerX + 5, centerY - 7, centerX - 4, centerY, color);
      M5.Display.drawLine(centerX - 4, centerY, centerX + 5, centerY + 7, color);
      break;

    case FooterAction::Next:
      M5.Display.drawLine(centerX - 5, centerY - 7, centerX + 4, centerY, color);
      M5.Display.drawLine(centerX + 4, centerY, centerX - 5, centerY + 7, color);
      break;

    case FooterAction::Home:
      M5.Display.drawTriangle(
          centerX - 8, centerY,
          centerX, centerY - 7,
          centerX + 8, centerY,
          color);
      M5.Display.drawRect(centerX - 6, centerY, 12, 8, color);
      break;

    case FooterAction::Back:
      M5.Display.drawLine(centerX - 7, centerY, centerX + 7, centerY, color);
      M5.Display.drawLine(centerX - 7, centerY, centerX - 1, centerY - 6, color);
      M5.Display.drawLine(centerX - 7, centerY, centerX - 1, centerY + 6, color);
      break;

    case FooterAction::None:
      break;
  }
}

bool CompatUi::footerVisible() const {
  if (screen_ == Screen::TileDetail) {
    return true;
  }
  return homePageCount() > 1;
}

std::size_t CompatUi::homePageCount() const {
  if (!config_ || config_->tiles.empty()) return 1;
  return (config_->tiles.size() + kTilesPerPage - 1) / kTilesPerPage;
}

void CompatUi::handleTouch(const m5::touch_detail_t& touch) {
  if (screen_ == Screen::Home) {
    handleHomeTouch(touch.x, touch.y);
  } else {
    handleDetailTouch(touch.x, touch.y);
  }
}

void CompatUi::handleHomeTouch(int x, int y) {
  if (!config_) return;
  const int height = M5.Display.height();
  if (footerVisible() && y >= height - kFooterHeight) {
    handleFooterTouch(x);
  }
}

bool CompatUi::beginHomeTilePress(
    const m5::touch_detail_t& touch,
    std::uint32_t now) {
  std::size_t tileIndex = 0;
  if (!hitTestHomeTile(touch.x, touch.y, tileIndex)) return false;

  homeTilePressActive_ = true;
  homeTilePressTile_ = tileIndex;
  homeTilePressStartX_ = touch.x;
  homeTilePressStartY_ = touch.y;
  tilePressGesture_.reset();
  tilePressGesture_.step(
      core::PressPhase::Pressed,
      now,
      config_ ? config_->ui.longPressMs : kDefaultLongPressMs);
  return true;
}

void CompatUi::updateHomeTilePress(
    const m5::touch_detail_t& touch,
    std::uint32_t now) {
  if (!homeTilePressActive_) return;

  const auto phase = pressPhaseForTouch(touch);
  if (phase == core::PressPhase::None) return;

  if (touch.isPressed() && !tilePressGesture_.cancelled()) {
    const int dx = touch.x - homeTilePressStartX_;
    const int dy = touch.y - homeTilePressStartY_;
    if (dx > kTilePressMoveTolerance || dx < -kTilePressMoveTolerance ||
        dy > kTilePressMoveTolerance || dy < -kTilePressMoveTolerance) {
      Serial.println("[UI] tile gesture cancelled: finger moved outside tolerance");
      tilePressGesture_.cancel();
    }
  }

  const auto event = tilePressGesture_.step(
      phase,
      now,
      config_ ? config_->ui.longPressMs : kDefaultLongPressMs);

  dispatchHomeTileGesture(event);

  if (phase == core::PressPhase::Released || !tilePressGesture_.active()) {
    resetHomeTilePress();
  }
}

void CompatUi::dispatchHomeTileGesture(core::PressGesture event) {
  if (event == core::PressGesture::None || !config_ ||
      homeTilePressTile_ >= config_->tiles.size()) {
    return;
  }

  const auto gesture = event == core::PressGesture::ShortPress
      ? core::TileGesture::ShortPress
      : core::TileGesture::LongPress;
  const auto behavior = tileBehaviorForTile(config_->tiles[homeTilePressTile_]);
  const auto action = core::resolveTileAction(behavior, gesture);
  executeTileAction(homeTilePressTile_, gesture, action);
}

void CompatUi::resetHomeTilePress() {
  tilePressGesture_.reset();
  homeTilePressActive_ = false;
  homeTilePressTile_ = 0;
  homeTilePressStartX_ = 0;
  homeTilePressStartY_ = 0;
}

bool CompatUi::hitTestHomeTile(
    int x,
    int y,
    std::size_t& tileIndex) const {
  if (!config_) return false;

  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int footerHeight = footerVisible() ? kFooterHeight : 0;
  const int top = statusHeight();
  if (x < 0 || x >= width || y < top || y >= height - footerHeight) {
    return false;
  }

  const int contentHeight = height - top - footerHeight;
  const int tileW = width / kColumns;
  const int tileH = contentHeight / kRows;
  const int col = min(kColumns - 1, max(0, x / tileW));
  const int row = min(kRows - 1, max(0, (y - top) / tileH));
  const int slot = row * kColumns + col;

  tileIndex = homePage_ * kTilesPerPage + slot;
  return tileIndex < config_->tiles.size();
}

core::TileBehavior CompatUi::tileBehaviorForTile(
    const model::Tile& tile) const {
  if (tile.switchCount() > 0) {
    return {core::TileAction::Toggle, core::TileAction::OpenDetail};
  }

  return {core::TileAction::OpenDetail, core::TileAction::OpenDetail};
}

void CompatUi::executeTileAction(
    std::size_t tileIndex,
    core::TileGesture gesture,
    core::TileAction action) {
  if (!config_ || tileIndex >= config_->tiles.size()) return;
  auto& tile = config_->tiles[tileIndex];

  Serial.printf(
      "[UI] tile gesture: tile=%u type=%u gesture=%s action=%s\n",
      static_cast<unsigned>(tileIndex),
      static_cast<unsigned>(tile.type),
      tileGestureText(gesture),
      tileActionText(action));

  switch (action) {
    case core::TileAction::Toggle:
      if (mqtt_) mqtt_->switchTile(tile.id, !tile.allSwitchesOn());
      break;

    case core::TileAction::OpenDetail:
      selectedTile_ = tileIndex;
      detailPage_ = 0;
      screen_ = Screen::TileDetail;
      break;

    case core::TileAction::None:
    case core::TileAction::Activate:
    case core::TileAction::SetValue:
      break;
  }

  dirty_ = true;
}

void CompatUi::handleDetailTouch(int x, int y) {
  if (debugMode_) {
    handleDetailTouchDebug(x, y);
  } else {
    handleDetailTouchUser(x, y);
  }
}

void CompatUi::handleDetailTouchDebug(int x, int y) {
  if (!config_ || selectedTile_ >= config_->tiles.size()) return;
  auto& tile = config_->tiles[selectedTile_];
  const int height = M5.Display.height();

  if (y >= height - kFooterHeight) {
    handleFooterTouch(x);
    return;
  }

  if (!mqtt_) return;

  constexpr int firstY = kDebugStatusHeight + 28;
  constexpr int rowHeight = 42;
  if (y < firstY) return;
  const int row = (y - firstY) / rowHeight;
  if (row < 0 || row >= kDebugDetailItemsPerPage) return;

  const std::size_t idx = detailPage_ * kDebugDetailItemsPerPage + row;
  if (idx >= tile.items.size()) return;
  auto& item = tile.items[idx];
  if (item.type != model::TileItemType::Switch) return;

  auto& device = item.switchDevice;
  mqtt_->switchTileItem(tile.id, item.id, !device.active);
  dirty_ = true;
}

void CompatUi::handleDetailTouchUser(int x, int y) {
  if (!config_ || selectedTile_ >= config_->tiles.size()) return;
  auto& tile = config_->tiles[selectedTile_];
  const int width = M5.Display.width();
  const int height = M5.Display.height();

  if (y >= height - kFooterHeight) {
    handleFooterTouch(x);
    return;
  }
  if (y < kUserStatusHeight || !mqtt_) return;

  if (tile.items.size() == 1 && tile.type != model::TileType::Scene) {
    auto& item = tile.items.front();
    if (item.type == model::TileItemType::Switch) {
      mqtt_->switchTileItem(tile.id, item.id, !item.switchDevice.active);
      dirty_ = true;
    }
    return;
  }

  const int contentHeight = height - kUserStatusHeight - kFooterHeight;
  const int tileW = width / kColumns;
  const int tileH = contentHeight / kRows;
  const int col = min(kColumns - 1, max(0, x / tileW));
  const int row = min(kRows - 1, max(0, (y - kUserStatusHeight) / tileH));
  const int slot = row * kColumns + col;
  const std::size_t idx = detailPage_ * kUserDetailItemsPerPage + slot;
  if (idx >= tile.items.size()) return;

  auto& item = tile.items[idx];
  if (item.type != model::TileItemType::Switch) return;
  mqtt_->switchTileItem(tile.id, item.id, !item.switchDevice.active);
  dirty_ = true;
}

void CompatUi::handleFooterTouch(int x) {
  if (!footerVisible()) return;

  const auto slot = footerSlotForX(x);
  const auto action = footerActionForSlot(slot);
  if (!footerActionEnabled(action)) return;

  executeFooterAction(action);
}

CompatUi::FooterSlot CompatUi::footerSlotForX(int x) const {
  const int width = M5.Display.width();
  if (x < width / 3) return FooterSlot::Left;
  if (x < (width * 2) / 3) return FooterSlot::Center;
  return FooterSlot::Right;
}

CompatUi::FooterAction CompatUi::footerActionForSlot(FooterSlot slot) const {
  if (screen_ == Screen::Home) {
    switch (slot) {
      case FooterSlot::Left: return FooterAction::Previous;
      case FooterSlot::Center: return FooterAction::Home;
      case FooterSlot::Right: return FooterAction::Next;
    }
  }

  switch (slot) {
    case FooterSlot::Left: return FooterAction::Previous;
    case FooterSlot::Center: return FooterAction::Back;
    case FooterSlot::Right: return FooterAction::Next;
  }
  return FooterAction::None;
}

bool CompatUi::footerActionEnabled(FooterAction action) const {
  if (!config_) return false;

  switch (action) {
    case FooterAction::None:
      return false;

    case FooterAction::Home:
      return true;

    case FooterAction::Back:
      return screen_ == Screen::TileDetail;

    case FooterAction::Previous:
      return screen_ == Screen::Home ? homePage_ > 0 : detailPage_ > 0;

    case FooterAction::Next:
      if (screen_ == Screen::Home) {
        return homePage_ + 1 < homePageCount();
      }

      if (selectedTile_ >= config_->tiles.size()) return false;
      {
        const auto& tile = config_->tiles[selectedTile_];
        const std::size_t perPage = detailItemsPerPage();
        const std::size_t pages = tile.items.empty()
            ? 1
            : (tile.items.size() + perPage - 1) / perPage;
        return detailPage_ + 1 < pages;
      }
  }
  return false;
}

void CompatUi::executeFooterAction(FooterAction action) {
  switch (action) {
    case FooterAction::Previous:
      if (screen_ == Screen::Home) {
        if (homePage_ > 0) --homePage_;
      } else if (detailPage_ > 0) {
        --detailPage_;
      }
      break;

    case FooterAction::Home:
      screen_ = Screen::Home;
      homePage_ = 0;
      detailPage_ = 0;
      break;

    case FooterAction::Back:
      screen_ = Screen::Home;
      detailPage_ = 0;
      break;

    case FooterAction::Next:
      if (screen_ == Screen::Home) {
        ++homePage_;
      } else {
        ++detailPage_;
      }
      break;

    case FooterAction::None:
      return;
  }

  Serial.printf("[UI] footer action: %s\n", footerActionLabel(action));
  dirty_ = true;
}

const char* CompatUi::footerActionLabel(FooterAction action) {
  switch (action) {
    case FooterAction::Previous: return "< Prev";
    case FooterAction::Home: return "Home";
    case FooterAction::Back: return "Back";
    case FooterAction::Next: return "Next >";
    case FooterAction::None: return "";
  }
  return "";
}

void CompatUi::showMessage(const String& message) {
  message_ = message;
  dirty_ = true;
}

void CompatUi::clearMessage() {
  message_ = "";
  dirty_ = true;
}

String CompatUi::truncate(const String& value, std::size_t maxChars) {
  if (value.length() <= maxChars) return value;
  if (maxChars <= 3) return value.substring(0, maxChars);
  return value.substring(0, maxChars - 3) + "...";
}

}  // namespace homepoint::ui
