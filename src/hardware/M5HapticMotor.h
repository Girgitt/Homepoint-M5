#pragma once

#include <M5Unified.h>
#include <cstdint>

#include "HapticFeedback.h"

namespace homepoint::hardware {

// M5Unified-backed actuator for the renderer-independent haptic controller.
// Keeping PMIC access here avoids coupling gesture/UI semantics to a specific
// Core2 power-management chip or to the current M5GFX renderer.
class M5HapticMotor {
 public:
  void apply(const core::HapticOutput& output) {
    if (!output.changed) return;
    M5.Power.setVibration(output.level);
  }

  void stop() {
    M5.Power.setVibration(0);
  }
};

}  // namespace homepoint::hardware
