#pragma once
#include <cstdint>

#include "core/net_trail.hpp"
#include "core/play_limit.hpp"

// 3.0.24-beta.31 (core/play_limit; R-MWNRVM and four more): the server's daily play-time limit as the game takes its
// words - remembered for the drop after them (no report, the box's words) and the login's refusal after a kick
namespace uagfx::win::playlimitwin {

void packet(uint16_t id, const uint8_t* p);  // a packet the game took (win/packet_tap.cpp): 0x008e's words
// The drop `d` the limit's kick: its words, the minutes since they came
bool kick(const nettrail::Drop& d, playlimit::Notice& n, int& pastMin);
bool kickedWithin(uint32_t ms);  // a limit's kick in this game within `ms` (the game's clock)

}  // namespace uagfx::win::playlimitwin
