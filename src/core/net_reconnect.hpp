#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/net_trail.hpp"
#include "core/settings/strings.hpp"

// 3.0.22-beta.3 (the owner's «ок» to ideas 1 and 2): the game's «Disconnected from Server.» says what happened - the
// trail's drop (core/net_trail) in the player's words; after a drop of the network's on a map the game logs in again
// by itself (win/msg_box's countdown, win/login_card's «Одразу в гру») - not after a kick, nor when another device
// took the account, nor in a loop.
namespace uagfx::reconnect {

enum class Cause : uint8_t { OtherLogin, Gm, StillOnline, ServerNotice, ServerClosed, Network, Unknown };
Cause causeOf(const nettrail::Drop& d);

constexpr uint32_t kSettledMs = 60000;  // a map's connection younger than this: dropped again soon - not by itself
// The map server's connection, in the game's mode, lived a minute or more, lost by the network or closed with no
// notice: worth logging in again by itself
bool worthy(const nettrail::Drop& d);

constexpr int kCountdownS = 10;
constexpr int kMostTries = 3;
constexpr uint64_t kTriesWindowMs = 600000;  // kMostTries in this long at most
class Tries {
public:
    bool allow(uint64_t nowMs) const;
    void tried(uint64_t nowMs);

private:
    std::vector<uint64_t> at_;
};

std::string words(const nettrail::Drop& d, settings::Lang l);  // "" for an unknown cause
std::string countdown(int seconds, settings::Lang l);

}  // namespace uagfx::reconnect
