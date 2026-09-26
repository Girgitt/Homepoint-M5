#include <unity.h>

#include "Crc16.h"
#include "PressGesture.h"
#include "ScreenPowerFsm.h"
#include "StatusCenterCycle.h"
#include "UiChange.h"
#include "TileInteraction.h"

void setUp() {}
void tearDown() {}

void test_crc16_ibm_known_vector() {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>("123456789");
  TEST_ASSERT_EQUAL_HEX16(0x4B37, homepoint::core::crc16Ibm(bytes, 9));
}

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

  // M5Unified reports touch_end here, and wasClicked() would be true. The FSM
  // must still consume this update instead of forwarding it to the UI.
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
  const auto step =
      fsm.step(homepoint::core::ScreenTouchPhase::Pressed, true);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::ScreenPowerState::Awake),
      static_cast<int>(fsm.state()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::ScreenPowerAction::None),
      static_cast<int>(step.action));
}

void test_press_gesture_short_press_on_release_before_threshold() {
  using homepoint::core::PressGesture;
  using homepoint::core::PressGestureClassifier;
  using homepoint::core::PressPhase;

  PressGestureClassifier classifier;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::None),
      static_cast<int>(classifier.step(PressPhase::Pressed, 100, 600)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::None),
      static_cast<int>(classifier.step(PressPhase::Held, 500, 600)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::ShortPress),
      static_cast<int>(classifier.step(PressPhase::Released, 650, 600)));
  TEST_ASSERT_FALSE(classifier.active());
}

void test_press_gesture_long_press_emits_once_and_consumes_release() {
  using homepoint::core::PressGesture;
  using homepoint::core::PressGestureClassifier;
  using homepoint::core::PressPhase;

  PressGestureClassifier classifier;
  classifier.step(PressPhase::Pressed, 100, 600);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::LongPress),
      static_cast<int>(classifier.step(PressPhase::Held, 700, 600)));
  TEST_ASSERT_TRUE(classifier.active());

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::None),
      static_cast<int>(classifier.step(PressPhase::Held, 900, 600)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::None),
      static_cast<int>(classifier.step(PressPhase::Released, 950, 600)));
  TEST_ASSERT_FALSE(classifier.active());
}

void test_press_gesture_cancel_suppresses_action_until_release() {
  using homepoint::core::PressGesture;
  using homepoint::core::PressGestureClassifier;
  using homepoint::core::PressPhase;

  PressGestureClassifier classifier;
  classifier.step(PressPhase::Pressed, 100, 600);
  classifier.cancel();
  TEST_ASSERT_TRUE(classifier.active());
  TEST_ASSERT_TRUE(classifier.cancelled());

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::None),
      static_cast<int>(classifier.step(PressPhase::Held, 900, 600)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PressGesture::None),
      static_cast<int>(classifier.step(PressPhase::Released, 950, 600)));
  TEST_ASSERT_FALSE(classifier.active());
}

void test_tile_behavior_resolves_gestures_independently_of_tile_type() {
  using homepoint::core::TileAction;
  using homepoint::core::TileBehavior;
  using homepoint::core::TileGesture;
  using homepoint::core::resolveTileAction;

  const TileBehavior dimmerLike{
      TileAction::Toggle,
      TileAction::OpenDetail,
  };

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileAction::Toggle),
      static_cast<int>(resolveTileAction(dimmerLike, TileGesture::ShortPress)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileAction::OpenDetail),
      static_cast<int>(resolveTileAction(dimmerLike, TileGesture::LongPress)));
}


void test_status_center_cycle_uses_four_seconds_time_two_seconds_ip() {
  using homepoint::core::StatusCenterCycle;
  using homepoint::core::StatusCenterMode;

  StatusCenterCycle cycle;
  cycle.reset(100);

  TEST_ASSERT_TRUE(cycle.update(100, true, true));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(StatusCenterMode::Time),
      static_cast<int>(cycle.mode()));

  TEST_ASSERT_FALSE(cycle.update(4099, true, true));
  TEST_ASSERT_TRUE(cycle.update(4100, true, true));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(StatusCenterMode::Ip),
      static_cast<int>(cycle.mode()));

  TEST_ASSERT_FALSE(cycle.update(6099, true, true));
  TEST_ASSERT_TRUE(cycle.update(6100, true, true));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(StatusCenterMode::Time),
      static_cast<int>(cycle.mode()));
}

void test_status_center_cycle_handles_single_available_source() {
  using homepoint::core::StatusCenterCycle;
  using homepoint::core::StatusCenterMode;

  StatusCenterCycle cycle;
  cycle.reset(0);
  cycle.update(0, false, true);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(StatusCenterMode::Ip),
      static_cast<int>(cycle.mode()));

  cycle.update(10000, true, false);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(StatusCenterMode::Time),
      static_cast<int>(cycle.mode()));
}

void test_ui_change_preserves_model_item_coordinates() {
  const auto change = homepoint::core::UiChange::modelItem(3, 7);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::UiChangeKind::ModelItem),
      static_cast<int>(change.kind));
  TEST_ASSERT_EQUAL_UINT32(3, change.tileIndex);
  TEST_ASSERT_EQUAL_UINT32(7, change.itemIndex);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_crc16_ibm_known_vector);
  RUN_TEST(test_screen_power_timeout_turns_display_off);
  RUN_TEST(test_screen_power_wake_gesture_is_consumed_through_release);
  RUN_TEST(test_screen_power_does_not_sleep_under_active_touch);
  RUN_TEST(test_press_gesture_short_press_on_release_before_threshold);
  RUN_TEST(test_press_gesture_long_press_emits_once_and_consumes_release);
  RUN_TEST(test_press_gesture_cancel_suppresses_action_until_release);
  RUN_TEST(test_tile_behavior_resolves_gestures_independently_of_tile_type);
  RUN_TEST(test_status_center_cycle_uses_four_seconds_time_two_seconds_ip);
  RUN_TEST(test_status_center_cycle_handles_single_available_source);
  RUN_TEST(test_ui_change_preserves_model_item_coordinates);
  return UNITY_END();
}
