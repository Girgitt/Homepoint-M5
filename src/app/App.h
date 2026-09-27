#pragma once

#include <Arduino.h>
#include <atomic>

#include "../config/ConfigStore.h"
#include "../config/PersistentSettings.h"
#include "../model/Model.h"
#include "../network/MqttManager.h"
#include "../network/WifiManager.h"
#include "../system/EspDeadlineScheduler.h"
#include "../system/M5DisplayCapture.h"
#include "../ui/CompatUi.h"
#include "../ui/UiBackend.h"
#include "../ui/UiController.h"
#include "../web/WebPortal.h"

namespace homepoint::app {

class App {
 public:
  void setup();
  void loop();

 private:
  config::PersistentSettingsStore persistentStore_;
  config::BootstrapSettings bootstrap_;
  config::ConfigStore configStore_;
  model::AppConfig config_;

  network::WifiManager wifi_;
  network::MqttManager mqtt_;
  web::WebPortal web_;
  homepoint::system::EspDeadlineScheduler timing_;
  homepoint::system::M5DisplayCapture displayCapture_;
  ui::UiController uiController_;
  ui::CompatUi m5gfxUi_;
  ui::UiBackend* uiBackend_ = &m5gfxUi_;

  bool configValid_ = false;
  bool timeConfigured_ = false;
  bool previousWifiConnected_ = false;
  std::atomic<bool> wifiStatusPending_{false};
  std::atomic<bool> mqttStatusPending_{false};
  std::atomic<int> pendingDebugUi_{-1};
  String configMessage_;

  void reloadConfiguration();
  void resolveConfiguredHostname();
  void applyHardwareConfig();
  void maybeConfigureTime();
  void processPendingUiStatusChanges();
  void processPendingUiControls();
  void setDebugUi(bool enabled);
  void showUiMessage(const String& message);
  void clearUiMessage();
  void executeUiCommand(const ui::UiCommand& command);
};

}  // namespace homepoint::app
