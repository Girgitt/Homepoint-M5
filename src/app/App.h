#pragma once

#include <Arduino.h>

#include "../config/ConfigStore.h"
#include "../config/PersistentSettings.h"
#include "../model/Model.h"
#include "../network/MqttManager.h"
#include "../network/WifiManager.h"
#include "../ui/CompatUi.h"
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
  ui::CompatUi ui_;

  bool configValid_ = false;
  bool timeConfigured_ = false;
  bool previousWifiConnected_ = false;
  String configMessage_;

  void reloadConfiguration();
  void applyHardwareConfig();
  void maybeConfigureTime();
  void setDebugUi(bool enabled);
};

}  // namespace homepoint::app
