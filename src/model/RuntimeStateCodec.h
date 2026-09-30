#pragma once

#include <Arduino.h>

#include <cstdint>

#include "Model.h"

namespace homepoint::model {

class RuntimeStateCodec {
 public:
  static constexpr int kSchemaVersion = 1;

  static bool serialize(
      const AppConfig& config,
      const String& wifiStatus,
      bool wifiConnected,
      const String& mqttStatus,
      bool mqttConnected,
      std::uint32_t capturedAtMs,
      String& output,
      String& error);
};

}  // namespace homepoint::model
