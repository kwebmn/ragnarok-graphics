#include "core/pack_build.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "core/ai/texture_cache.hpp"
#include "core/grf/grf.hpp"
#include "core/sprite_exe.hpp"
#include "core/zip.hpp"

namespace uagfx::pack {
namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

bool endsWith(const std::string& s, const char* tail) {
    const size_t n = std::char_traits<char>::length(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

// A file name the builder may take: the textures' and the sprites' (normal names).
bool wanted(const std::string& n) {
    return (n.rfind("data\\texture\\", 0) == 0 && (endsWith(n, ".bmp") || endsWith(n, ".tga"))) ||
           (n.rfind("data\\sprite\\", 0) == 0 && endsWith(n, ".spr"));
}

const std::string kInterface = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스

}  // namespace

std::vector<std::string> dataArchives(const std::string& text) {
    std::vector<std::pair<long, std::string>> found;
    bool data = false;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const std::string l = trim(text.substr(start, end - start));
        start = end + 1;
        if (l.empty() || l[0] == ';') continue;
        if (l[0] == '[') {
            data = lower(l) == "[data]";
            continue;
        }
        const size_t eq = l.find('=');
        if (!data || eq == std::string::npos) continue;
        const std::string key = trim(l.substr(0, eq)), file = trim(l.substr(eq + 1));
        char* stop = nullptr;
        const long n = std::strtol(key.c_str(), &stop, 10);
        if (!key.empty() && stop && *stop == '\0' && !file.empty()) found.emplace_back(n, file);
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::string> out;
    for (const auto& f : found) out.push_back(f.second);
    return out;
}

GameFiles::GameFiles() = default;
GameFiles::~GameFiles() = default;

bool GameFiles::open(const std::string& dir, std::string& error) {
    const char sep = dir.find('/') != std::string::npos && dir.find('\\') == std::string::npos ? '/' : '\\';
    std::string ini;
    if (std::FILE* f = std::fopen((dir + sep + "DATA.INI").c_str(), "rb")) {
        char buf[4096];
        size_t n = 0;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && ini.size() < (1u << 20)) ini.append(buf, n);
        std::fclose(f);
    }
    for (const std::string& file : dataArchives(ini)) {
        const std::string ext = lower(file.size() > 4 ? file.substr(file.size() - 4) : "");
        if (ext != ".grf" && ext != ".gpf" && ext != ".zip") continue;
        const std::string path = file.find(':') != std::string::npos ? file : dir + sep + file;
        std::string why;
        const int archive = int(order_.size());
        size_t count = 0;
        if (ext == ".zip") {
            auto z = std::make_unique<zip::Index>();
            if (!z->open(path, [](const std::string& n) { return wanted(n); }, why)) continue;
            for (const std::string& n : z->names())
                if (!where_.count(n)) {
                    where_[n] = Place{archive, n};
                    ++count;
                }
            zips_.push_back(std::move(z));
            order_.emplace_back(true, int(zips_.size()) - 1);
        } else {
            auto g = std::make_unique<grf::Archive>();
            if (!g->open(path, false, why)) continue;
            for (const std::string& stored : g->names()) {
                const std::string n = zip::normalName(stored);
                if (!wanted(n) || where_.count(n)) continue;
                where_[n] = Place{archive, stored};
                ++count;
            }
            grfs_.push_back(std::move(g));
            order_.emplace_back(false, int(grfs_.size()) - 1);
        }
        paths_.push_back(file);
        counts_.push_back(count);
    }
    if (order_.empty()) {
        error = "DATA.INI lists no archive that opens in " + dir;
        return false;
    }
    names_.reserve(where_.size());
    for (const auto& w : where_) names_.push_back(w.first);
    std::sort(names_.begin(), names_.end());
    return true;
}

bool GameFiles::read(const std::string& name, std::vector<uint8_t>& out) {
    const auto it = where_.find(name);
    if (it == where_.end()) return false;
    const std::pair<bool, int>& a = order_[size_t(it->second.archive)];
    std::string error;
    return a.first ? zips_[size_t(a.second)]->read(it->second.stored, out, error)
                   : grfs_[size_t(a.second)]->read(it->second.stored, out, error);
}

std::string GameFiles::report() const {
    std::string s;
    for (size_t i = 0; i < paths_.size(); ++i)
        s += (i ? ", " : "") + paths_[i] + " (" + std::to_string(counts_[i]) + " pictures' files)";
    return s;
}

Source sourceOf(const std::string& n, const Choice& c) {
    Source s;
    if (!wanted(n)) return s;
    if (n.rfind("data\\sprite\\", 0) == 0) {
        switch (sprite_exe::sourceOf(n.c_str())) {
            case sprite_exe::Source::Monster:
            case sprite_exe::Source::Unknown: s.type = c.monsters ? Source::Sprite : Source::None; break;
            case sprite_exe::Source::Player: s.type = c.players ? Source::Sprite : Source::None; break;
            case sprite_exe::Source::Npc: s.type = c.npcs ? Source::Sprite : Source::None; break;
            case sprite_exe::Source::Item: s.type = c.items ? Source::Sprite : Source::None; break;
            case sprite_exe::Source::Effect:
                s.type = c.effects ? Source::Sprite : Source::None;
                s.kind = ai::Kind::Effects;
                break;
            case sprite_exe::Source::Interface:
                s.type = c.interface ? Source::Sprite : Source::None;
                s.kind = ai::Kind::Icons;
                break;
        }
        if (s.type != Source::None && s.kind == ai::Kind::None) s.kind = ai::Kind::Sprites;
        if (s.type == Source::None) s.kind = ai::Kind::None;
        return s;
    }
    if (n.find("\\" + kInterface + "\\") != std::string::npos) {
        if (c.interface && endsWith(n, ".bmp")) s = Source{Source::Interface, ai::Kind::Icons};
        return s;
    }
    const bool effect = n.find("\\effect\\") != std::string::npos;
    if (effect ? !c.effects : !c.world) return s;
    s.type = endsWith(n, ".tga") ? Source::Tga : Source::Texture;
    s.kind = effect ? ai::Kind::Effects : ai::Kind::Ground;
    return s;
}

std::vector<Picture> picturesOf(const Source& s, const uint8_t* data, size_t size, const Choice& c) {
    std::vector<Picture> out;
    Picture p;
    switch (s.type) {
        case Source::Texture:
        case Source::Tga:
            if (!worldTexture(data, size, s.type == Source::Tga, p) || p.width < c.minSide || p.height < c.minSide ||
                p.width > c.maxSide || p.height > c.maxSide || ai::softPicture(p.argb.data(), p.width, p.height) ||
                ai::softAlpha(p.argb.data(), p.width, p.height))
                break;
            out.push_back(std::move(p));
            break;
        case Source::Interface: {
            if (!interfacePicture(data, size, p)) break;
            const int w = p.width, h = p.height;
            const bool icon = w == h && w >= 16 && w <= 64;
            const bool picture = w >= 32 && h >= 32 && w <= 512 && h <= 512 && std::max(w, h) <= 3 * std::min(w, h);
            if (icon || picture) out.push_back(std::move(p));
            break;
        }
        case Source::Sprite: {
            std::vector<Picture> frames;
            spriteFrames(data, size, frames);
            for (Picture& f : frames)
                if (!ai::softPicture(f.argb.data(), f.width, f.height) &&
                    !ai::softAlpha(f.argb.data(), f.width, f.height))
                    out.push_back(std::move(f));
            break;
        }
        case Source::None: break;
    }
    return out;
}

KeyCheck checkKeys(const std::vector<std::string>& cacheNames, const std::unordered_set<std::string>& made) {
    KeyCheck k;
    std::unordered_map<std::string, ai::Kind> kinds;
    std::unordered_set<std::string> pictures;
    for (const std::string& name : cacheNames) {
        const ai::ParsedEntry e = ai::parseEntry(name);
        if (e.type == ai::ParsedEntry::KindTag)
            kinds.emplace(e.key, e.kind);
        else if (e.type == ai::ParsedEntry::Picture)
            pictures.insert(e.key);
    }
    std::vector<std::string> sorted(pictures.begin(), pictures.end());
    std::sort(sorted.begin(), sorted.end());
    for (const std::string& key : sorted) {
        const auto it = kinds.find(key);
        const bool found = made.count(key) != 0;
        if (it == kinds.end() || int(it->second) >= ai::kKinds) {
            ++k.untagged;
            k.untaggedFound += found;
        } else {
            ++k.seen[int(it->second)];
            k.found[int(it->second)] += found;
        }
        if (!found && k.missing.size() < 12)
            k.missing.push_back(std::string(it == kinds.end() ? "untagged" : ai::kindName(it->second)) + " " + key);
    }
    return k;
}

std::string keyCheckText(const KeyCheck& k) {
    std::string s;
    for (int i = 0; i < ai::kKinds; ++i)
        s += std::string(i ? ", " : "") + ai::kindName(ai::Kind(i)) + " " + std::to_string(k.found[i]) + " of " +
             std::to_string(k.seen[i]);
    if (k.untagged) s += ", of no kind " + std::to_string(k.untaggedFound) + " of " + std::to_string(k.untagged);
    return s;
}

}  // namespace uagfx::pack
