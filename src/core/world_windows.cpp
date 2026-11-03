#include "core/world_windows.hpp"

#include <algorithm>
#include <iterator>

namespace uagfx::worldwindows {
namespace {

constexpr uint32_t kWorld[] = {0x696D4C, 0x696CB4, 0x696C18, 0x696068, 0x6966A4, 0x69673C};

}  // namespace

bool worldWindow(uint32_t vtable) {
    return std::find(std::begin(kWorld), std::end(kWorld), vtable) != std::end(kWorld);
}

}  // namespace uagfx::worldwindows
