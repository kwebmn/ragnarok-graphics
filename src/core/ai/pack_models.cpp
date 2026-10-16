#include "core/ai/pack_models.hpp"

#include <cstdlib>

#include "core/sprite_exe.hpp"

namespace uagfx::ai {
namespace {

constexpr int kScreenSide = 128;  // as the driver's kUiPictureSide: the interface's big pictures

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

Group groupOfPicture(const std::string& fileName, const pack::Source& s, int width, int height) {
    switch (s.type) {
        case pack::Source::Sprite: return groupOfSource(uint8_t(sprite_exe::sourceOf(fileName.c_str())));
        case pack::Source::Interface:
            return width >= kScreenSide && height >= kScreenSide ? Group::Screens : Group::Icons;
        case pack::Source::Jpeg: return s.kind == Kind::Icons ? Group::Screens : groupOfKind(s.kind);
        default: return s.kind == Kind::Effects ? Group::Effects : Group::World;
    }
}

std::optional<Group> groupOfEntry(const std::string& name, const pack::IndexEntry& e) {
    Group g;
    if (!e.group.empty() && parseGroup(e.group, g)) return g;
    if (e.kind == Kind::Effects) return Group::Effects;
    if (name.rfind("world/", 0) == 0) return Group::World;
    if (name.rfind("ui/", 0) == 0) {
        int w = 0, h = 0;
        return sizeOfName(name, w, h) && w >= kScreenSide && h >= kScreenSide ? Group::Screens : Group::Icons;
    }
    if (e.kind == Kind::Icons) return Group::Icons;  // the cursor's frames
    return std::nullopt;
}

std::string modelOfEntry(const pack::IndexEntry& e, Group g, const AiSettings& a) {
    if (!e.model.empty()) return e.model;
    return g == Group::World ? a.model : a.spriteModel;
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
        if (model != a.modelFor(*g)) ++gc.other;
    }
    return c;
}

bool groupOn(const AiSettings& s, Group g) {
    switch (g) {
        case Group::World: return s.ground || s.objects;
        case Group::Monsters: return s.monsters;
        case Group::Players: return s.players;
        case Group::Npcs: return s.npcs;
        case Group::Items: return s.items;
        case Group::Effects: return s.effects;
        case Group::Icons:
        case Group::Screens: return s.icons;
    }
    return false;
}

std::array<std::string, kGroups> modelsOfSet(ModelSet s) {
    std::array<std::string, kGroups> m{};
    if (s == ModelSet::Own) return m;
    for (int g = 0; g < kGroups; ++g) {
        const Group group = Group(g);
        m[size_t(g)] = group == Group::World && s != ModelSet::Fast ? "general"
                       : s == ModelSet::Maximum || (s == ModelSet::Balanced && group == Group::Screens) ? "anime6b"
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
