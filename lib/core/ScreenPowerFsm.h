#pragma once

#include <cstdint>

namespace homepoint::core {

enum class ScreenPowerState : std::uint8_t {
  Awake,
  Off,
  WakeGuard,
};

enum class ScreenPowerAction : std::uint8_t {
  None,
  TurnOff,
  TurnOn,
};

enum class ScreenTouchPhase : std::uint8_t {
  None,
  Pressed,
  Released,
};

struct ScreenPowerStep {
  ScreenPowerAction action = ScreenPowerAction::None;
  bool consumeTouch = false;
  bool stateChanged = false;
};

// Small, UI-framework-independent state machine for screen blanking/wakeup.
//
// The important invariant is that a touch which wakes the display is never
// delivered to the application. The FSM remains in WakeGuard until the wake
// gesture has been released (and consumes that release event as well).
class ScreenPowerFsm {
 public:
  ScreenPowerState state() const { return state_; }
  bool acceptsUiInput() const { return state_ == ScreenPowerState::Awake; }

  void forceAwake() { state_ = ScreenPowerState::Awake; }

  ScreenPowerStep step(ScreenTouchPhase touch, bool timeoutExpired) {
    switch (state_) {
      case ScreenPowerState::Awake:
        // Never blank underneath an active/releasing touch. Besides avoiding
        // surprising behavior, this ensures the gesture that put us at the
        // timeout boundary remains a normal UI gesture.
        if (timeoutExpired && touch == ScreenTouchPhase::None) {
          state_ = ScreenPowerState::Off;
          return {ScreenPowerAction::TurnOff, false, true};
        }
        return {};

      case ScreenPowerState::Off:
        if (touch != ScreenTouchPhase::None) {
          state_ = ScreenPowerState::WakeGuard;
          return {ScreenPowerAction::TurnOn, true, true};
        }
        return {};

      case ScreenPowerState::WakeGuard:
        // M5Unified emits touch_end for one update after the physical touch is
        // released. That is also the update where wasClicked() is true, so it
        // must still be consumed. If the release event was missed, None also
        // safely completes the guard on the next update.
        if (touch == ScreenTouchPhase::Released ||
            touch == ScreenTouchPhase::None) {
          state_ = ScreenPowerState::Awake;
          return {ScreenPowerAction::None, true, true};
        }
        return {ScreenPowerAction::None, true, false};
    }

    return {};
  }

 private:
  ScreenPowerState state_ = ScreenPowerState::Awake;
};

}  // namespace homepoint::core
