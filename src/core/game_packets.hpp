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
    uint16_t skill = 0;  // 3.0.11-beta.41 (the chat's «Бій»): a skill's (0x01de, 0x0114 <skill>.W at 2); 0 an attack
};
// 3.0.7-beta.6 (the user's notifications 6, 7, 8; uAthena clif.c): the party - 0x00fb its list <len>.W <party>.24B
// {<account>.L <nick>.24B <map>.16B <leader>.B <offline>.B}.46B* from 28; 0x01e9 one joining or changed (<account>.L
// at 2, <nick>.24B at 39); 0x0105 one gone <account>.L <nick>.24B <type>.B; 0x0106 one's HP <account>.L <hp>.W
// <most>.W (both a per cent when the most is over 32767), sent to those near
// 3.0.11-beta.39 (the party's panel): each seat's map (without its .gat), whether the leader, whether online - the
// list's <map>.16B at 28, <leader>.B at 44 (0 the leader), <offline>.B at 45 of each; 0x01e9 <leader>.L at 6,
// <offline>.B at 14, <map>.16B at 63
struct PartySeat {
    uint32_t account = 0;
    std::string name, map;
    bool leader = false, online = true;
};
struct PartyList {
    std::vector<PartySeat> seats;
};
struct PartyMember {
    uint32_t account = 0;
    std::string name, map;
    bool leader = false, online = true;
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
// 3.0.7-beta.7 (the user: «когда хп и сп восстановились»): the character's own status - 0x00b0 <type>.W <value>.L
// (5 HP, 6 its most, 7 SP, 8 its most, ...)
struct Status {
    uint16_t type = 0;
    uint32_t value = 0;
};

// 3.0.11-beta.41 (the chat's «Бій»): an item into the bag - 0x00a0 <index>.W <amount>.W <item>.W .. <result>.B at 22
// (0 taken; 1 the bag full ...); the character's experience - 0x00b1 <type>.W <value>.L (1 base, 2 job, 0x14 zeny)
struct Pickup {
    uint16_t item = 0, amount = 0;
    bool ok = false;
};
struct StatusLong {
    uint16_t type = 0;
    uint32_t value = 0;
};

// 3.0.11-beta.48 (the cast's label over the hero): a skill's cast begun - 0x013e <source>.L <target>.L <x>.W <y>.W
// <skill>.W <element>.L <time>.L (the game's FUN_0058b560: the caster's gauge for `ms`)
struct Casting {
    uint32_t source = 0, target = 0;
    uint16_t skill = 0;
    uint32_t ms = 0;
};

// 3.0.11-beta.80 (the owner: «иконки можно сделать как лица персонажей?»): a character in sight, as uAthena's
// PACKETVER 7 sends it (clif.c clif_set0078/007b and their spawn): 0x022a standing and 0x022b come in - <unit>.L at 2,
// <class>.W at 16, <hair>.W at 18, <hair colour>.W at 30, <sex>.B at 49, <level>.W at 56 (0x022b: at 55); 0x022c
// walking - the hair colour at 34, the sex at 53, the level at 62. A player's unit is its account
struct Appear {
    uint32_t id = 0;
    uint16_t job = 0, hair = 0, hairColour = 0, level = 0;
    uint8_t sex = 0;
    bool dead = false;  // 3.0.11-beta.81: 0x022a's <dead_sit>.B at 55 is 1 (2: sitting); come in or walking: alive
};

// 3.0.11-beta.82: a party member's cell - 0x0107 <account>.L <x>.W <y>.W (uAthena's clif_party_xy, to the party on
// its map)
struct PartyPos {
    uint32_t account = 0;
    uint16_t x = 0, y = 0;
};

// 3.0.11-beta.95 (the owner: «Полоски HP мобов - всем показывай»): a unit's name line - 0x0195 <unit>.L <name>.24B
// <party>.24B <guild>.24B <title>.24B; for a monster uAthena's show_mob_info 6 puts «Lv. 24 | HP: 57%» in the party's
// place, sent to everyone near on each hit, heal and spawn (status_damage / status_heal)
struct ActorInfo {
    uint32_t id = 0;
    int hp = -1;  // per cent; -1 none (a character's party name)
};

using Event = std::variant<std::monostate, Whisper, PartyInvite, TradeRequest, GuildInvite, FriendRequest, StatusIcon,
                           Vanish, SkillCast, Hit, PartyList, PartyMember, PartyLeft, PartyHp, PartyChat, GuildChat,
                           Chat, Status, Pickup, StatusLong, Casting, Appear, PartyPos, ActorInfo>;

// A packet as the game took it, `size` bytes of it readable: one of the above, or nothing (another packet, one cut
// short or naming more bytes than there are).
Event parse(const uint8_t* p, size_t size);

}  // namespace uagfx::packets
