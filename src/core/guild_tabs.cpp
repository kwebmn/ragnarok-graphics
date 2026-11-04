#include "core/guild_tabs.hpp"

#include <algorithm>

namespace uagfx::guildtabs {
namespace {

int u16(const uint8_t* p, size_t at) { return int(p[at] | p[at + 1] << 8); }
uint32_t u32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}
std::string cstr(const uint8_t* p, size_t at, size_t most) {
    size_t n = 0;
    while (n < most && p[at + n]) ++n;
    return std::string(reinterpret_cast<const char*>(p + at), n);
}
Position& position(Data& d, int id) {
    for (Position& q : d.positions)
        if (q.id == id) return q;
    d.positions.push_back(Position{id, "", false, false, 0});
    return d.positions.back();
}

}  // namespace

bool read(Data& d, const uint8_t* p, size_t size) {
    if (!p || size < 4) return false;
    const int id = u16(p, 0);
    const size_t len = std::min(size, size_t(id == 0x16f ? 182 : u16(p, 2)));
    switch (id) {
        case 0x166:
            for (size_t at = 4; at + 28 <= len; at += 28) position(d, int(u32(p, at))).name = cstr(p, at + 4, 24);
            break;
        case 0x160:
            for (size_t at = 4; at + 16 <= len; at += 16) {
                Position& q = position(d, int(u32(p, at)));
                const uint32_t mode = u32(p, at + 4);
                q.invite = (mode & 0x01) != 0;
                q.expel = (mode & 0x10) != 0;
                q.tax = int(u32(p, at + 12));
            }
            break;
        case 0x162:
            if (len < 6) return false;
            d.points = u16(p, 4);
            d.skills.clear();
            for (size_t at = 6; at + 37 <= len; at += 37)
                d.skills.push_back(Skill{u16(p, at), u16(p, at + 6), p[at + 36] != 0});
            break;
        case 0x163:
            d.expelled.clear();
            for (size_t at = 4; at + 88 <= len; at += 88)
                d.expelled.push_back(Expelled{cstr(p, at, 24), cstr(p, at + 24, 24), cstr(p, at + 48, 40)});
            break;
        case 0x16f:
            if (len < 182) return false;
            d.subject = cstr(p, 2, 60);
            d.body = cstr(p, 62, 120);
            break;
        default:
            return false;
    }
    std::sort(d.positions.begin(), d.positions.end(), [](const Position& a, const Position& b) { return a.id < b.id; });
    return true;
}

std::string skillName(int id) {
    static const char* const kNames[] = {"Official Guild Approval", "Kafra Contract",   "Guardian Research",
                                         "Strengthen Guardians",    "Guild Extension",  "Guild's Glory",
                                         "Great Leadership",        "Glorious Wounds",  "Cold Heart",
                                         "Sharp Gaze",              "Battle Orders",    "Regeneration",
                                         "Restoration",             "Urgent Call",      "Permanent Development"};
    const int i = id - 10000;
    return i >= 0 && i < int(sizeof kNames / sizeof kNames[0]) ? kNames[i] : "";
}

}  // namespace uagfx::guildtabs
