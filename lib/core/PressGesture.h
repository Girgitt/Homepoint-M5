#pragma once

#include <cstdint>

namespace homepoint::core {

enum class PressPhase : std::uint8_t {
  None,
  Pressed,
  Held,
  Released,
};

enum class PressGesture : std::uint8_t {
  None,
  ShortPress,
  LongPress,
};

// UI-framework-independent short/long press classifier.
//
// A press produces exactly one semantic gesture:
// - release before longPressMs => ShortPress
// - reaching longPressMs       => LongPress (emitted once while still held)
//
// After a long press the eventual release is consumed without generating a
// second gesture. cancel() similarly suppresses actions until that physical
// press is released; this is useful when a finger moves outside its target.
class PressGestureClassifier {
 public:
  bool active() const { return state_ != State::Idle; }
  bool cancelled() const { return state_ == State::Cancelled; }

  void reset() {
    state_ = State::Idle;
    startedAt_ = 0;
  }

  void cancel() {
    if (state_ != State::Idle) state_ = State::Cancelled;
  }

  PressGesture step(
      PressPhase phase,
      std::uint32_t now,
      std::uint32_t longPressMs) {
    switch (state_) {
      case State::Idle:
        if (phase == PressPhase::Pressed) {
          state_ = State::Pressed;
          startedAt_ = now;
        }
        return PressGesture::None;

      case State::Pressed:
        if (phase == PressPhase::Released || phase == PressPhase::None) {
          reset();
          return PressGesture::ShortPress;
        }
        if ((phase == PressPhase::Pressed || phase == PressPhase::Held) &&
            static_cast<std::uint32_t>(now - startedAt_) >= longPressMs) {
          state_ = State::LongPressEmitted;
          return PressGesture::LongPress;
        }
        return PressGesture::None;

      case State::LongPressEmitted:
        if (phase == PressPhase::Released || phase == PressPhase::None) {
          reset();
        }
        return PressGesture::None;

      case State::Cancelled:
        if (phase == PressPhase::Released || phase == PressPhase::None) {
          reset();
        }
        return PressGesture::None;
    }

    return PressGesture::None;
  }

 private:
  enum class State : std::uint8_t {
    Idle,
    Pressed,
    LongPressEmitted,
    Cancelled,
  };

  State state_ = State::Idle;
  std::uint32_t startedAt_ = 0;
};

}  // namespace homepoint::core
