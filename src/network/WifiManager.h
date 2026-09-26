#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <functional>

#include "../config/PersistentSettings.h"

namespace homepoint::network {

enum class WifiState {
  Provisioning,
  Connecting,
  Connected,
  Disconnected,
};

class WifiManager {
 public:
  using StatusChangedCallback = std::function<void()>;

  void begin(
      const config::BootstrapSettings& settings,
      StatusChangedCallback statusChanged = {});
  void tick();

  WifiState state() const { return state_; }
  bool stationConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool provisioningApActive() const { return provisioningApActive_; }
  bool recoveryApActive() const { return recoveryApActive_; }

  String ipAddress() const;
  String statusText() const;
  String scanNetworksJson();

  static constexpr const char* kProvisioningSsid = "HomePoint-Config";
  static constexpr const char* kRecoverySsid = "HomePoint-M5-Recovery";

 private:
  config::BootstrapSettings settings_;
  WifiState state_ = WifiState::Disconnected;
  DNSServer dns_;

  bool provisioningApActive_ = false;
  bool recoveryApActive_ = false;

  std::uint32_t connectStartedAt_ = 0;
  std::uint32_t nextReconnectAt_ = 0;
  std::uint32_t reconnectDelayMs_ = 2000;
  StatusChangedCallback statusChanged_;

  void startStation();
  void startAp(const char* ssid, bool provisioning);
  void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
  void setState(WifiState state, bool forceNotify = false);
  void notifyStatusChanged();
};

}  // namespace homepoint::network
