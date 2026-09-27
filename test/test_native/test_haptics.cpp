#include <unity.h>

#include "HapticFeedback.h"

void test_haptic_press_pulse_is_bounded() {
  using homepoint::core::HapticCue;
  using homepoint::core::HapticFeedback;

  HapticFeedback haptics;
  auto output = haptics.trigger(HapticCue::Press, 100);
  TEST_ASSERT_TRUE(output.changed);
  TEST_ASSERT_EQUAL_UINT8(HapticFeedback::kMotorLevel, output.level);
  TEST_ASSERT_TRUE(haptics.active());

  output = haptics.update(100 + HapticFeedback::kPressPulseMs - 1);
  TEST_ASSERT_FALSE(output.changed);
  TEST_ASSERT_TRUE(haptics.active());

  output = haptics.update(100 + HapticFeedback::kPressPulseMs);
  TEST_ASSERT_TRUE(output.changed);
  TEST_ASSERT_EQUAL_UINT8(0, output.level);
  TEST_ASSERT_FALSE(haptics.active());
}

void test_haptic_long_press_restarts_feedback_window() {
  using homepoint::core::HapticCue;
  using homepoint::core::HapticFeedback;

  HapticFeedback haptics;
  haptics.trigger(HapticCue::Press, 100);
  haptics.update(100 + HapticFeedback::kPressPulseMs);

  auto output = haptics.trigger(HapticCue::LongPress, 700);
  TEST_ASSERT_TRUE(output.changed);
  TEST_ASSERT_EQUAL_UINT8(HapticFeedback::kMotorLevel, output.level);

  output = haptics.update(700 + HapticFeedback::kLongPressPulseMs - 1);
  TEST_ASSERT_FALSE(output.changed);
  output = haptics.update(700 + HapticFeedback::kLongPressPulseMs);
  TEST_ASSERT_TRUE(output.changed);
  TEST_ASSERT_EQUAL_UINT8(0, output.level);
}

void test_haptic_stop_turns_active_pulse_off_immediately() {
  homepoint::core::HapticFeedback haptics;
  haptics.trigger(homepoint::core::HapticCue::Press, 10);
  const auto output = haptics.stop();
  TEST_ASSERT_TRUE(output.changed);
  TEST_ASSERT_EQUAL_UINT8(0, output.level);
  TEST_ASSERT_FALSE(haptics.active());
}
