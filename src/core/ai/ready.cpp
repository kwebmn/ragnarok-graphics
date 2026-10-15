#include "core/ai/ready.hpp"

#include <algorithm>
#include <map>
#include <utility>

#include "core/ai/texture_cache.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::ai {

ReadyPictures::ReadyPictures(const AiSettings& s) : settings_(s) {
    const int scale = s.scale == 2 ? 2 : 4;
    for (int k = 0; k < kKinds; ++k) {
        on_[k] = aiKind(s, k);
        const std::string& token = k < 2 ? s.model : s.spriteModel;  // the ground and objects, the rest
        folders_[k] = netFolder(token, token == "general" ? std::clamp(s.denoise, 0, 100) : 100, scale);
    }
}

void ReadyPictures::addPack(std::function<bool(const std::string&)> has, const std::string& index) {
    packHas_ = std::move(has);
    std::map<std::string, Kind> pictureKinds;  // a world or interface picture's name -> its kind
    for (const auto& e : pack::parseIndex(index)) {
        pack::Group g = pack::Group::World;
        std::string key;
        if (int(e.second) >= kKinds || !pack::keyOfPicture(e.first, g, key) || g == pack::Group::Sprites) continue;
        pictureKinds[e.first] = e.second;
        kinds_.emplace(key, e.second);
    }
    for (const auto& a : pack::parseAliases(index)) {
        if (!packHas_ || !packHas_(a.second)) continue;
        aliases_.emplace(a.first, a.second);
        const auto k = pictureKinds.find(a.second);
        if (k != pictureKinds.end()) kinds_.emplace(a.first, k->second);
    }
}

void ReadyPictures::addGrf(const std::vector<std::string>& names) {
    for (const std::string& name : names) {
        const ParsedEntry e = parseEntry(name);
        if (e.type == ParsedEntry::KindTag) {
            kinds_.emplace(e.key, e.kind);
        } else if (e.type == ParsedEntry::Picture) {
            bool ours = false;
            for (int k = 0; k < kKinds && !ours; ++k)
                ours = on_[k] && (e.folder == folders_[k] || e.folder == kindFolder(folders_[k], Kind::Icons));
            if (ours)
                grf_.insert(name);
            else
                ++other_;
        }
    }
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

bool ReadyPictures::find(const std::string& packed, const std::string& grf, Source& out) const {
    if (packHas_ && packHas_(packed)) {
        out = Source{packed, true};
        return true;
    }
    if (grf_.count(grf)) {
        out = Source{grf, false};
        return true;
    }
    return false;
}

bool ReadyPictures::texture(const std::string& key, Kind kind, Source& out) const {
    if (!kindOn(kind)) return false;
    const bool ui = kind == Kind::Icons;
    std::string packed = pack::pictureName(ui ? pack::Group::Ui : pack::Group::World, key);
    if (!ui && !(packHas_ && packHas_(packed))) {
        const auto a = aliases_.find(key);
        if (a != aliases_.end()) packed = a->second;
    }
    return find(packed, entryName(kindFolder(folders_[int(kind)], kind), key), out);
}

bool ReadyPictures::cell(const std::string& key, Source& out) const {
    int folder = -1;  // the sprites' network's: the first of their kinds that is on
    for (Kind k : {Kind::Sprites, Kind::Effects, Kind::Icons})
        if (folder < 0 && on_[int(k)]) folder = int(k);
    if (folder < 0) return false;
    return find(pack::pictureName(pack::Group::Sprites, key), entryName(folders_[folder], key), out);
}

bool ReadyPictures::icon(const std::string& key, Source& out) const {
    if (!on_[int(Kind::Icons)]) return false;
    return find(pack::pictureName(pack::Group::Ui, key),
                entryName(kindFolder(folders_[int(Kind::Icons)], Kind::Icons), key), out);
}

}  // namespace uagfx::ai
