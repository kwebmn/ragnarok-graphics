#include "core/tk_mission.hpp"

#include <cstring>

namespace uagfx::tkmission {

bool parse(const uint8_t* p, size_t n, Mission& out) {
    if (!p || n < kPacketLen || (p[0] | p[1] << 8) != kPacketId || p[31] != kMissionKind) return false;
    char name[25] = {};
    std::memcpy(name, p + 2, 24);
    out.name = name;
    out.mob = int(uint32_t(p[26]) | uint32_t(p[27]) << 8 | uint32_t(p[28]) << 16 | uint32_t(p[29]) << 24);
    out.progress = p[30];
    return out.mob > 0;
}

std::string line(const Mission& m, const refindex::Index* index, settings::Lang l) {
    using settings::Text;
    if (!index) return {};
    const auto mob = index->mobs.find(m.mob);
    const std::string name = mob != index->mobs.end() && !mob->second.name.empty() ? mob->second.name : m.name;
    const auto where = index->spawnsOf.find(m.mob);
    if (where == index->spawnsOf.end() || where->second.empty())
        return settings::fmt(settings::tr(l, Text::TkMissionNowhere), {name});
    std::string maps;
    const std::vector<refindex::Where>& w = where->second;
    for (size_t i = 0; i < w.size() && i < kMostMaps; ++i)
        maps += (i ? ", " : "") + w[i].map + " (" + std::to_string(w[i].count) + ")";
    if (w.size() > kMostMaps)
        maps += " " + settings::fmt(settings::tr(l, Text::TkMissionMore), {std::to_string(w.size() - kMostMaps)});
    return settings::fmt(settings::tr(l, Text::TkMissionWhere), {name, maps});
}

}  // namespace uagfx::tkmission
