#include "MqttManager.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <utility>

namespace homepoint::network {
namespace {

JsonVariantConst findKey(JsonVariantConst value, const String& key) {
  if (value.is<JsonObjectConst>()) {
    JsonObjectConst object = value.as<JsonObjectConst>();
    if (object.containsKey(key.c_str())) return object[key.c_str()];

    for (JsonPairConst pair : object) {
      const auto nested = findKey(pair.value(), key);
      if (!nested.isNull()) return nested;
    }
  } else if (value.is<JsonArrayConst>()) {
    for (JsonVariantConst item : value.as<JsonArrayConst>()) {
      const auto nested = findKey(item, key);
      if (!nested.isNull()) return nested;
    }
  }
  return JsonVariantConst();
}

String scalarToString(JsonVariantConst value) {
  if (value.isNull()) return "";
  if (value.is<const char*>()) return String(value.as<const char*>());
  String out;
  serializeJson(value, out);
  return out;
}

}  // namespace

void MqttManager::begin(
    model::AppConfig* config, std::function<void()> modelChanged) {
  modelChanged_ = std::move(modelChanged);
  reconfigure(config);
}

void MqttManager::reconfigure(model::AppConfig* config) {
  if (client_.connected()) client_.disconnect();

  config_ = config;
  host_ = "";
  port_ = 1883;
  reconnectDelayMs_ = 3000;
  nextReconnectAt_ = 0;

  if (!config_ || config_->mqtt.uri.isEmpty()) {
    state_ = MqttState::Disabled;
    Serial.println("[MQTT] disabled: mqttbroker is not configured");
    return;
  }

  if (!parseUri(config_->mqtt.uri)) {
    state_ = MqttState::Error;
    Serial.printf("[MQTT] invalid broker URI: '%s'\n", config_->mqtt.uri.c_str());
    return;
  }

  Serial.printf("[MQTT] configured broker: %s:%u\n", host_.c_str(), port_);
  configureClient();
  state_ = WiFi.status() == WL_CONNECTED
               ? MqttState::Connecting
               : MqttState::WaitingForWifi;
  nextReconnectAt_ = millis();
}

bool MqttManager::parseUri(const String& uri) {
  String work = uri;
  if (work.startsWith("mqtt://")) {
    work.remove(0, 7);
  } else if (work.startsWith("tcp://")) {
    work.remove(0, 6);
  } else if (work.indexOf("://") >= 0) {
    return false;  // TLS will be a later explicit configuration feature.
  }

  const int slash = work.indexOf('/');
  if (slash >= 0) work = work.substring(0, slash);

  const int colon = work.lastIndexOf(':');
  if (colon > 0) {
    host_ = work.substring(0, colon);
    const long parsedPort = work.substring(colon + 1).toInt();
    if (parsedPort <= 0 || parsedPort > 65535) return false;
    port_ = static_cast<std::uint16_t>(parsedPort);
  } else {
    host_ = work;
    port_ = 1883;
  }
  return !host_.isEmpty();
}

void MqttManager::configureClient() {
  client_.begin(host_.c_str(), port_, net_);
  client_.onMessage([this](String& topic, String& payload) {
    handleMessage(topic, payload);
  });
}

void MqttManager::tick() {
  if (!config_ || config_->mqtt.uri.isEmpty()) {
    state_ = MqttState::Disabled;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (client_.connected()) client_.disconnect();
    state_ = MqttState::WaitingForWifi;
    return;
  }

  if (client_.connected()) {
    client_.loop();
    state_ = MqttState::Connected;
    return;
  }

  const auto now = millis();
  if (static_cast<std::int32_t>(now - nextReconnectAt_) < 0) return;

  state_ = MqttState::Connecting;
  if (connect()) {
    state_ = MqttState::Connected;
    reconnectDelayMs_ = 3000;
    Serial.printf("[MQTT] ONLINE: broker=%s:%u\n", host_.c_str(), port_);
    subscribeAll();
    if (modelChanged_) modelChanged_();
  } else {
    state_ = MqttState::Error;
    Serial.printf(
        "[MQTT] connection failed: error=%d returnCode=%d retry-in=%u ms\n",
        static_cast<int>(client_.lastError()),
        static_cast<int>(client_.returnCode()),
        static_cast<unsigned>(reconnectDelayMs_));
    nextReconnectAt_ = now + reconnectDelayMs_;
    reconnectDelayMs_ = reconnectDelayMs_ >= 30000u ? 60000u : reconnectDelayMs_ * 2u;
  }
}

bool MqttManager::connect() {
  if (!config_) return false;

  const String id = clientId();
  bool ok = false;
  if (config_->mqtt.username.isEmpty()) {
    ok = client_.connect(id.c_str());
  } else {
    ok = client_.connect(
        id.c_str(),
        config_->mqtt.username.c_str(),
        config_->mqtt.password.c_str());
  }

  if (!ok) nextReconnectAt_ = millis() + reconnectDelayMs_;
  return ok;
}

void MqttManager::subscribeAll() {
  if (!client_.connected() || !config_) return;

  for (const auto& tile : config_->tiles) {
    for (const auto& item : tile.items) {
      if (item.type == model::TileItemType::Switch) {
        if (!item.switchDevice.getTopic.isEmpty()) {
          client_.subscribe(item.switchDevice.getTopic);
        }
      } else if (!item.sensorDevice.getTopic.isEmpty()) {
        client_.subscribe(item.sensorDevice.getTopic);
      }
    }
  }
}

void MqttManager::handleMessage(String& topic, String& payload) {
  if (!config_) return;
  bool changed = false;

  for (auto& tile : config_->tiles) {
    for (auto& item : tile.items) {
      if (item.type == model::TileItemType::Switch) {
        auto& device = item.switchDevice;
        if (device.getTopic != topic) continue;
        const bool before = device.active;
        if (payload == device.onValue) device.active = true;
        if (payload == device.offValue) device.active = false;
        changed = changed || before != device.active;
        continue;
      }

      auto& device = item.sensorDevice;
      if (device.getTopic != topic) continue;

      if (device.jsonData) {
        const String first = jsonValueByKeyAnywhere(payload, device.firstKey);
        const String second = device.type == model::SensorType::CombinedValues
                                  ? jsonValueByKeyAnywhere(payload, device.secondKey)
                                  : "";
        if (!first.isEmpty() && first != device.firstValue) {
          device.firstValue = first;
          changed = true;
        }
        if (!second.isEmpty() && second != device.secondValue) {
          device.secondValue = second;
          changed = true;
        }
      } else if (payload != device.firstValue) {
        device.firstValue = payload;
        changed = true;
      }
    }
  }

  if (changed && modelChanged_) modelChanged_();
}

bool MqttManager::switchTile(std::uint16_t tileId, bool on) {
  if (!config_ || !client_.connected()) return false;
  if (tileId >= config_->tiles.size()) return false;

  auto& tile = config_->tiles[tileId];
  bool sawSwitch = false;
  bool ok = true;
  for (auto& item : tile.items) {
    if (item.type != model::TileItemType::Switch) continue;
    sawSwitch = true;
    auto& device = item.switchDevice;
    const String& value = on ? device.onValue : device.offValue;
    const bool published = client_.publish(device.setTopic, value);
    ok = published && ok;
    if (published) device.active = on;
  }

  if (sawSwitch && modelChanged_) modelChanged_();
  return sawSwitch && ok;
}

bool MqttManager::switchTileItem(
    std::uint16_t tileId, std::uint16_t itemId, bool on) {
  if (!config_ || !client_.connected()) return false;
  if (tileId >= config_->tiles.size()) return false;

  auto& tile = config_->tiles[tileId];
  for (auto& item : tile.items) {
    if (item.id != itemId || item.type != model::TileItemType::Switch) continue;
    auto& device = item.switchDevice;
    const String& value = on ? device.onValue : device.offValue;
    const bool ok = client_.publish(device.setTopic, value);
    if (ok) {
      device.active = on;
      if (modelChanged_) modelChanged_();
    }
    return ok;
  }
  return false;
}

String MqttManager::statusText() const {
  switch (state_) {
    case MqttState::Disabled: return "NOT CONFIG";
    case MqttState::WaitingForWifi: return "WAIT WIFI";
    case MqttState::Connecting: return "CONNECTING";
    case MqttState::Connected: return "ONLINE";
    case MqttState::Error: return "OFFLINE";
  }
  return "UNKNOWN";
}

String MqttManager::clientId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  return String("homepoint-m5-") + mac;
}

String MqttManager::jsonValueByKeyAnywhere(
    const String& payload, const String& key) {
  if (key.isEmpty()) return "";

  JsonDocument document;
  if (deserializeJson(document, payload)) return "";

  return scalarToString(findKey(document.as<JsonVariantConst>(), key));
}

}  // namespace homepoint::network
