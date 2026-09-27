#pragma once

#include <cstdint>

namespace homepoint::core {

enum class HapticCue : std::uint8_t {
  Press,
  LongPress,
};

struct HapticOutput {
  bool changed = false;
  std::uint8_t level = 0;
};

// UI-framework-independent haptic pulse controller.
//
// The input/gesture layer emits semantic cues. This class owns only pulse
// timing and desired motor level; it has no Arduino, M5Unified, M5GFX, or
// LVGL dependency. A platform backend applies the returned HapticOutput.
class HapticFeedback {
 public:
  static constexpr std::uint8_t kMotorLevel = 255;
  static constexpr std::uint32_t kPressPulseMs = 100;
  static constexpr std::uint32_t kLongPressPulseMs = 250;

  HapticOutput trigger(HapticCue cue, std::uint32_t now) {
    active_ = true;
    startedAt_ = now;
    durationMs_ = cue == HapticCue::Press
        ? kPressPulseMs
        : kLongPressPulseMs;
    return {true, kMotorLevel};
  }

  HapticOutput update(std::uint32_t now) {
    if (!active_) return {};
    if (static_cast<std::uint32_t>(now - startedAt_) < durationMs_) {
      return {};
    }

    active_ = false;
    return {true, 0};
  }

  HapticOutput stop() {
    if (!active_) return {};
    active_ = false;
    return {true, 0};
  }

  bool active() const { return active_; }

 private:
  bool active_ = false;
  std::uint32_t startedAt_ = 0;
  std::uint32_t durationMs_ = 0;
};

}  // namespace homepoint::core
