#pragma once

#include <cstdint>

namespace homepoint::core {

using DeadlineHandle = std::uint8_t;
constexpr DeadlineHandle kInvalidDeadlineHandle = 0xFF;
using DeadlineCallback = void (*)(void* context);

// Platform-neutral one-shot deadline scheduler.
//
// Callbacks execute on the scheduler's worker context, not the UI/main task.
// They must therefore be short and must not draw or mutate UI objects. A
// callback may perform timing-sensitive hardware work (for example switching
// the vibration motor off) or post an atomic/queued semantic event for the
// main application task to consume.
class DeadlineScheduler {
 public:
  virtual ~DeadlineScheduler() = default;

  // Registers a callback once and returns a stable handle for its lifetime.
  // Implementations may provide a fixed number of slots and return
  // kInvalidDeadlineHandle if none remain.
  virtual DeadlineHandle registerCallback(
      DeadlineCallback callback,
      void* context) = 0;

  // Arms/re-arms a one-shot deadline. Re-arming the same handle replaces its
  // previous deadline; a stale callback from the previous generation must not
  // be delivered.
  virtual bool schedule(DeadlineHandle handle, std::uint32_t delayMs) = 0;

  // Cancels an armed deadline. It is safe to cancel an idle handle.
  virtual void cancel(DeadlineHandle handle) = 0;
};

}  // namespace homepoint::core
