#include <unity.h>

#include "StatusCenterCycle.h"

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
