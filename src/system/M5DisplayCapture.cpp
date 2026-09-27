#include "M5DisplayCapture.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace homepoint::system {
namespace {

constexpr std::size_t kBmpHeaderSize = 54;

}  // namespace

bool M5DisplayCapture::begin() {
  if (ready_) return true;
  resultMutex_ = xSemaphoreCreateMutex();
  ready_ = resultMutex_ != nullptr;

  Serial.printf(
      "[DISPLAY] screenshot service: %s\n",
      ready_ ? "ready" : "unavailable");
  return ready_;
}

core::DisplayCaptureResult M5DisplayCapture::requestSnapshot(
    std::uint32_t& requestId) {
  requestId = 0;
  if (!ready_) return core::DisplayCaptureResult::Unavailable;

  std::uint32_t id = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
  if (id == 0) {
    id = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
  }

  std::uint32_t expected = 0;
  if (!activeId_.compare_exchange_strong(
          expected, id, std::memory_order_acq_rel)) {
    requestId = expected;
    return core::DisplayCaptureResult::Busy;
  }

  requestedId_.store(id, std::memory_order_release);
  requestId = id;
  return core::DisplayCaptureResult::Ok;
}

core::DisplayCaptureResult M5DisplayCapture::readSnapshot(
    std::uint32_t requestId,
    core::DisplaySnapshot& snapshot) {
  if (!ready_) return core::DisplayCaptureResult::Unavailable;

  if (xSemaphoreTake(resultMutex_, 0) != pdTRUE) {
    // The result lock is held only while the UI task publishes a completed
    // frame. Never block AsyncTCP; a caller can simply poll again.
    return core::DisplayCaptureResult::Pending;
  }

  const auto atOrAfter = [](std::uint32_t completed, std::uint32_t requested) {
    return static_cast<std::int32_t>(completed - requested) >= 0;
  };

  if (requestId == 0) {
    if (successfulId_ != 0 && completedSnapshot_.data && completedSnapshot_.size) {
      snapshot = completedSnapshot_;
      xSemaphoreGive(resultMutex_);
      return core::DisplayCaptureResult::Ok;
    }
    const bool failed = lastAttemptId_ != 0 && !lastAttemptOk_;
    xSemaphoreGive(resultMutex_);
    if (failed) return core::DisplayCaptureResult::Failed;
    return activeId_.load(std::memory_order_acquire) != 0
        ? core::DisplayCaptureResult::Pending
        : core::DisplayCaptureResult::NotFound;
  }

  if (successfulId_ != 0 && atOrAfter(successfulId_, requestId) &&
      completedSnapshot_.data && completedSnapshot_.size) {
    snapshot = completedSnapshot_;
    xSemaphoreGive(resultMutex_);
    return core::DisplayCaptureResult::Ok;
  }

  const bool attemptCompleted =
      lastAttemptId_ != 0 && atOrAfter(lastAttemptId_, requestId);
  const bool attemptFailed = attemptCompleted && !lastAttemptOk_;
  xSemaphoreGive(resultMutex_);

  if (attemptFailed) return core::DisplayCaptureResult::Failed;

  const std::uint32_t active = activeId_.load(std::memory_order_acquire);
  if (active != 0 && atOrAfter(active, requestId)) {
    return core::DisplayCaptureResult::Pending;
  }
  return core::DisplayCaptureResult::NotFound;
}

void M5DisplayCapture::service() {
  if (!ready_) return;

  const std::uint32_t requestId =
      requestedId_.exchange(0, std::memory_order_acq_rel);
  if (!requestId) return;

  core::DisplaySnapshot snapshot = captureBmp();
  const bool ok = snapshot.data && snapshot.size;

  if (xSemaphoreTake(resultMutex_, portMAX_DELAY) == pdTRUE) {
    lastAttemptId_ = requestId;
    lastAttemptOk_ = ok;
    if (ok) {
      completedSnapshot_ = std::move(snapshot);
      successfulId_ = requestId;
    }
    xSemaphoreGive(resultMutex_);
  }

  activeId_.store(0, std::memory_order_release);
}

core::DisplaySnapshot M5DisplayCapture::captureBmp() const {
  core::DisplaySnapshot snapshot;

  const int width = M5.Display.width();
  const int height = M5.Display.height();
  if (width <= 0 || height <= 0 || width > 65535 || height > 65535) {
    Serial.printf("[DISPLAY] screenshot invalid geometry: %dx%d\n", width, height);
    return snapshot;
  }

  const std::size_t rowBytes = static_cast<std::size_t>(width) * 3u;
  const std::size_t rowStride = (rowBytes + 3u) & ~std::size_t(3u);
  const std::size_t pixelBytes = rowStride * static_cast<std::size_t>(height);
  const std::size_t totalBytes = kBmpHeaderSize + pixelBytes;

  auto* raw = static_cast<std::uint8_t*>(heap_caps_malloc(
      totalBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!raw) {
    raw = static_cast<std::uint8_t*>(heap_caps_malloc(
        totalBytes, MALLOC_CAP_8BIT));
  }
  if (!raw) {
    Serial.printf(
        "[DISPLAY] screenshot allocation failed: %u bytes\n",
        static_cast<unsigned>(totalBytes));
    return snapshot;
  }

  std::memset(raw, 0, kBmpHeaderSize);
  raw[0] = 'B';
  raw[1] = 'M';
  writeLe32(raw + 2, static_cast<std::uint32_t>(totalBytes));
  writeLe32(raw + 10, static_cast<std::uint32_t>(kBmpHeaderSize));
  writeLe32(raw + 14, 40u);
  writeLe32(raw + 18, static_cast<std::uint32_t>(width));
  writeLe32(raw + 22, static_cast<std::uint32_t>(height));
  writeLe16(raw + 26, 1u);
  writeLe16(raw + 28, 24u);
  writeLe32(raw + 34, static_cast<std::uint32_t>(pixelBytes));
  writeLe32(raw + 38, 2835u);
  writeLe32(raw + 42, 2835u);

  auto* rgbRow = static_cast<std::uint8_t*>(heap_caps_malloc(
      rowBytes, MALLOC_CAP_8BIT));
  if (!rgbRow) {
    heap_caps_free(raw);
    Serial.printf(
        "[DISPLAY] screenshot row allocation failed: %u bytes\n",
        static_cast<unsigned>(rowBytes));
    return snapshot;
  }

  for (int y = 0; y < height; ++y) {
    auto* row = raw + kBmpHeaderSize
        + static_cast<std::size_t>(height - 1 - y) * rowStride;
    M5.Display.readRectRGB(0, y, width, 1, rgbRow);
    for (int x = 0; x < width; ++x) {
      const std::size_t offset = static_cast<std::size_t>(x) * 3u;
      // M5GFX readRectRGB() writes R,G,B bytes; 24-bit BMP stores B,G,R.
      row[offset] = rgbRow[offset + 2u];
      row[offset + 1u] = rgbRow[offset + 1u];
      row[offset + 2u] = rgbRow[offset];
    }
    if (rowStride > rowBytes) {
      std::memset(row + rowBytes, 0, rowStride - rowBytes);
    }
  }
  heap_caps_free(rgbRow);

  snapshot.data = std::shared_ptr<std::uint8_t>(
      raw,
      [](std::uint8_t* ptr) {
        if (ptr) heap_caps_free(ptr);
      });
  snapshot.size = totalBytes;
  snapshot.width = static_cast<std::uint16_t>(width);
  snapshot.height = static_cast<std::uint16_t>(height);
  snapshot.mimeType = "image/bmp";

  return snapshot;
}

void M5DisplayCapture::writeLe16(std::uint8_t* out, std::uint16_t value) {
  out[0] = static_cast<std::uint8_t>(value & 0xffu);
  out[1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
}

void M5DisplayCapture::writeLe32(std::uint8_t* out, std::uint32_t value) {
  out[0] = static_cast<std::uint8_t>(value & 0xffu);
  out[1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
  out[2] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
  out[3] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
}

}  // namespace homepoint::system
