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

  const bool persistentReady = persistentStore_.begin();
  auto loadResult = config::PersistentLoadResult::IoError;
  if (persistentReady) {
    loadResult = persistentStore_.load(bootstrap_);
  }

  Serial.printf(
      "[BOOT] bootstrap storage: begin=%s load=%s configured=%s ssid='%s' hostname='%s'\n",
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

  applyHardwareConfig();

  wifi_.begin(bootstrap_, [this]() {
    wifiStatusPending_.store(true, std::memory_order_relaxed);
  });

  mqtt_.begin(
      &config_,
      [this](const model::ModelChange& change) {
        ui_.applyChange(core::UiChange::modelItem(
            change.tileIndex, change.itemIndex));
      },
      [this]() {
        mqttStatusPending_.store(true, std::memory_order_relaxed);
      });

  ui_.begin(&config_, &wifi_, &mqtt_, bootstrap_.debugUi);
  if (!bootstrap_.configured) {
    ui_.showMessage("Setup AP: HomePoint-Config\nOpen 192.168.99.1");
  } else if (!configValid_) {
    ui_.showMessage(configMessage_);
  }

  web_.begin(
      &persistentStore_,
      &bootstrap_,
      &configStore_,
      &wifi_,
      &mqtt_,
      [this]() { reloadConfiguration(); },
      [this](bool enabled) { setDebugUi(enabled); });
}

void App::loop() {
  M5.update();

  wifi_.tick();
  mqtt_.tick();
  web_.tick();
  maybeConfigureTime();
  processPendingUiStatusChanges();
  ui_.tick();

  delay(5);
}

void App::processPendingUiStatusChanges() {
  if (wifiStatusPending_.exchange(false, std::memory_order_relaxed)) {
    ui_.applyChange(core::UiChange::wifiStatus());
  }
  if (mqttStatusPending_.exchange(false, std::memory_order_relaxed)) {
    ui_.applyChange(core::UiChange::mqttStatus());
  }
}

void App::reloadConfiguration() {
  model::AppConfig candidate;
  String error;
  if (!configStore_.load(candidate, error)) {
    configValid_ = false;
    configMessage_ = String("Reload failed: ") + error;
    ui_.showMessage(configMessage_);
    return;
  }

  config_ = std::move(candidate);
  configValid_ = true;
  configMessage_ = error;

  applyHardwareConfig();
  mqtt_.reconfigure(&config_);
  ui_.setConfig(&config_);
  ui_.clearMessage();
  timeConfigured_ = false;
}

void App::setDebugUi(bool enabled) {
  bootstrap_.debugUi = enabled;
  ui_.setDebugMode(enabled);
  Serial.printf("[UI] display mode changed: %s\n", enabled ? "debug" : "user");
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

  setenv("TZ", config_.timezone.c_str(), 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  timeConfigured_ = true;
}

}  // namespace homepoint::app
