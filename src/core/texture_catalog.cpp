#include "core/texture_catalog.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <map>
#include <set>
#include <unordered_set>

#include "core/ai/pack_models.hpp"
#include "core/map_textures.hpp"
#include "core/pack_source.hpp"
#include "core/rsm.hpp"
#include "core/rsw.hpp"
#include "core/zip.hpp"

namespace uagfx::catalog {
namespace {

// 2: (2.5.18) an effect's TGA has a second picture, as the .str animations take it (pack::picturesOf) - a catalog of
// 1 knows none of them: the files are read again.
constexpr uint32_t kVersion = 2;

std::vector<std::string> paletteFiles(const std::string& name, const std::vector<std::string>& allNames,
                                      const pack::Choice& c) {
    return pack::palettesOf(name, allNames, c.bodyColours, c.maxHairColour, c.maxClothColour);
}

// The catalog's bytes: little-endian numbers, strings by their length.
class Out {
public:
    void u8(uint32_t v) { s_ += char(v & 0xFF); }
    void u16(uint32_t v) { u8(v), u8(v >> 8); }
    void u32(uint32_t v) { u16(v & 0xFFFF), u16(v >> 16); }
    void u64(uint64_t v) { u32(uint32_t(v)), u32(uint32_t(v >> 32)); }
    void str(const std::string& v) {
        u32(uint32_t(v.size()));
        s_ += v;
    }
    std::string& bytes() { return s_; }

private:
    std::string s_;
};

class In {
public:
    explicit In(const std::string& s) : s_(s) {}
    bool ok() const { return ok_; }
    size_t left() const { return s_.size() - at_; }
    uint32_t u8() {
        if (!ok_ || at_ >= s_.size()) return ok_ = false, 0;
        return uint8_t(s_[at_++]);
    }
    uint32_t u16() { return u8() | u8() << 8; }
    uint32_t u32() { return u16() | u16() << 16; }
    uint64_t u64() { return uint64_t(u32()) | uint64_t(u32()) << 32; }
    std::string str() {
        const uint32_t n = u32();
        if (!ok_ || n > left()) return ok_ = false, std::string();
        std::string v = s_.substr(at_, n);
        at_ += n;
        return v;
    }
    // a count of things of at least `least` bytes each: no more than the bytes left can hold
    uint32_t count(size_t least) {
        const uint32_t n = u32();
        if (!ok_ || uint64_t(n) * least > left()) return ok_ = false, 0;
        return n;
    }

private:
    const std::string& s_;
    size_t at_ = 0;
    bool ok_ = true;
};

}  // namespace

Catalog start(const std::vector<ArchiveStamp>& archives,
              const std::vector<std::pair<std::string, std::vector<uint16_t>>>& names, const pack::Choice& choice,
              const Catalog* old, std::vector<size_t>& toDecode) {
    Catalog c;
    c.archives = archives;
    c.maxHair = choice.maxHairColour;
    c.maxCloth = choice.maxClothColour;
    c.minSide = choice.minSide;
    c.maxSide = choice.maxSide;
    c.bodyColours = choice.bodyColours;
    toDecode.clear();
    std::vector<std::pair<std::string, std::vector<uint16_t>>> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::string> allNames;
    allNames.reserve(sorted.size());
    std::unordered_map<std::string, const std::vector<uint16_t>*> archivesOf;
    for (const auto& n : sorted) {
        allNames.push_back(n.first);
        archivesOf[n.first] = &n.second;
    }
    std::unordered_map<std::string, const FileEntry*> before;
    if (old && old->maxHair == c.maxHair && old->maxCloth == c.maxCloth && old->minSide == c.minSide &&
        old->maxSide == c.maxSide && old->bodyColours == c.bodyColours)
        for (const FileEntry& f : old->files) before[f.name] = &f;
    // An archive as it was: its stamp at that place before, or (a palette's) anywhere among the old ones.
    const auto same = [&](const std::vector<uint16_t>& was, const std::vector<uint16_t>& now) {
        if (was.size() != now.size()) return false;
        for (size_t i = 0; i < was.size(); ++i)
            if (was[i] >= old->archives.size() || now[i] >= archives.size() ||
                !(old->archives[was[i]] == archives[now[i]]))
                return false;
        return true;
    };
    const auto unchanged = [&](const std::string& file) {
        const auto it = archivesOf.find(file);
        if (it == archivesOf.end()) return false;
        for (uint16_t a : *it->second)
            if (a >= archives.size() ||
                std::find(old->archives.begin(), old->archives.end(), archives[a]) == old->archives.end())
                return false;
        return true;
    };
    for (const auto& n : sorted) {
        const pack::Source s = pack::sourceOf(n.first, choice);
        if (s.type == pack::Source::None) continue;
        FileEntry f;
        f.name = n.first;
        f.archives = n.second;
        f.type = uint8_t(s.type);
        f.kind = uint8_t(s.kind);
        const auto was = before.find(f.name);
        bool keep = was != before.end() && was->second->type == f.type && was->second->kind == f.kind &&
                    same(was->second->archives, f.archives);
        if (keep && s.type == pack::Source::Sprite) {
            const std::vector<std::string> colours = paletteFiles(f.name, allNames, choice);
            keep = colours == was->second->palettes && std::all_of(colours.begin(), colours.end(), unchanged);
        }
        if (keep) {
            f.pictures = was->second->pictures;
            f.palettes = was->second->palettes;
        } else {
            toDecode.push_back(c.files.size());
        }
        c.files.push_back(std::move(f));
    }
    return c;
}

bool decode(FileEntry& f, const std::vector<std::string>& allNames, const Read& read, const pack::Choice& choice,
            const std::atomic<bool>* stop) {
    f.pictures.clear();
    f.palettes.clear();
    std::vector<uint8_t> data;
    if (!read(f.name, data)) return !(stop && *stop);
    const pack::Source s{pack::Source::Type(f.type), ai::Kind(f.kind)};
    try {
        std::vector<std::pair<std::string, pack::Picture>> all;
        for (pack::Picture& p : pack::picturesOf(s, data.data(), data.size(), choice))
            all.emplace_back("", std::move(p));
        if (s.type == pack::Source::Sprite) {
            f.palettes = paletteFiles(f.name, allNames, choice);
            std::vector<uint8_t> palette;
            for (const std::string& name : f.palettes) {
                if (stop && *stop) return false;
                if (!read(name, palette) || palette.size() < 1024) continue;
                for (pack::Picture& p : pack::picturesOf(s, data.data(), data.size(), choice, palette.data()))
                    all.emplace_back(name, std::move(p));
            }
        }
        std::unordered_set<std::string> seen;
        for (auto& e : all) {
            const pack::Picture& p = e.second;
            if (!seen.insert(p.key).second) continue;
            PictureOf o;
            o.key = p.key;
            o.group = uint8_t(p.group);
            o.aiGroup = uint8_t(ai::groupOfPicture(f.name, s, p.width, p.height));
            o.width = uint16_t(p.width);
            o.height = uint16_t(p.height);
            o.palette = e.first;
            if (p.group == pack::Group::World)
                for (int div : {2, 4}) {
                    pack::Picture low;
                    if (pack::lowerDetail(p, div, low) && low.key != p.key) o.lowKeys.push_back(low.key);
                }
            f.pictures.push_back(std::move(o));
        }
    } catch (const std::exception&) {  // a file broken past what the readers check: no pictures of it
        f.pictures.clear();
    }
    return !(stop && *stop);
}

void mapsOf(Catalog& c, const std::vector<std::string>& maps, const Read& read, const std::atomic<bool>* stop) {
    c.maps = maps;
    std::unordered_map<std::string, size_t> fileAt;
    for (size_t i = 0; i < c.files.size(); ++i) {
        c.files[i].maps.clear();
        c.files[i].grid = atlas_tiles::Grid{};
        fileAt[c.files[i].name] = i;
    }
    std::map<std::string, std::vector<std::string>> models;  // a model's file -> its textures (read once)
    std::unordered_map<size_t, map_textures::GroundUse> ground;
    std::vector<uint8_t> data;
    for (size_t m = 0; m < maps.size(); ++m) {
        if (stop && *stop) return;
        std::set<std::string> textures;
        if (read("data\\" + maps[m] + ".gnd", data)) {
            for (const std::string& t : map_textures::groundTextures(data.data(), data.size()))
                textures.insert(zip::normalName("data\\texture\\" + t));
            for (const auto& u : map_textures::groundUses(data.data(), data.size())) {
                const auto f = fileAt.find(u.first);
                if (f == fileAt.end()) continue;
                map_textures::GroundUse& all = ground[f->second];
                for (const map_textures::UvRect& r : u.second.rects)
                    if (std::none_of(all.rects.begin(), all.rects.end(), [&r](const map_textures::UvRect& k) {
                            return k.u0 == r.u0 && k.v0 == r.v0 && k.u1 == r.u1 && k.v1 == r.v1;
                        }))
                        all.rects.push_back(r);
                all.pairs += u.second.pairs;
                all.joined += u.second.joined;
            }
        }
        rsw::World world;
        std::string error;
        if (read("data\\" + maps[m] + ".rsw", data) && rsw::parse(data.data(), data.size(), world, error))
            for (const rsw::Model& placed : world.models) {
                const std::string file = zip::normalName("data\\model\\" + placed.file);
                auto known = models.find(file);
                if (known == models.end()) {
                    std::vector<std::string> own;
                    rsm::Model model;
                    if (read(file, data) && rsm::parse(data.data(), data.size(), model, error))
                        for (const rsm::Part& p : model.parts)
                            if (!p.texture.empty()) own.push_back(zip::normalName("data\\texture\\" + p.texture));
                    known = models.emplace(file, std::move(own)).first;
                }
                textures.insert(known->second.begin(), known->second.end());
            }
        for (const std::string& t : textures) {
            const auto f = fileAt.find(t);
            if (f != fileAt.end()) c.files[f->second].maps.push_back(uint32_t(m));
        }
    }
    for (const auto& g : ground) c.files[g.first].grid = atlas_tiles::gridOf(g.second);
}

std::unordered_multimap<std::string, std::pair<size_t, size_t>> byKey(const Catalog& c) {
    std::unordered_multimap<std::string, std::pair<size_t, size_t>> out;
    for (size_t f = 0; f < c.files.size(); ++f)
        for (size_t p = 0; p < c.files[f].pictures.size(); ++p) {
            const PictureOf& pic = c.files[f].pictures[p];
            out.emplace(pic.key, std::make_pair(f, p));
            for (const std::string& low : pic.lowKeys) out.emplace(low, std::make_pair(f, p));
        }
    return out;
}

State stateOf(const PictureOf& p, const PackView& v) {
    if (v.index.ownAlways.count(p.key)) return State::OwnAlways;
    if (v.pictures.count(pack::pictureName(pack::Group(p.group), p.key))) return State::InPack;
    const auto alias = v.index.aliases.find(p.key);
    return alias != v.index.aliases.end() && v.pictures.count(alias->second) ? State::InPack : State::Missing;
}

std::vector<std::string> orphans(const Catalog& c, const PackView& v) {
    std::unordered_set<std::string> named;
    for (const FileEntry& f : c.files)
        for (const PictureOf& p : f.pictures) named.insert(pack::pictureName(pack::Group(p.group), p.key));
    std::vector<std::string> out;
    for (const auto& p : v.pictures)
        if (p.first != pack::kIndexName && !named.count(p.first)) out.push_back(p.first);
    std::sort(out.begin(), out.end());
    return out;
}

std::string save(const Catalog& c) {
    Out o;
    o.bytes() = "UCAT";
    o.u32(kVersion);
    o.u32(uint32_t(c.maxHair));
    o.u32(uint32_t(c.maxCloth));
    o.u32(uint32_t(c.minSide));
    o.u32(uint32_t(c.maxSide));
    o.u8(c.bodyColours);
    o.u32(uint32_t(c.archives.size()));
    for (const ArchiveStamp& a : c.archives) {
        o.str(a.path);
        o.u64(a.size);
        o.u64(uint64_t(a.time));
    }
    o.u32(uint32_t(c.maps.size()));
    for (const std::string& m : c.maps) o.str(m);
    o.u32(uint32_t(c.files.size()));
    for (const FileEntry& f : c.files) {
        o.str(f.name);
        o.u32(uint32_t(f.archives.size()));
        for (uint16_t a : f.archives) o.u16(a);
        o.u8(f.type);
        o.u8(f.kind);
        o.u8(uint32_t(f.grid.cols));
        o.u8(uint32_t(f.grid.rows));
        o.u32(uint32_t(f.maps.size()));
        for (uint32_t m : f.maps) o.u32(m);
        o.u32(uint32_t(f.palettes.size()));
        for (const std::string& p : f.palettes) o.str(p);
        o.u32(uint32_t(f.pictures.size()));
        for (const PictureOf& p : f.pictures) {
            o.str(p.key);
            o.u8(p.group);
            o.u8(p.aiGroup);
            o.u16(p.width);
            o.u16(p.height);
            o.str(p.palette);
            o.u32(uint32_t(p.lowKeys.size()));
            for (const std::string& k : p.lowKeys) o.str(k);
        }
    }
    return std::move(o.bytes());
}

bool load(const std::string& bytes, Catalog& out) {
    if (bytes.size() < 8 || bytes.compare(0, 4, "UCAT") != 0) return false;
    const std::string body = bytes.substr(4);
    In in(body);
    if (in.u32() != kVersion) return false;
    Catalog c;
    c.maxHair = int(int32_t(in.u32()));
    c.maxCloth = int(int32_t(in.u32()));
    c.minSide = int(int32_t(in.u32()));
    c.maxSide = int(int32_t(in.u32()));
    c.bodyColours = in.u8() != 0;
    c.archives.resize(in.count(20));
    for (ArchiveStamp& a : c.archives) {
        a.path = in.str();
        a.size = in.u64();
        a.time = int64_t(in.u64());
    }
    c.maps.resize(in.count(4));
    for (std::string& m : c.maps) m = in.str();
    c.files.resize(in.count(22));
    for (FileEntry& f : c.files) {
        f.name = in.str();
        f.archives.resize(in.count(2));
        for (uint16_t& a : f.archives) {
            a = uint16_t(in.u16());
            if (a >= c.archives.size()) return false;
        }
        f.type = uint8_t(in.u8());
        f.kind = uint8_t(in.u8());
        f.grid.cols = int(in.u8());
        f.grid.rows = int(in.u8());
        if (f.grid.cols < 1 || f.grid.rows < 1 || f.grid.cols > 4 || f.grid.rows > 4) return false;
        f.maps.resize(in.count(4));
        for (uint32_t& m : f.maps) {
            m = in.u32();
            if (m >= c.maps.size()) return false;
        }
        f.palettes.resize(in.count(4));
        for (std::string& p : f.palettes) p = in.str();
        f.pictures.resize(in.count(14));
        for (PictureOf& p : f.pictures) {
            p.key = in.str();
            p.group = uint8_t(in.u8());
            p.aiGroup = uint8_t(in.u8());
            p.width = uint16_t(in.u16());
            p.height = uint16_t(in.u16());
            p.palette = in.str();
            p.lowKeys.resize(in.count(4));
            for (std::string& k : p.lowKeys) k = in.str();
        }
        if (!in.ok()) return false;
    }
    if (!in.ok() || in.left() != 0) return false;
    out = std::move(c);
    return true;
}

}  // namespace uagfx::catalog
