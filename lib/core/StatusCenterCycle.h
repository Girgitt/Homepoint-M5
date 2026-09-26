#pragma once

#include <cstdint>

namespace homepoint::core {

enum class StatusCenterMode {
  Fallback,
  Time,
  Ip,
};

class StatusCenterCycle {
 public:
  static constexpr std::uint32_t kTimeDurationMs = 4000;
  static constexpr std::uint32_t kIpDurationMs = 2000;

  void reset(std::uint32_t now) {
    mode_ = StatusCenterMode::Fallback;
    enteredAt_ = now;
    hadTime_ = false;
    hadIp_ = false;
  }

  bool update(std::uint32_t now, bool haveTime, bool haveIp) {
    const auto before = mode_;

    if (haveTime && haveIp) {
      // Whenever the complete pair becomes available, start a fresh cycle
      // with the more useful clock view: 4 seconds time, then 2 seconds IP.
      if (!(hadTime_ && hadIp_)) {
        mode_ = StatusCenterMode::Time;
        enteredAt_ = now;
      } else if (mode_ == StatusCenterMode::Time &&
                 elapsed(now) >= kTimeDurationMs) {
        setMode(StatusCenterMode::Ip, now);
      } else if (mode_ == StatusCenterMode::Ip &&
                 elapsed(now) >= kIpDurationMs) {
        setMode(StatusCenterMode::Time, now);
      } else if (mode_ == StatusCenterMode::Fallback) {
        setMode(StatusCenterMode::Time, now);
      }
    } else if (haveTime) {
      setMode(StatusCenterMode::Time, now);
    } else if (haveIp) {
      setMode(StatusCenterMode::Ip, now);
    } else {
      setMode(StatusCenterMode::Fallback, now);
    }

    hadTime_ = haveTime;
    hadIp_ = haveIp;
    return before != mode_;
  }

  StatusCenterMode mode() const { return mode_; }

 private:
  StatusCenterMode mode_ = StatusCenterMode::Fallback;
  std::uint32_t enteredAt_ = 0;
  bool hadTime_ = false;
  bool hadIp_ = false;

  std::uint32_t elapsed(std::uint32_t now) const { return now - enteredAt_; }

  void setMode(StatusCenterMode mode, std::uint32_t now) {
    if (mode_ == mode) return;
    mode_ = mode;
    enteredAt_ = now;
  }
};

}  // namespace homepoint::core
