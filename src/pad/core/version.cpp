#include "pad/core/version.hpp"

#include "core/version.hpp"

namespace uapad {

// 2.9: the pad is a part of uaRO Graphics - its version is the driver's
const char* version() { return uagfx::kVersion; }

}  // namespace uapad
