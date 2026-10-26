#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

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

// 3.0.4-beta.4: a unit hurt - 0x008a (damage, endured, several hits, critical) <source>.L <target>.L .. <damage>.W
// at 22, <type>.B at 26; a skill's 0x01de <damage>.L at 24, 0x0114 <damage>.W at 24. 3.0.7-beta.6: a miss (0, or
// type 11 dodged by luck) an attack too - damage 0
struct Hit {
    uint32_t source = 0, target = 0;
    int32_t damage = 0;
};
// 3.0.7-beta.6 (the user's notifications 6, 7, 8; uAthena clif.c): the party - 0x00fb its list <len>.W <party>.24B
// {<account>.L <nick>.24B <map>.16B <leader>.B <offline>.B}.46B* from 28; 0x01e9 one joining or changed (<account>.L
// at 2, <nick>.24B at 39); 0x0105 one gone <account>.L <nick>.24B <type>.B; 0x0106 one's HP <account>.L <hp>.W
// <most>.W (both a per cent when the most is over 32767), sent to those near
struct PartyList {
    std::vector<std::pair<uint32_t, std::string>> members;  // account, name
};
struct PartyMember {
    uint32_t account = 0;
    std::string name;
};
struct PartyLeft {
    uint32_t account = 0;
};
struct PartyHp {
    uint32_t account = 0;
    uint16_t hp = 0, most = 0;
};
// the chat, each text "Name : words": 0x0109 the party's <len>.W <account>.L <text> at 8, 0x017f the guild's <len>.W
// <text> at 4, 0x008d one near <len>.W <unit>.L <text> at 8
struct PartyChat {
    uint32_t account = 0;
    std::string text;
};
struct GuildChat {
    std::string text;
};
struct Chat {
    uint32_t unit = 0;
    std::string text;
};

using Event = std::variant<std::monostate, Whisper, PartyInvite, TradeRequest, GuildInvite, FriendRequest, StatusIcon,
                           Vanish, SkillCast, Hit, PartyList, PartyMember, PartyLeft, PartyHp, PartyChat, GuildChat,
                           Chat>;

// A packet as the game took it, `size` bytes of it readable: one of the above, or nothing (another packet, one cut
// short or naming more bytes than there are).
Event parse(const uint8_t* p, size_t size);

}  // namespace uagfx::packets
