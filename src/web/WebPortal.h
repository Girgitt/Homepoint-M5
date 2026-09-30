#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../config/ConfigStore.h"
#include "../config/PersistentSettings.h"
#include "../network/MqttManager.h"
#include "../network/WifiManager.h"
#include "../../lib/core/DisplayCapture.h"

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
      core::DisplayCapture* displayCapture,
      std::function<void()> reloadCallback,
      std::function<void(bool)> debugUiChangedCallback);

  void tick();
  void updateRuntimeState(
      const model::AppConfig& config,
      std::uint32_t nowMs,
      bool force = false);

 private:
  AsyncWebServer server_{80};

  config::PersistentSettingsStore* persistentStore_ = nullptr;
  config::BootstrapSettings* bootstrap_ = nullptr;
  config::ConfigStore* configStore_ = nullptr;
  network::WifiManager* wifi_ = nullptr;
  network::MqttManager* mqtt_ = nullptr;
  core::DisplayCapture* displayCapture_ = nullptr;
  std::function<void()> reloadCallback_;
  std::function<void(bool)> debugUiChangedCallback_;

  bool restartPending_ = false;
  SemaphoreHandle_t runtimeStateMutex_ = nullptr;
  String runtimeStateJson_;
  String runtimeStateError_;
  std::uint32_t runtimeStateLastBuildMs_ = 0;
  bool runtimeStateBuilt_ = false;
  std::atomic<bool> runtimeStateDemandSeen_{false};
  std::atomic<std::uint32_t> runtimeStateLastRequestMs_{0};
  std::atomic<bool> runtimeStatePublished_{false};
  std::atomic<std::uint32_t> runtimeStatePublishedAtMs_{0};
  std::uint32_t restartAt_ = 0;

  bool authenticate(AsyncWebServerRequest* request) const;
  void requestRestart(std::uint32_t delayMs = 700);
  void installRoutes();
  void installCaptiveRoutes();
  void handleDashboardBody(
      AsyncWebServerRequest* request,
      std::uint8_t* data,
      std::size_t len,
      std::size_t index,
      std::size_t total,
      bool persist);
  void handleLayoutBody(
      AsyncWebServerRequest* request,
      std::uint8_t* data,
      std::size_t len,
      std::size_t index,
      std::size_t total,
      bool persist);
  void handleDashboardMutationBody(
      AsyncWebServerRequest* request,
      std::uint8_t* data,
      std::size_t len,
      std::size_t index,
      std::size_t total,
      bool upgrade);

  static String normalizeUploadPath(const String& filename);
  static String normalizePath(const String& path);
  static String contentTypeForPath(const String& path);
  static bool queryFlag(AsyncWebServerRequest* request, const char* name);
};

}  // namespace homepoint::web
