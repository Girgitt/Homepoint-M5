#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace homepoint::core {

struct DisplaySnapshot {
  std::shared_ptr<std::uint8_t> data;
  std::size_t size = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  const char* mimeType = "application/octet-stream";
};

enum class DisplayCaptureResult : std::uint8_t {
  Ok,
  Busy,
  Pending,
  Unavailable,
  Failed,
  NotFound,
};

class DisplayCapture {
 public:
  virtual ~DisplayCapture() = default;

  // Non-blocking. Returns Ok when a new capture is accepted. Busy may return
  // the id of the capture already in progress so multiple callers can share it.
  // Only one hardware capture is active at a time.
  virtual DisplayCaptureResult requestSnapshot(std::uint32_t& requestId) = 0;

  // Non-blocking. requestId == 0 returns the most recent successful capture.
  // A newer completed frame may satisfy an older request id.
  virtual DisplayCaptureResult readSnapshot(
      std::uint32_t requestId,
      DisplaySnapshot& snapshot) = 0;
};

}  // namespace homepoint::core
