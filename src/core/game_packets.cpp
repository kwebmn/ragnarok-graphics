#include "core/game_packets.hpp"

#include <cstring>

namespace uagfx::packets {
namespace {

uint16_t rd16(const uint8_t* p, size_t at) { return uint16_t(p[at] | p[at + 1] << 8); }
uint32_t rd32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}
// A name of 24 bytes to its end (or all 24)
std::string name(const uint8_t* p, size_t at) {
    const char* s = reinterpret_cast<const char*>(p + at);
    return std::string(s, strnlen(s, 24));
}

// 3.0.11-beta.95: the per cent in a monster's name line «Lv. 24 | HP: 57%» (or «HP: 1234/5678»); -1 none
int hpOf(const std::string& s) {
    const size_t at = s.find("HP: ");
    if (at == std::string::npos) return -1;
    size_t i = at + 4;
    const auto number = [&](long long& v) {
        const size_t from = i;
        v = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9' && v < 1000000000000LL) v = v * 10 + (s[i++] - '0');
        return i > from;
    };
    long long a = 0, b = 0;
    if (!number(a)) return -1;
    if (i < s.size() && s[i] == '%') return a <= 100 ? int(a) : -1;
    if (i < s.size() && s[i] == '/' && (++i, number(b)) && b > 0) return int(a * 100 / b > 100 ? 100 : a * 100 / b);
    return -1;
}

// 3.0.11-beta.39: a map's name of 16 bytes, its «.gat» off
std::string mapAt(const uint8_t* p, size_t at) {
    const char* s = reinterpret_cast<const char*>(p + at);
    std::string m(s, strnlen(s, 16));
    if (m.size() > 4 && m.compare(m.size() - 4, 4, ".gat") == 0) m.resize(m.size() - 4);
    return m;
}

// A packet of its own length (<len>.W at 2): its text from `at` to its end (or a nul); false - cut short or longer
// than there is
bool textOf(const uint8_t* p, size_t size, size_t at, std::string& out) {
    if (size < at) return false;
    const size_t len = rd16(p, 2);
    if (len < at || len > size) return false;
    const char* t = reinterpret_cast<const char*>(p + at);
    out.assign(t, strnlen(t, len - at));
    return true;
}

}  // namespace

Event parse(const uint8_t* p, size_t size) {
    if (!p || size < 2) return {};
    switch (rd16(p, 0)) {
        case 0x0097: {
            if (size < 28) return {};
            const size_t len = rd16(p, 2);
            if (len < 28 || len > size) return {};
            const char* t = reinterpret_cast<const char*>(p + 28);
            return Whisper{name(p, 4), std::string(t, strnlen(t, len - 28))};
        }
        case 0x00fe:
            if (size < 30) return {};
            return PartyInvite{rd32(p, 2), name(p, 6)};
        case 0x00e5:
            if (size < 26) return {};
            return TradeRequest{name(p, 2)};
        case 0x016a:
            if (size < 30) return {};
            return GuildInvite{rd32(p, 2), name(p, 6)};
        case 0x0207:
            if (size < 34) return {};
            return FriendRequest{rd32(p, 2), name(p, 10)};
        case 0x0196:
            if (size < 9) return {};
            return StatusIcon{rd16(p, 2), rd32(p, 4), p[8] != 0};
        case 0x0080:
            if (size < 7) return {};
            return Vanish{rd32(p, 2), p[6]};
        case 0x00b4:  // 3.0.11-beta.98: an NPC's words
        case 0x00b7: {  // and its menu
            std::string text;
            if (size < 8 || !textOf(p, size, 8, text)) return {};
            if (rd16(p, 0) == 0x00b4) return NpcSay{rd32(p, 4), text};
            return NpcMenu{rd32(p, 4), text};
        }
        case 0x0195:  // 3.0.11-beta.95: a unit's name line - a monster's HP % in its party's place
            if (size < 102) return {};
            return ActorInfo{rd32(p, 2), hpOf(name(p, 30))};
        case 0x011a:
            if (size < 15) return {};
            return SkillCast{rd16(p, 2), rd16(p, 4), rd32(p, 6), rd32(p, 10)};
        case 0x008a: {  // 3.0.4-beta.4; 3.0.7-beta.6 a miss too (damage 0), dodged by luck (11)
            if (size < 29) return {};
            const uint8_t type = p[26];
            const int32_t damage = int16_t(rd16(p, 22));
            if ((type != 0 && type != 4 && type != 8 && type != 10 && type != 11) || damage < 0) return {};
            return Hit{rd32(p, 2), rd32(p, 6), damage};
        }
        case 0x01de: {
            if (size < 33) return {};
            const int32_t damage = int32_t(rd32(p, 24));
            if (damage < 0) return {};
            return Hit{rd32(p, 4), rd32(p, 8), damage, rd16(p, 2)};
        }
        case 0x0114: {
            if (size < 31) return {};
            const int32_t damage = int16_t(rd16(p, 24));
            if (damage < 0) return {};
            return Hit{rd32(p, 4), rd32(p, 8), damage, rd16(p, 2)};
        }
        case 0x00fb: {  // 3.0.7-beta.6: the party's list
            if (size < 28) return {};
            const size_t len = rd16(p, 2);
            if (len < 28 || len > size || (len - 28) % 46 != 0) return {};
            PartyList l;
            for (size_t at = 28; at + 46 <= len; at += 46)
                l.seats.push_back(
                    PartySeat{rd32(p, at), name(p, at + 4), mapAt(p, at + 28), p[at + 44] == 0, p[at + 45] == 0});
            return l;
        }
        case 0x01e9: {
            if (size < 63) return {};
            PartyMember m{rd32(p, 2), name(p, 39), "", rd32(p, 6) == 0, p[14] == 0};
            if (size >= 79) m.map = mapAt(p, 63);
            return m;
        }
        case 0x0105:
            if (size < 30) return {};
            return PartyLeft{rd32(p, 2)};
        case 0x0106:
            if (size < 10) return {};
            return PartyHp{rd32(p, 2), rd16(p, 6), rd16(p, 8)};
        case 0x0109: {
            PartyChat c;
            if (size < 8 || !textOf(p, size, 8, c.text)) return {};
            c.account = rd32(p, 4);
            return c;
        }
        case 0x017f: {
            GuildChat c;
            if (!textOf(p, size, 4, c.text)) return {};
            return c;
        }
        case 0x00a0:  // 3.0.11-beta.41
            if (size < 23) return {};
            return Pickup{rd16(p, 6), rd16(p, 4), p[22] == 0};
        case 0x00b1:
            if (size < 8) return {};
            return StatusLong{rd16(p, 2), rd32(p, 4)};
        case 0x0107:  // 3.0.11-beta.82: a party member's cell
            if (size < 10) return {};
            return PartyPos{rd32(p, 2), rd16(p, 6), rd16(p, 8)};
        case 0x022a:  // 3.0.11-beta.80: a character in sight
            if (size < 58) return {};
            return Appear{rd32(p, 2), rd16(p, 16), rd16(p, 18), rd16(p, 30), rd16(p, 56), p[49], p[55] == 1};
        case 0x022b:
            if (size < 57) return {};
            return Appear{rd32(p, 2), rd16(p, 16), rd16(p, 18), rd16(p, 30), rd16(p, 55), p[49]};
        case 0x022c:
            if (size < 64) return {};
            return Appear{rd32(p, 2), rd16(p, 16), rd16(p, 18), rd16(p, 34), rd16(p, 62), p[53]};
        case 0x013e:  // 3.0.11-beta.48
            if (size < 24) return {};
            return Casting{rd32(p, 2), rd32(p, 6), rd16(p, 14), rd32(p, 20)};
        case 0x00b0:  // 3.0.7-beta.7
            if (size < 8) return {};
            return Status{rd16(p, 2), rd32(p, 4)};
        case 0x008d: {
            Chat c;
            if (size < 8 || !textOf(p, size, 8, c.text)) return {};
            c.unit = rd32(p, 4);
            return c;
        }
        default:
            return {};
    }
}

}  // namespace uagfx::packets
