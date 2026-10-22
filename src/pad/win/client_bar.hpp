#pragma once
#include <array>

#include "pad/core/world/world.hpp"

// The uaRO client keeps its hotkey bar to itself (it has no 0x02b9 / 0x02ba): read F1-F9 from its memory.
namespace uapad::clientbar {

// True when the running exe is the uaRO build whose layout we know (checked by code signatures).
bool supported();
// Reads F1-F9 of the first row. False when some memory was not readable; `out` is then unusable.
bool read(std::array<ClientBarSlot, kClientBarSlots>& out);

}  // namespace uapad::clientbar
