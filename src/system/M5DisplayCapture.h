#pragma once

#include <Arduino.h>
#include <atomic>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../lib/core/DisplayCapture.h"

namespace homepoint::system {

class M5DisplayCapture final : public core::DisplayCapture {
 public:
  bool begin();
  void service();

  core::DisplayCaptureResult requestSnapshot(std::uint32_t& requestId) override;
  core::DisplayCaptureResult readSnapshot(
      std::uint32_t requestId,
      core::DisplaySnapshot& snapshot) override;

 private:
  SemaphoreHandle_t resultMutex_ = nullptr;
  std::atomic<std::uint32_t> activeId_{0};
  std::atomic<std::uint32_t> requestedId_{0};
  std::atomic<std::uint32_t> nextRequestId_{1};

  std::uint32_t lastAttemptId_ = 0;
  bool lastAttemptOk_ = false;
  std::uint32_t successfulId_ = 0;
  core::DisplaySnapshot completedSnapshot_;
  bool ready_ = false;

  core::DisplaySnapshot captureBmp() const;
  static void writeLe16(std::uint8_t* out, std::uint16_t value);
  static void writeLe32(std::uint8_t* out, std::uint32_t value);
};

}  // namespace homepoint::system
