#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>

#include "../config/ConfigStore.h"
#include "../config/PersistentSettings.h"
#include "../network/MqttManager.h"
#include "../network/WifiManager.h"

namespace homepoint::web {

class WebPortal {
 public:
  WebPortal();

  void begin(
      config::PersistentSettingsStore* persistentStore,
      config::BootstrapSettings* bootstrap,
      config::ConfigStore* configStore,
      network::WifiManager* wifi,
      network::MqttManager* mqtt,
      std::function<void()> reloadCallback,
      std::function<void(bool)> debugUiChangedCallback);

  void tick();

 private:
  AsyncWebServer server_{80};

  config::PersistentSettingsStore* persistentStore_ = nullptr;
  config::BootstrapSettings* bootstrap_ = nullptr;
  config::ConfigStore* configStore_ = nullptr;
  network::WifiManager* wifi_ = nullptr;
  network::MqttManager* mqtt_ = nullptr;
  std::function<void()> reloadCallback_;
  std::function<void(bool)> debugUiChangedCallback_;

  bool restartPending_ = false;
  std::uint32_t restartAt_ = 0;

  bool authenticate(AsyncWebServerRequest* request) const;
  void requestRestart(std::uint32_t delayMs = 700);
  void installRoutes();
  void installCaptiveRoutes();

  static String normalizeUploadPath(const String& filename);
};

}  // namespace homepoint::web
