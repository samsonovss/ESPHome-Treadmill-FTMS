#pragma once

namespace treadmill_display {
inline void (*phase_beep)() = nullptr;
inline void beep() {
  if (phase_beep) phase_beep();
}
}  // namespace treadmill_display

// Optional display output only. Arguments are not evaluated without a backend.
#define TREADMILL_DISPLAY_OUTPUT(...) do {} while (false)
