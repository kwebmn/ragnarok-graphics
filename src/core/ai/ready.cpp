#include "core/ai/ready.hpp"

#include <map>
#include <utility>

#include "core/texture_pack.hpp"

namespace uagfx::ai {

ReadyPictures::ReadyPictures(const AiSettings& s) : settings_(s) {
    for (int k = 0; k < kKinds; ++k) on_[k] = aiKind(s, k);
}

void ReadyPictures::addPack(std::function<bool(const std::string&)> has, const std::string& index) {
    packHas_ = std::move(has);
    const pack::Index x = pack::parseIndexFile(index);
    for (const auto& e : x.pictures) {
        pack::Group g = pack::Group::World;
        std::string key;
        if (int(e.second.kind) < kKinds && pack::keyOfPicture(e.first, g, key) && g != pack::Group::Sprites)
            kinds_.emplace(key, e.second.kind);
    }
    for (const auto& n : x.alike) {
        pack::Likeness l;
        if (packHas_ && packHas_(n.first) && pack::parseLikeness(n.second, l)) alike_.add(n.first, l);
    }
    for (const auto& a : x.aliases) {
        if (!packHas_ || !packHas_(a.second)) continue;
        aliases_.emplace(a.first, a.second);
        const auto k = x.pictures.find(a.second);
        if (k != x.pictures.end() && int(k->second.kind) < kKinds) kinds_.emplace(a.first, k->second.kind);
    }
}

void ReadyPictures::addMade(const std::string& name) { made_.insert(name); }

void ReadyPictures::noteKind(const std::string& key, Kind kind) {
    if (int(kind) < kKinds) kinds_.emplace(key, kind);
}

bool ReadyPictures::anyCells() const {
    return on_[int(Kind::Sprites)] || on_[int(Kind::Effects)] || on_[int(Kind::Icons)];
}

bool ReadyPictures::knownKind(const std::string& key, Kind& kind) const {
    if (!settings_.ahead) return false;
    const auto it = kinds_.find(key);
    if (it == kinds_.end()) return false;
    kind = it->second;
    return true;
}

bool ReadyPictures::texture(const std::string& key, Kind kind, std::string& name) const {
    if (!kindOn(kind)) return false;
    const bool ui = kind == Kind::Icons;
    name = pack::pictureName(ui ? pack::Group::Ui : pack::Group::World, key);
    if (has(name)) return true;
    if (ui) return false;
    const auto a = aliases_.find(key);
    if (a == aliases_.end() || !has(a->second)) return false;
    name = a->second;
    return true;
}

bool ReadyPictures::alike(const uint32_t* argb, int width, int height, Kind kind, std::string& name) const {
    return alikeName(argb, width, height, name) && alikeFits(name, kind);
}

bool ReadyPictures::alikeName(const uint32_t* argb, int width, int height, std::string& name) const {
    return alike_.has(width, height) && alike_.find(argb, width, height, name);
}

bool ReadyPictures::alikeFits(const std::string& name, Kind kind) const {
    return kindOn(kind) && (kind == Kind::Icons) == (name.rfind(pack::groupName(pack::Group::Ui), 0) == 0);
}

bool ReadyPictures::cell(const std::string& key, std::string& name) const {
    if (!anyCells()) return false;
    name = pack::pictureName(pack::Group::Sprites, key);
    return has(name);
}

bool ReadyPictures::icon(const std::string& key, std::string& name) const {
    if (!on_[int(Kind::Icons)]) return false;
    name = pack::pictureName(pack::Group::Ui, key);
    return has(name);
}

}  // namespace uagfx::ai
