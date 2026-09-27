#pragma once

#include <Arduino.h>
#include <cstddef>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "DeadlineScheduler.h"

namespace homepoint::system {

// ESP32 implementation of DeadlineScheduler.
//
// Resource model is deliberately fixed and long-lived:
//   * one esp_timer created once at startup and reused for every deadline;
//   * one statically allocated FreeRTOS worker task;
//   * a fixed callback/deadline table (no allocation per scheduled action).
//
// The esp_timer callback never performs application or hardware work. It only
// wakes the worker task. This avoids doing I2C/display work in esp_timer's
// shared callback task and keeps callback latency bounded.
class EspDeadlineScheduler final : public core::DeadlineScheduler {
 public:
  static constexpr std::size_t kMaxCallbacks = 8;

  bool begin();

  core::DeadlineHandle registerCallback(
      core::DeadlineCallback callback,
      void* context) override;
  bool schedule(core::DeadlineHandle handle, std::uint32_t delayMs) override;
  void cancel(core::DeadlineHandle handle) override;

 private:
  struct Slot {
    core::DeadlineCallback callback = nullptr;
    void* context = nullptr;
    std::int64_t deadlineUs = 0;
    std::uint32_t generation = 0;
    bool registered = false;
    bool armed = false;
  };

  struct PendingCallback {
    core::DeadlineHandle handle = core::kInvalidDeadlineHandle;
    core::DeadlineCallback callback = nullptr;
    void* context = nullptr;
    std::uint32_t generation = 0;
  };

  static constexpr std::uint32_t kTaskStackWords = 2048;
  static constexpr UBaseType_t kTaskPriority = 2;

  Slot slots_[kMaxCallbacks]{};
  portMUX_TYPE slotsMux_ = portMUX_INITIALIZER_UNLOCKED;

  esp_timer_handle_t timer_ = nullptr;
  StaticTask_t taskBuffer_{};
  StackType_t taskStack_[kTaskStackWords]{};
  TaskHandle_t taskHandle_ = nullptr;
  bool started_ = false;

  static void timerThunk(void* arg);
  static void taskThunk(void* arg);

  void notifyWorker();
  void run();
  void serviceDeadlines();
  bool callbackStillCurrent(const PendingCallback& pending);
  std::int64_t earliestDeadlineUs();
  void armPlatformTimer(std::int64_t deadlineUs);
};

}  // namespace homepoint::system
