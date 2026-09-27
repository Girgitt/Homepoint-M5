#include <unity.h>

#include "Crc16.h"

void test_crc16_ibm_known_vector() {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>("123456789");
  TEST_ASSERT_EQUAL_HEX16(0x4B37, homepoint::core::crc16Ibm(bytes, 9));
}
