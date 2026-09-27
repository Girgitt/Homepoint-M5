#include <unity.h>

#include "PressGesture.h"

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
