#include "App.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <utility>

namespace homepoint::app {
namespace {

const char* persistentLoadResultText(config::PersistentLoadResult result) {
  switch (result) {
    case config::PersistentLoadResult::Ok: return "ok";
    case config::PersistentLoadResult::Uninitialized: return "uninitialized";
    case config::PersistentLoadResult::Corrupt: return "corrupt";
    case config::PersistentLoadResult::UnsupportedVersion: return "unsupported-version";
    case config::PersistentLoadResult::IoError: return "io-error";
  }
  return "unknown";
}

}  // namespace

void App::setup() {
  Serial.begin(115200);
  delay(50);

  auto m5cfg = M5.config();
  M5.begin(m5cfg);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(8, 8);
  M5.Display.println("Homepoint-M5 bootstrap");

  const bool timingReady = timing_.begin();
  if (!timingReady) {
    Serial.println("[BOOT] timing service unavailable; using UI polling fallbacks");
  }

  const bool displayCaptureReady = displayCapture_.begin();
  if (!displayCaptureReady) {
    Serial.println("[BOOT] display screenshot service unavailable");
  }

  const bool persistentReady = persistentStore_.begin();
  auto loadResult = config::PersistentLoadResult::IoError;
  if (persistentReady) {
    loadResult = persistentStore_.load(bootstrap_);
  }

  Serial.printf(
      "[BOOT] bootstrap storage: begin=%s load=%s configured=%s ssid='%s' legacy-hostname='%s'\n",
      persistentReady ? "ok" : "failed",
      persistentLoadResultText(loadResult),
      bootstrap_.configured ? "yes" : "no",
      bootstrap_.wifiSsid.c_str(),
      bootstrap_.hostname.c_str());
  Serial.printf("[BOOT] display mode: %s\n", bootstrap_.debugUi ? "debug" : "user");

  // Fresh devices may not have a LittleFS filesystem yet. Only auto-format
  // when bootstrap storage is not already configured, so a later filesystem
  // failure cannot silently erase a deployed configuration.
  const bool allowInitialFsFormat =
      loadResult != config::PersistentLoadResult::Ok || !bootstrap_.configured;
  Serial.printf(
      "[BOOT] filesystem automatic format: %s\n",
      allowInitialFsFormat ? "allowed" : "disabled");
  const bool fsReady = configStore_.begin(allowInitialFsFormat);
  Serial.printf(
      "[BOOT] filesystem result: %s - %s\n",
      fsReady ? "ready" : "unavailable",
      configStore_.statusText().c_str());

  if (loadResult != config::PersistentLoadResult::Ok && configStore_.mounted()) {
    config::BootstrapSettings imported;
    if (configStore_.importLegacyBootstrap(imported)) {
      bootstrap_ = imported;
      if (persistentReady && persistentStore_.save(bootstrap_)) {
        loadResult = config::PersistentLoadResult::Ok;
        Serial.println("Imported legacy Wi-Fi/web bootstrap settings from config.json");
      }
    }
  }

  if (configStore_.mounted()) {
    configStore_.ensureDefaultConfig();
  }

  String error;
  configValid_ = configStore_.load(config_, error);
  configMessage_ = configValid_ ? error : String("Config error: ") + error;
  Serial.printf(
      "[CONFIG] load: %s%s%s\n",
      configValid_ ? "ok" : "failed",
      error.isEmpty() ? "" : " - ",
      error.c_str());

  resolveConfiguredHostname();
  applyHardwareConfig();

  wifi_.begin(bootstrap_, config_.hostname, [this]() {
    wifiStatusPending_.store(true, std::memory_order_relaxed);
  });

  mqtt_.begin(
      &config_,
      [this](const model::ModelChange& change) {
        uiBackend_->applyChange(core::UiChange::modelItem(
            change.tileIndex, change.itemIndex));
      },
      [this]() {
        mqttStatusPending_.store(true, std::memory_order_relaxed);
      });

  uiController_.begin(
      &config_,
      bootstrap_.debugUi,
      m5gfxUi_.layoutMetrics(bootstrap_.debugUi));
  m5gfxUi_.begin(
      &config_,
      &wifi_,
      &mqtt_,
      &uiController_,
      [this](const ui::UiCommand& command) { executeUiCommand(command); },
      timingReady ? &timing_ : nullptr);
  if (!bootstrap_.configured) {
    showUiMessage("Setup AP: HomePoint-Config\nOpen 192.168.99.1");
  } else if (!configValid_) {
    showUiMessage(configMessage_);
  }

  web_.begin(
      &persistentStore_,
      &bootstrap_,
      &configStore_,
      &wifi_,
      &mqtt_,
      displayCaptureReady ? &displayCapture_ : nullptr,
      [this]() { queueConfigurationReload(); },
      [this](bool enabled) { setDebugUi(enabled); });
  web_.updateRuntimeState(config_, millis(), true);
}

void App::loop() {
  M5.update();

  wifi_.tick();
  mqtt_.tick();
  web_.tick();
  processPendingConfigurationReload();
  web_.updateRuntimeState(config_, millis());
  maybeConfigureTime();
  processPendingUiStatusChanges();
  processPendingUiControls();
  uiBackend_->tick();
  displayCapture_.service();

  delay(5);
}

void App::queueConfigurationReload() {
  // WebPortal callbacks execute on the AsyncWebServer task. Defer config,
  // MQTT and UI mutations to the main application task so the renderer stays
  // single-threaded (and remains safe for the planned LVGL backend).
  configReloadPending_.store(true, std::memory_order_release);
  Serial.println("[CONFIG] reload queued");
}

void App::processPendingConfigurationReload() {
  if (!configReloadPending_.exchange(false, std::memory_order_acq_rel)) return;
  reloadConfiguration();
}

void App::processPendingUiStatusChanges() {
  if (wifiStatusPending_.exchange(false, std::memory_order_relaxed)) {
    uiBackend_->applyChange(core::UiChange::wifiStatus());
  }
  if (mqttStatusPending_.exchange(false, std::memory_order_relaxed)) {
    uiBackend_->applyChange(core::UiChange::mqttStatus());
  }
}

void App::reloadConfiguration() {
  model::AppConfig candidate;
  String error;
  if (!configStore_.load(candidate, error)) {
    configValid_ = false;
    configMessage_ = String("Reload failed: ") + error;
    showUiMessage(configMessage_);
    return;
  }

  config_ = std::move(candidate);
  configValid_ = true;
  configMessage_ = error;

  resolveConfiguredHostname();
  if (config_.hostname != wifi_.requestedHostname()) {
    Serial.printf(
        "[CONFIG] hostname changed from '%s' to '%s'; reboot required to update DHCP identity\n",
        wifi_.requestedHostname().c_str(),
        config_.hostname.c_str());
  }

  applyHardwareConfig();
  mqtt_.reconfigure(&config_);
  uiController_.setConfig(
      &config_,
      uiBackend_->layoutMetrics(uiController_.debugMode()));
  uiBackend_->setConfig(&config_);
  web_.updateRuntimeState(config_, millis(), true);
  clearUiMessage();
  timeConfigured_ = false;
}

void App::resolveConfiguredHostname() {
  if (!config_.hostname.isEmpty()) return;

  String fallback = bootstrap_.hostname;
  if (fallback.isEmpty()) fallback = "homepoint-m5";
  config_.hostname = fallback;

  if (!configValid_ || !configStore_.mounted()) {
    Serial.printf(
        "[CONFIG] hostname missing; using fallback '%s' for this boot (not persisted)\n",
        fallback.c_str());
    return;
  }

  String error;
  if (configStore_.setHostname(fallback, error)) {
    Serial.printf(
        "[CONFIG] migrated hostname '%s' into config.json; EEPROM hostname is now legacy-only\n",
        fallback.c_str());
  } else {
    Serial.printf(
        "[CONFIG] hostname migration not persisted: %s; using '%s' for this boot\n",
        error.c_str(),
        fallback.c_str());
  }
}

void App::setDebugUi(bool enabled) {
  // WebPortal runs on the AsyncWebServer task. Keep UI mutations on the main
  // task so display state remains single-threaded now and LVGL-safe later.
  pendingDebugUi_.store(enabled ? 1 : 0, std::memory_order_release);
  Serial.printf("[UI] display mode change queued: %s\n", enabled ? "debug" : "user");
}

void App::processPendingUiControls() {
  const int pending = pendingDebugUi_.exchange(-1, std::memory_order_acq_rel);
  if (pending < 0) return;

  const bool enabled = pending != 0;
  if (uiController_.debugMode() == enabled) return;
  uiController_.setDebugMode(enabled, uiBackend_->layoutMetrics(enabled));
  uiBackend_->onDisplayModeChanged();
  Serial.printf("[UI] display mode changed: %s\n", enabled ? "debug" : "user");
}

void App::showUiMessage(const String& message) {
  uiController_.setMessage(message);
  uiBackend_->applyChange(core::UiChange::message());
}

void App::clearUiMessage() {
  uiController_.clearMessage();
  uiBackend_->applyChange(core::UiChange::message());
}

void App::executeUiCommand(const ui::UiCommand& command) {
  if (command.kind == ui::UiCommandKind::None ||
      command.tileIndex >= config_.tiles.size()) {
    return;
  }

  const auto& tile = config_.tiles[command.tileIndex];
  switch (command.kind) {
    case ui::UiCommandKind::SetTileSwitchState:
      mqtt_.switchTile(tile.id, command.targetOn);
      return;

    case ui::UiCommandKind::SetTileItemSwitchState:
      if (command.itemIndex >= tile.items.size()) return;
      mqtt_.switchTileItem(
          tile.id,
          tile.items[command.itemIndex].id,
          command.targetOn);
      return;

    case ui::UiCommandKind::None:
      return;
  }
}

void App::applyHardwareConfig() {
  M5.Display.setRotation(config_.hardware.screenRotationAngle);
  M5.Display.invertDisplay(config_.hardware.displayColorInverted);

  // Old Core2 code switched AXP192 M-Bus mode directly. M5Unified exposes the
  // same intent as external-port direction and handles AXP192/AXP2101 variants.
  // external 5V -> input (false); USB/battery -> output (true)
  M5.Power.setExtOutput(!config_.hardware.powerFrom5vRailNotUsb);
}

void App::maybeConfigureTime() {
  const bool wifiConnected = wifi_.stationConnected();
  if (!wifiConnected) {
    previousWifiConnected_ = false;
    return;
  }

  if (!previousWifiConnected_) {
    previousWifiConnected_ = true;
    timeConfigured_ = false;
  }

  if (timeConfigured_ || config_.timezone.isEmpty()) return;

  Serial.printf("[TIME] configuring SNTP timezone='%s'\n", config_.timezone.c_str());
  configTzTime(config_.timezone.c_str(), "pool.ntp.org", "time.nist.gov");
  timeConfigured_ = true;
}

}  // namespace homepoint::app
