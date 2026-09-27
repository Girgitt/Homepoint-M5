#include <unity.h>

#include "ScreenPowerFsm.h"

void test_screen_power_timeout_turns_display_off() {
  homepoint::core::ScreenPowerFsm fsm;
  const auto step = fsm.step(homepoint::core::ScreenTouchPhase::None, true);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::ScreenPowerState::Off),
      static_cast<int>(fsm.state()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::ScreenPowerAction::TurnOff),
      static_cast<int>(step.action));
  TEST_ASSERT_FALSE(step.consumeTouch);
}

void test_screen_power_wake_gesture_is_consumed_through_release() {
  using homepoint::core::ScreenPowerAction;
  using homepoint::core::ScreenPowerFsm;
  using homepoint::core::ScreenPowerState;
  using homepoint::core::ScreenTouchPhase;

  ScreenPowerFsm fsm;
  fsm.step(ScreenTouchPhase::None, true);

  auto step = fsm.step(ScreenTouchPhase::Pressed, false);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ScreenPowerState::WakeGuard),
      static_cast<int>(fsm.state()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ScreenPowerAction::TurnOn),
      static_cast<int>(step.action));
  TEST_ASSERT_TRUE(step.consumeTouch);

  step = fsm.step(ScreenTouchPhase::Pressed, false);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ScreenPowerState::WakeGuard),
      static_cast<int>(fsm.state()));
  TEST_ASSERT_TRUE(step.consumeTouch);

  step = fsm.step(ScreenTouchPhase::Released, false);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ScreenPowerState::Awake),
      static_cast<int>(fsm.state()));
  TEST_ASSERT_TRUE(step.consumeTouch);

  step = fsm.step(ScreenTouchPhase::None, false);
  TEST_ASSERT_FALSE(step.consumeTouch);
  TEST_ASSERT_TRUE(fsm.acceptsUiInput());
}

void test_screen_power_does_not_sleep_under_active_touch() {
  homepoint::core::ScreenPowerFsm fsm;
  const auto step = fsm.step(homepoint::core::ScreenTouchPhase::Pressed, true);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::ScreenPowerState::Awake),
      static_cast<int>(fsm.state()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::ScreenPowerAction::None),
      static_cast<int>(step.action));
}
