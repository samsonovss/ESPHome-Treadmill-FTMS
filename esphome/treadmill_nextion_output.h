#pragma once

#include "treadmill_display_output.h"
#include "esphome/components/nextion/nextion.h"

namespace treadmill_display {
inline esphome::nextion::Nextion *nextion = nullptr;
}  // namespace treadmill_display

#undef TREADMILL_DISPLAY_OUTPUT
#define TREADMILL_DISPLAY_OUTPUT(...) \
  do { \
    if (::treadmill_display::nextion) ::treadmill_display::nextion->__VA_ARGS__; \
  } while (false)
