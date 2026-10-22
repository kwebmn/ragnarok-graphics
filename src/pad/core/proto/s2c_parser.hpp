#pragma once
#include <optional>

#include "pad/core/proto/events.hpp"

namespace uapad {

// Parses one whole map server -> client packet (as cut by StreamFramer).
// Packets the pad does not care about, and truncated ones, give nullopt.
std::optional<Event> parseS2C(const Bytes& p);

}  // namespace uapad
