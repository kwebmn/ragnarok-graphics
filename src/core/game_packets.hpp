#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace uagfx::packets {

// 3.0.4-beta.3 (the user's ideas 2, 4, 8): the zone server's packets the driver reads as uaRO.exe takes them - its
// packet loop's read at 0x579b02 (win/packet_tap.cpp): who whispers, invites, asks to trade or to be a friend, the
// player's status icons, deaths, skills cast without damage (the buffs, with their level). Names and texts in the
// game's code page, as they came.
struct Whisper {  // 0x0097: <len>.W <nick>.24B <text>
    std::string from, text;
};
struct PartyInvite {  // 0x00fe: <account>.L <party>.24B
    uint32_t account = 0;
    std::string party;
};
struct TradeRequest {  // 0x00e5: <nick>.24B
    std::string from;
};
struct GuildInvite {  // 0x016a: <guild id>.L <guild>.24B
    uint32_t id = 0;
    std::string guild;
};
struct FriendRequest {  // 0x0207: <account>.L <char>.L <nick>.24B
    uint32_t account = 0;
    std::string from;
};
struct StatusIcon {  // 0x0196: <icon>.W <unit>.L <on>.B
    uint16_t icon = 0;
    uint32_t id = 0;
    bool on = false;
};
struct Vanish {  // 0x0080: <unit>.L <type>.B (1: died)
    uint32_t id = 0;
    uint8_t type = 0;
};
struct SkillCast {  // 0x011a: <skill>.W <level>.W <target>.L <source>.L <result>.B
    uint16_t skill = 0, level = 0;
    uint32_t target = 0, source = 0;
};

using Event = std::variant<std::monostate, Whisper, PartyInvite, TradeRequest, GuildInvite, FriendRequest, StatusIcon,
                           Vanish, SkillCast>;

// A packet as the game took it, `size` bytes of it readable: one of the above, or nothing (another packet, one cut
// short or naming more bytes than there are).
Event parse(const uint8_t* p, size_t size);

}  // namespace uagfx::packets
