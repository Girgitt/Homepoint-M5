#include "EspDeadlineScheduler.h"

#include <algorithm>

namespace homepoint::system {

bool EspDeadlineScheduler::begin() {
  if (started_) return true;

  taskHandle_ = xTaskCreateStatic(
      taskThunk,
      "hp-timing",
      kTaskStackWords,
      this,
      kTaskPriority,
      taskStack_,
      &taskBuffer_);
  if (!taskHandle_) {
    Serial.println("[TIMING] failed to create worker task");
    return false;
  }

  esp_timer_create_args_t timerArgs{};
  timerArgs.callback = &EspDeadlineScheduler::timerThunk;
  timerArgs.arg = this;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "hp-deadline";
  timerArgs.skip_unhandled_events = true;

  const esp_err_t result = esp_timer_create(&timerArgs, &timer_);
  if (result != ESP_OK) {
    Serial.printf("[TIMING] esp_timer_create failed: %d\n", static_cast<int>(result));
    vTaskDelete(taskHandle_);
    taskHandle_ = nullptr;
    return false;
  }

  started_ = true;
  Serial.println("[TIMING] deadline scheduler ready");
  return true;
}

core::DeadlineHandle EspDeadlineScheduler::registerCallback(
    core::DeadlineCallback callback,
    void* context) {
  if (!callback) return core::kInvalidDeadlineHandle;

  portENTER_CRITICAL(&slotsMux_);
  for (std::size_t i = 0; i < kMaxCallbacks; ++i) {
    auto& slot = slots_[i];
    if (slot.registered) continue;

    slot.callback = callback;
    slot.context = context;
    slot.deadlineUs = 0;
    slot.generation = 0;
    slot.registered = true;
    slot.armed = false;
    portEXIT_CRITICAL(&slotsMux_);
    return static_cast<core::DeadlineHandle>(i);
  }
  portEXIT_CRITICAL(&slotsMux_);
  return core::kInvalidDeadlineHandle;
}

bool EspDeadlineScheduler::schedule(
    core::DeadlineHandle handle,
    std::uint32_t delayMs) {
  if (!started_ || handle == core::kInvalidDeadlineHandle ||
      handle >= kMaxCallbacks) {
    return false;
  }

  const std::int64_t nowUs = esp_timer_get_time();
  const std::int64_t delayUs = static_cast<std::int64_t>(delayMs) * 1000LL;

  bool accepted = false;
  portENTER_CRITICAL(&slotsMux_);
  auto& slot = slots_[handle];
  if (slot.registered) {
    ++slot.generation;
    if (slot.generation == 0) ++slot.generation;
    slot.deadlineUs = nowUs + delayUs;
    slot.armed = true;
    accepted = true;
  }
  portEXIT_CRITICAL(&slotsMux_);

  if (accepted) notifyWorker();
  return accepted;
}

void EspDeadlineScheduler::cancel(core::DeadlineHandle handle) {
  if (!started_ || handle == core::kInvalidDeadlineHandle ||
      handle >= kMaxCallbacks) {
    return;
  }

  portENTER_CRITICAL(&slotsMux_);
  auto& slot = slots_[handle];
  if (slot.registered) {
    ++slot.generation;
    if (slot.generation == 0) ++slot.generation;
    slot.armed = false;
  }
  portEXIT_CRITICAL(&slotsMux_);
  notifyWorker();
}

void EspDeadlineScheduler::timerThunk(void* arg) {
  static_cast<EspDeadlineScheduler*>(arg)->notifyWorker();
}

void EspDeadlineScheduler::taskThunk(void* arg) {
  static_cast<EspDeadlineScheduler*>(arg)->run();
}

void EspDeadlineScheduler::notifyWorker() {
  if (taskHandle_) xTaskNotifyGive(taskHandle_);
}

void EspDeadlineScheduler::run() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    serviceDeadlines();
  }
}

void EspDeadlineScheduler::serviceDeadlines() {
  for (;;) {
    PendingCallback pending[kMaxCallbacks]{};
    std::size_t pendingCount = 0;
    const std::int64_t nowUs = esp_timer_get_time();

    portENTER_CRITICAL(&slotsMux_);
    for (std::size_t i = 0; i < kMaxCallbacks; ++i) {
      auto& slot = slots_[i];
      if (!slot.registered || !slot.armed || slot.deadlineUs > nowUs) continue;

      pending[pendingCount++] = {
          static_cast<core::DeadlineHandle>(i),
          slot.callback,
          slot.context,
          slot.generation,
      };
      slot.armed = false;
    }
    portEXIT_CRITICAL(&slotsMux_);

    for (std::size_t i = 0; i < pendingCount; ++i) {
      const auto& callback = pending[i];
      // Re-check the generation immediately before dispatch. If another task
      // re-armed/cancelled this slot after it became due, this callback is
      // stale and must not be delivered.
      if (callback.callback && callbackStillCurrent(callback)) {
        callback.callback(callback.context);
      }
    }

    const std::int64_t nextDeadlineUs = earliestDeadlineUs();
    armPlatformTimer(nextDeadlineUs);

    // A due callback may have scheduled another immediate deadline. Loop once
    // more before sleeping so it does not have to wait for task scheduling.
    if (pendingCount == 0) break;
    if (nextDeadlineUs < 0 || nextDeadlineUs > esp_timer_get_time()) break;
  }
}

bool EspDeadlineScheduler::callbackStillCurrent(
    const PendingCallback& pending) {
  bool current = false;
  portENTER_CRITICAL(&slotsMux_);
  if (pending.handle < kMaxCallbacks) {
    const auto& slot = slots_[pending.handle];
    current = slot.registered && !slot.armed &&
        slot.generation == pending.generation;
  }
  portEXIT_CRITICAL(&slotsMux_);
  return current;
}

std::int64_t EspDeadlineScheduler::earliestDeadlineUs() {
  std::int64_t earliest = -1;
  portENTER_CRITICAL(&slotsMux_);
  for (const auto& slot : slots_) {
    if (!slot.registered || !slot.armed) continue;
    if (earliest < 0 || slot.deadlineUs < earliest) earliest = slot.deadlineUs;
  }
  portEXIT_CRITICAL(&slotsMux_);
  return earliest;
}

void EspDeadlineScheduler::armPlatformTimer(std::int64_t deadlineUs) {
  if (!timer_) return;

  // Only the timing worker manipulates the reusable esp_timer, so stop/start
  // operations cannot race each other. Notifications received while this runs
  // remain pending and cause an immediate worker pass afterwards.
  esp_timer_stop(timer_);
  if (deadlineUs < 0) return;

  const std::int64_t nowUs = esp_timer_get_time();
  const std::uint64_t delayUs = static_cast<std::uint64_t>(
      std::max<std::int64_t>(1, deadlineUs - nowUs));
  const esp_err_t result = esp_timer_start_once(timer_, delayUs);
  if (result != ESP_OK) {
    Serial.printf("[TIMING] esp_timer_start_once failed: %d\n", static_cast<int>(result));
  }
}

}  // namespace homepoint::system
