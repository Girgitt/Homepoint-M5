#pragma once

#include <Arduino.h>
#include <MQTT.h>
#include <WiFiClient.h>
#include <functional>

#include "../model/Model.h"

namespace homepoint::network {

enum class MqttState {
  Disabled,
  WaitingForWifi,
  Connecting,
  Connected,
  Error,
};

class MqttManager {
 public:
  using ModelChangedCallback = std::function<void(const model::ModelChange&)>;
  using StatusChangedCallback = std::function<void()>;

  void begin(
      model::AppConfig* config,
      ModelChangedCallback modelChanged,
      StatusChangedCallback statusChanged = {});
  void reconfigure(model::AppConfig* config);
  void tick();

  MqttState state() const { return state_; }
  String statusText() const;

  bool switchTile(std::uint16_t tileId, bool on);
  bool switchTileItem(std::uint16_t tileId, std::uint16_t itemId, bool on);

 private:
  model::AppConfig* config_ = nullptr;
  ModelChangedCallback modelChanged_;
  StatusChangedCallback statusChanged_;

  WiFiClient net_;
  MQTTClient client_{2048};

  MqttState state_ = MqttState::Disabled;
  String host_;
  std::uint16_t port_ = 1883;

  std::uint32_t nextReconnectAt_ = 0;
  std::uint32_t reconnectDelayMs_ = 3000;

  void configureClient();
  bool parseUri(const String& uri);
  bool connect();
  void subscribeAll();
  void handleMessage(String& topic, String& payload);
  void setState(MqttState state);

  static String clientId();
  static String jsonValueByKeyAnywhere(const String& payload, const String& key);
};

}  // namespace homepoint::network
