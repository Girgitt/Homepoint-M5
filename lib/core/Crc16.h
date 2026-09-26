#pragma once

#include <cstddef>
#include <cstdint>

namespace homepoint::core {

inline std::uint16_t crc16Ibm(const std::uint8_t* data, std::size_t length) {
  std::uint16_t crc = 0xFFFFu;
  for (std::size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) ? static_cast<std::uint16_t>((crc >> 1u) ^ 0xA001u)
                       : static_cast<std::uint16_t>(crc >> 1u);
    }
  }
  return crc;
}

}  // namespace homepoint::core
