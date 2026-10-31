#include "pad/core/proto/s2c_parser.hpp"

#include <algorithm>
#include <cstring>

namespace uapad {
namespace {

std::string mapName(const Bytes& p, size_t o) {
    std::string s;
    for (size_t i = o; i < o + 16 && i < p.size() && p[i] != 0; ++i) s.push_back(char(p[i]));
    return s;
}

// 3.0.11-beta.86: «Lv. 24 | HP: 57%» or «HP: 300/600» (uAthena's show_mob_info) - its per cent; -1 none
int hpOf(const std::string& s) {
    const size_t at = s.find("HP: ");
    if (at == std::string::npos) return -1;
    size_t i = at + 4;
    auto number = [&](long long& v) {
        const size_t from = i;
        v = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9' && v < 1000000000000LL) v = v * 10 + (s[i++] - '0');
        return i > from;
    };
    long long a = 0, b = 0;
    if (!number(a)) return -1;
    if (i < s.size() && s[i] == '%') return a <= 100 ? int(a) : -1;
    if (i < s.size() && s[i] == '/') {
        ++i;
        if (!number(b) || b <= 0) return -1;
        return int(std::min<long long>(100, a * 100 / b));
    }
    return -1;
}

}  // namespace

std::optional<Event> parseS2C(const Bytes& p) {
    if (p.size() < 2) return std::nullopt;
    const uint8_t* d = p.data();
    const size_t n = p.size();
    switch (rd16(d, 0)) {
        case 0x0073:
            if (n < 11) break;
            return EvAuthOk{decodePos3(d, 6)};
        case 0x0091:
            if (n < 22) break;
            return EvMapChange{mapName(p, 2), Cell{rd16(d, 18), rd16(d, 20)}};
        case 0x0092:
            if (n < 28) break;
            return EvMapChange{mapName(p, 2), Cell{rd16(d, 18), rd16(d, 20)}};
        case 0x0087: {
            if (n < 12) break;
            const Move m = decodePos6(d, 6);
            return EvSelfWalk{m.from, m.to};
        }
        case 0x0088:
        case 0x01ff:
            if (n < 10) break;
            return EvSetPos{rd32(d, 2), Cell{rd16(d, 6), rd16(d, 8)}};
        case 0x0080:
            if (n < 7) break;
            return EvVanish{rd32(d, 2), rd8(d, 6)};
        case 0x0078: {
            if (n < 54) break;
            const Cell c = decodePos3(d, 46);
            return EvActor{rd32(d, 2), rd16(d, 14), rd16(d, 16), rd16(d, 6), c,
                           c,          rd16(d, 8),  rd16(d, 10), rd16(d, 12)};
        }
        case 0x007b: {  // a non-player walking into view on PACKETVER < 7 (this server sends 0x022c instead)
            if (n < 60) break;
            const Move m = decodePos6(d, 50);
            return EvActor{rd32(d, 2), rd16(d, 14), rd16(d, 16), rd16(d, 6), m.from,
                           m.to,       rd16(d, 8),  rd16(d, 10), rd16(d, 12)};
        }
        case 0x007c: {
            if (n < 41) break;
            const Cell c = decodePos3(d, 36);
            return EvActor{rd32(d, 2), rd16(d, 20), rd16(d, 14), rd16(d, 6), c,
                           c,          rd16(d, 8),  rd16(d, 10), rd16(d, 12)};
        }
        case 0x022c: {
            if (n < 64) break;
            const Move m = decodePos6(d, 54);
            return EvActor{rd32(d, 2), rd16(d, 16), rd16(d, 18), rd16(d, 6), m.from,
                           m.to,       rd16(d, 8),  rd16(d, 10), rd32(d, 12)};
        }
        case 0x0119:  // clif_changeoption before PACKETVER 7
            if (n < 13) break;
            return EvActorOption{rd32(d, 2), rd16(d, 6), rd16(d, 8), rd16(d, 10)};
        case 0x0229:  // clif_changeoption
            if (n < 15) break;
            return EvActorOption{rd32(d, 2), rd16(d, 6), rd16(d, 8), rd32(d, 10)};
        case 0x0086: {
            if (n < 16) break;
            const Move m = decodePos6(d, 6);
            return EvActorMove{rd32(d, 2), m.from, m.to};
        }
        case 0x01a4:
            if (n < 11) break;
            return EvPetInfo{rd8(d, 2), rd32(d, 3)};
        case 0x008a:
            if (n < 29) break;
            return EvAction{rd32(d, 2), rd32(d, 6), rd8(d, 26)};
        case 0x01de:
        case 0x0114:
        case 0x0115:
            if (n < 12) break;
            return EvSkillHit{rd16(d, 2), rd32(d, 4), rd32(d, 8)};
        case 0x009d:
        case 0x009e:
            if (n < 17) break;
            return EvGroundItem{rd32(d, 2), rd16(d, 6), Cell{rd16(d, 9), rd16(d, 11)}};
        case 0x00a1:
            if (n < 6) break;
            return EvGroundItemGone{rd32(d, 2)};
        case 0x01ee: {
            EvInventory ev;
            for (size_t o = 4; o + 18 <= n; o += 18)
                ev.items.push_back(InvEntry{rd16(d, o), rd16(d, o + 2), rd16(d, o + 6), false});
            return ev;
        }
        case 0x00a0:
            if (n < 23 || rd8(d, 22) != 0) break;  // non-zero = the item was not added
            return EvItemAdd{InvEntry{rd16(d, 2), rd16(d, 6), rd16(d, 4), rd16(d, 19) != 0}};
        case 0x00af:
            if (n < 6) break;
            return EvItemRemove{rd16(d, 2), rd16(d, 4)};
        case 0x01c8:
            if (n < 13) break;
            return EvItemUsed{rd16(d, 2), rd16(d, 10), rd32(d, 6), rd8(d, 12) != 0};
        case 0x00a8:
            if (n < 7) break;
            return EvItemUsed{rd16(d, 2), rd16(d, 4), 0, rd8(d, 6) != 0};
        case 0x010f: {
            EvSkills ev;
            for (size_t o = 4; o + 37 <= n; o += 37) {
                const char* name = reinterpret_cast<const char*>(d + o + 12);
                SkillInfo s{rd16(d, o), rd32(d, o + 2), rd16(d, o + 6), rd16(d, o + 10),
                            std::string(name, strnlen(name, 24))};
                ev.skills.push_back(std::move(s));
            }
            return ev;
        }
        case 0x0095:
        case 0x0195: {
            if (n < 30) break;
            const char* name = reinterpret_cast<const char*>(d + 6);
            EvActorName ev{rd32(d, 2), std::string(name, strnlen(name, 24))};
            if (rd16(d, 0) == 0x0195 && n >= 54) {
                const char* party = reinterpret_cast<const char*>(d + 30);
                ev.hp = hpOf(std::string(party, strnlen(party, 24)));
            }
            return ev;
        }
        case 0x010e:
            if (n < 11) break;
            return EvSkillUp{rd16(d, 2), rd16(d, 4), rd16(d, 8)};
        case 0x02b9: {
            if (n < 191) break;
            EvHotkeys ev;
            for (size_t i = 0; i < ev.keys.size(); ++i) {
                const size_t o = 2 + i * 7;
                ev.keys[i] = Hotkey{rd8(d, o), rd32(d, o + 1), rd16(d, o + 5)};
            }
            return ev;
        }
        case 0x00b0:  // clif_updatestatus; only SP_SPEED (type 0) matters to us
            if (n < 8 || rd16(d, 2) != 0 || rd32(d, 4) == 0) break;
            return EvSelfSpeed{uint16_t(rd32(d, 4))};
        case 0x0148:
            if (n < 8) break;
            return EvResurrect{rd32(d, 2)};
        default:
            break;
    }
    return std::nullopt;
}

}  // namespace uapad
