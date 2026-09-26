#include "WifiManager.h"

#include <ArduinoJson.h>

namespace homepoint::network {
namespace {

IPAddress kApIp(192, 168, 99, 1);
IPAddress kApMask(255, 255, 255, 0);

}  // namespace

void WifiManager::begin(const config::BootstrapSettings& settings) {
  settings_ = settings;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    onWifiEvent(event, info);
  });

  if (!settings_.configured || settings_.wifiSsid.isEmpty()) {
    Serial.println("[WIFI] bootstrap Wi-Fi not configured; starting provisioning AP");
    startAp(kProvisioningSsid, true);
    state_ = WifiState::Provisioning;
    return;
  }

  startStation();
}

void WifiManager::startStation() {
  Serial.printf(
      "[WIFI] starting station: ssid='%s' hostname='%s'\n",
      settings_.wifiSsid.c_str(),
      settings_.hostname.c_str());
  WiFi.mode(WIFI_STA);
  if (!settings_.hostname.isEmpty()) {
    WiFi.setHostname(settings_.hostname.c_str());
  }
  WiFi.begin(settings_.wifiSsid.c_str(), settings_.wifiPassword.c_str());

  state_ = WifiState::Connecting;
  connectStartedAt_ = millis();
  nextReconnectAt_ = millis() + reconnectDelayMs_;
}

void WifiManager::startAp(const char* ssid, bool provisioning) {
  // Keep STA enabled while the AP is active so provisioning can scan
  // nearby networks.  This also lets the recovery AP coexist with station
  // reconnect attempts.
  WiFi.mode(WIFI_AP_STA);

  WiFi.softAPConfig(kApIp, kApIp, kApMask);
  WiFi.softAP(ssid);
  dns_.start(53, "*", kApIp);

  Serial.printf(
      "[WIFI] AP active: ssid='%s' ip=%s mode=%s\n",
      ssid,
      WiFi.softAPIP().toString().c_str(),
      provisioning ? "provisioning" : "recovery");

  provisioningApActive_ = provisioning;
  recoveryApActive_ = !provisioning;
}

void WifiManager::onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      state_ = WifiState::Connected;
      Serial.printf(
          "[WIFI] station ONLINE: ip=%s gateway=%s rssi=%d dBm\n",
          WiFi.localIP().toString().c_str(),
          WiFi.gatewayIP().toString().c_str(),
          WiFi.RSSI());
      reconnectDelayMs_ = 2000;
      nextReconnectAt_ = 0;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WIFI] station disconnected");
      if (settings_.configured) {
        state_ = WifiState::Disconnected;
        nextReconnectAt_ = millis() + reconnectDelayMs_;
        reconnectDelayMs_ = reconnectDelayMs_ >= 30000u ? 60000u : reconnectDelayMs_ * 2u;
      }
      break;

    default:
      break;
  }
}

void WifiManager::tick() {
  if (provisioningApActive_ || recoveryApActive_) {
    dns_.processNextRequest();
  }

  if (!settings_.configured) return;
  if (stationConnected()) return;

  const auto now = millis();

  // Recovery must not depend on the configured AP remaining reachable.
  if (!recoveryApActive_ && now - connectStartedAt_ > 60000u) {
    Serial.println("[WIFI] station unavailable for 60s; enabling recovery AP");
    startAp(kRecoverySsid, false);
  }

  if (nextReconnectAt_ && static_cast<std::int32_t>(now - nextReconnectAt_) >= 0) {
    WiFi.reconnect();
    state_ = WifiState::Connecting;
    nextReconnectAt_ = now + reconnectDelayMs_;
  }
}

String WifiManager::ipAddress() const {
  if (stationConnected()) return WiFi.localIP().toString();
  if (provisioningApActive_ || recoveryApActive_) return WiFi.softAPIP().toString();
  return "0.0.0.0";
}

String WifiManager::statusText() const {
  if (recoveryApActive_ && !stationConnected()) return "RECOVERY AP";
  switch (state_) {
    case WifiState::Provisioning: return "CONFIG AP";
    case WifiState::Connecting: return "CONNECTING";
    case WifiState::Connected: return "ONLINE";
    case WifiState::Disconnected: return "OFFLINE";
  }
  return "UNKNOWN";
}

String WifiManager::scanNetworksJson() {
  JsonDocument document;
  JsonArray networks = document["networks"].to<JsonArray>();

  const int count = WiFi.scanNetworks(false, true);
  for (int i = 0; i < count; ++i) {
    JsonObject network = networks.add<JsonObject>();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();

  String out;
  serializeJson(document, out);
  return out;
}

}  // namespace homepoint::network
