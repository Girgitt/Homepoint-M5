#pragma once

#include "UiChange.h"
#include "UiController.h"
#include "../model/Model.h"

namespace homepoint::ui {

// Rendering/input backend contract. The application/controller layer owns
// semantic UI state; a backend owns framework-specific drawing and raw input.
class UiBackend {
 public:
  virtual ~UiBackend() = default;

  virtual UiLayoutMetrics layoutMetrics(bool debugMode) const = 0;
  virtual void setConfig(model::AppConfig* config) = 0;
  virtual void onDisplayModeChanged() = 0;
  virtual void applyChange(const core::UiChange& change) = 0;
  virtual void tick() = 0;
};

}  // namespace homepoint::ui
