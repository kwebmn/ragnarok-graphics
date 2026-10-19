#include "core/ai/pack_models.hpp"

#include <cstdlib>

#include "core/sprite_exe.hpp"

namespace uagfx::ai {
namespace {

// "ui/256x128\0123.webp" -> 256, 128
bool sizeOfName(const std::string& name, int& w, int& h) {
    const size_t slash = name.find('/');
    if (slash == std::string::npos) return false;
    char* end = nullptr;
    w = int(std::strtol(name.c_str() + slash + 1, &end, 10));
    if (!end || *end != 'x') return false;
    h = int(std::strtol(end + 1, nullptr, 10));
    return w > 0 && h > 0;
}

}  // namespace

Group groupOfPicture(const std::string& fileName, const pack::Source& s, int width, int height,
                     const WorldUses* uses) {
    switch (s.type) {
        case pack::Source::Sprite: return spriteGroupOf(fileName);
        case pack::Source::Interface: return interfaceGroupOf(fileName, width, height);
        case pack::Source::Jpeg:
            return s.kind == Kind::Icons ? interfaceGroupOf(fileName, width, height) : worldGroupOf(fileName, uses);
        default: return worldGroupOf(fileName, uses);  // an effect's texture by its folder
    }
}

std::optional<Group> groupOfEntry(const std::string&, const pack::IndexEntry& e) {
    Group g;  // "monsters" and "npcs" were families' words too: their pictures count as those groups' until then
    if (!e.group.empty() && parseGroup(e.group, g)) return g;
    return std::nullopt;
}

std::string modelOfEntry(const pack::IndexEntry& e, Group g, const AiSettings& a) {
    if (!e.model.empty()) return e.model;
    return familyOf(g) == Family::World ? a.model : a.spriteModel;
}

PackCounts countPack(const pack::Index& index, const AiSettings& a) {
    PackCounts c;
    for (const auto& p : index.pictures) {
        const std::optional<Group> g = groupOfEntry(p.first, p.second);
        if (!g) {
            ++c.unknown;
            continue;
        }
        GroupCount& gc = c.groups[size_t(*g)];
        ++gc.pictures;
        const std::string model = modelOfEntry(p.second, *g, a);
        const int m = modelIndex(model);
        if (m >= 0) ++gc.byModel[size_t(m)];
        int w = 0, h = 0;  // 1.6.3: the model it should have, small pictures too
        const std::string& want = sizeOfName(p.first, w, h) ? a.modelForPicture(*g, w, h) : a.modelFor(*g);
        if (model != want) ++gc.other;
    }
    return c;
}

bool groupOn(const AiSettings& s, Group g) {
    switch (familyOf(g)) {
        case Family::World: return sectionOf(g) == 0 ? s.ground : s.objects;
        case Family::Monsters: return s.monsters;
        case Family::Players: return s.players;
        case Family::Npcs: return s.npcs;
        case Family::Items: return s.items;
        case Family::Effects: return s.effects;
        case Family::Icons:
        case Family::Screens: return s.icons;
    }
    return false;
}

std::array<std::string, kGroups> modelsOfSet(ModelSet s) {
    std::array<std::string, kGroups> m{};
    if (s == ModelSet::Own) return m;
    for (int g = 0; g < kGroups; ++g) {
        const Family f = familyOf(Group(g));
        m[size_t(g)] = f == Family::World && s != ModelSet::Fast ? "general"
                       : s == ModelSet::Maximum || (s == ModelSet::Balanced && f == Family::Screens) ? "anime6b"
                                                                                                    : "anime";
    }
    return m;
}

ModelSet modelSetOf(const AiSettings& a) {
    for (ModelSet s : {ModelSet::Fast, ModelSet::Balanced, ModelSet::Maximum}) {
        const std::array<std::string, kGroups> m = modelsOfSet(s);
        bool same = true;
        for (int g = 0; g < kGroups; ++g) same = same && a.modelFor(Group(g)) == m[size_t(g)];
        if (same) return s;
    }
    return ModelSet::Own;
}

}  // namespace uagfx::ai
