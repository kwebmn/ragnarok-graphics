#include "core/pack_build.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "core/ai/texture_cache.hpp"
#include "core/grf/grf.hpp"
#include "core/jpeg_image.hpp"
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
    return (n.rfind("data\\texture\\", 0) == 0 &&
            (endsWith(n, ".bmp") || endsWith(n, ".tga") || endsWith(n, ".jpg"))) ||
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
        if (c.interface && endsWith(n, ".jpg")) s = Source{Source::Jpeg, ai::Kind::Icons};  // 0.49: loading screens
        return s;
    }
    const bool effect = n.find("\\effect\\") != std::string::npos;
    if (effect ? !c.effects : !c.world) return s;
    if (endsWith(n, ".jpg")) return Source{Source::Jpeg, effect ? ai::Kind::Effects : ai::Kind::Objects};  // water
    s.type = endsWith(n, ".tga") ? Source::Tga : Source::Texture;
    s.kind = effect ? ai::Kind::Effects : ai::Kind::Ground;
    return s;
}

std::vector<std::string> palettesOf(const std::string& sprite, const std::vector<std::string>& names) {
    const std::string human = "\xC0\xCE\xB0\xA3\xC1\xB7", head = "\xB8\xD3\xB8\xAE\xC5\xEB",
                      hair = "\xB8\xD3\xB8\xAE";  // 인간족, 머리통, 머리
    const std::string dir = "data\\sprite\\" + human + "\\" + head + "\\";
    std::vector<std::string> out;
    if (sprite.rfind(dir, 0) != 0 || !endsWith(sprite, ".spr")) return out;
    const std::string rest = sprite.substr(dir.size());  // "<sex>\<N>_<sex>.spr"
    const size_t slash = rest.find('\\');
    if (slash == std::string::npos) return out;
    const std::string sex = rest.substr(0, slash), file = rest.substr(slash + 1);
    const std::string tail = "_" + sex + ".spr";
    if (file.size() <= tail.size() || file.compare(file.size() - tail.size(), tail.size(), tail) != 0) return out;
    const std::string style = file.substr(0, file.size() - tail.size());
    const std::string prefix = "data\\palette\\" + hair + "\\" + hair + style + "_" + sex + "_";
    for (auto it = std::lower_bound(names.begin(), names.end(), prefix); it != names.end(); ++it) {
        if (it->compare(0, prefix.size(), prefix) != 0) break;
        const std::string colour = it->substr(prefix.size());
        if (colour.size() <= 4 || !endsWith(colour, ".pal")) continue;
        bool digits = true;
        for (size_t i = 0; i + 4 < colour.size(); ++i) digits = digits && colour[i] >= '0' && colour[i] <= '9';
        if (digits) out.push_back(*it);
    }
    return out;
}

std::vector<Picture> picturesOf(const Source& s, const uint8_t* data, size_t size, const Choice& c,
                                const uint8_t* palette) {
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
            // 0.45: a big one the game draws as a Direct3D texture (a minimap): its 16-bit pixels too - the driver's
            // big pictures of the interface (128 to 1024 a side, whole: a power of two), soft light left out.
            Picture t;
            const bool pow2 = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;
            if (w >= 128 && h >= 128 && w <= 1024 && h <= 1024 && pow2 && worldTexture(data, size, false, t) &&
                !ai::softPicture(t.argb.data(), t.width, t.height) &&
                !ai::softAlpha(t.argb.data(), t.width, t.height) && (out.empty() || out.back().key != t.key)) {
                t.group = Group::Ui;
                out.push_back(std::move(t));
            }
            break;
        }
        case Source::Jpeg: {  // 0.49
            std::vector<uint32_t> pixels;
            int w = 0, h = 0;
            if (!jpeg::decode(data, size, pixels, w, h)) break;
            int tw = w, th = h;
            if (s.kind == ai::Kind::Icons) {  // the game's texture: a power of two a side, the picture in its corner
                tw = th = 1;
                while (tw < w) tw *= 2;
                while (th < h) th *= 2;
                if (tw < 128 || th < 128 || tw > 1024 || th > 1024) break;
            } else if ((w & (w - 1)) || (h & (h - 1)) || w < c.minSide || h < c.minSide || w > c.maxSide ||
                       h > c.maxSide) {
                break;
            }
            Picture p;
            p.group = s.kind == ai::Kind::Icons ? Group::Ui : Group::World;
            p.width = tw;
            p.height = th;
            p.argb.assign(size_t(tw) * size_t(th), 0);
            for (int y = 0; y < h; ++y)
                std::copy(pixels.begin() + std::ptrdiff_t(y) * w, pixels.begin() + std::ptrdiff_t(y + 1) * w,
                          p.argb.begin() + std::ptrdiff_t(y) * tw);
            p.contentWidth = w;
            p.contentHeight = h;
            p.key = ai::keyName(ai::textureKey(p.argb.data(), tw, th));
            out.push_back(std::move(p));
            break;
        }
        case Source::Sprite: {
            std::vector<Picture> frames;
            spriteFrames(data, size, frames, palette);
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

namespace {

const char* const kChoiceWords[7] = {"world", "effects", "monsters", "players", "npcs", "items", "interface"};
bool* choiceField(Choice& c, int i) {
    bool* const fields[7] = {&c.world, &c.effects, &c.monsters, &c.players, &c.npcs, &c.items, &c.interface};
    return fields[i];
}
const char* const kStates[6] = {"starting", "reading", "building", "done", "stopped", "failed"};

std::string oneLine(const std::string& s) {
    std::string out = s;
    for (char& ch : out)
        if (ch == '\n' || ch == '\r') ch = ' ';
    return out;
}

}  // namespace

std::string choiceText(const Choice& c) {
    Choice copy = c;
    std::string s;
    for (int i = 0; i < 7; ++i)
        if (*choiceField(copy, i)) s += std::string(s.empty() ? "" : ",") + kChoiceWords[i];
    return s;
}

Choice parseChoice(const std::string& words) {
    Choice c;
    for (int i = 0; i < 7; ++i) *choiceField(c, i) = false;
    size_t at = 0;
    while (at <= words.size()) {
        size_t end = words.find(',', at);
        if (end == std::string::npos) end = words.size();
        const std::string w = words.substr(at, end - at);
        for (int i = 0; i < 7; ++i)
            if (w == kChoiceWords[i]) *choiceField(c, i) = true;
        at = end + 1;
    }
    return c;
}

bool anyChosen(const Choice& c) {
    return c.world || c.effects || c.monsters || c.players || c.npcs || c.items || c.interface;
}

std::string statusText(const Status& s) {
    char rate[32];
    std::snprintf(rate, sizeof rate, "%.2f", s.rate);
    std::string t = std::string("state=") + kStates[std::min<int>(s.state, 5)] + "\n";
    t += "files=" + std::to_string(s.filesDone) + "/" + std::to_string(s.filesTotal) + "\n";
    t += "pictures=" + std::to_string(s.made) + "/" + std::to_string(s.toMake) + "\n";
    t += "in_pack=" + std::to_string(s.inPack) + "\n";
    t += "bytes=" + std::to_string(s.bytes) + "\n";
    t += std::string("rate=") + rate + "\n";
    t += "eta=" + std::to_string(s.eta) + "\n";
    t += "pid=" + std::to_string(s.pid) + "\n";
    t += "current=" + oneLine(s.current) + "\n";
    t += "message=" + oneLine(s.message) + "\n";
    return t;
}

bool parseStatus(const std::string& text, Status& out) {
    Status s;
    bool state = false;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        const auto pair = [&](uint32_t& a, uint32_t& b) {
            const size_t slash = v.find('/');
            a = uint32_t(std::strtoul(v.c_str(), nullptr, 10));
            b = slash == std::string::npos ? 0 : uint32_t(std::strtoul(v.c_str() + slash + 1, nullptr, 10));
        };
        if (k == "state") {
            for (int i = 0; i < 6; ++i)
                if (v == kStates[i]) {
                    s.state = Status::State(i);
                    state = true;
                }
        } else if (k == "files") {
            pair(s.filesDone, s.filesTotal);
        } else if (k == "pictures") {
            pair(s.made, s.toMake);
        } else if (k == "in_pack") {
            s.inPack = uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        } else if (k == "bytes") {
            s.bytes = std::strtoull(v.c_str(), nullptr, 10);
        } else if (k == "rate") {
            s.rate = std::strtod(v.c_str(), nullptr);
        } else if (k == "eta") {
            s.eta = int(std::strtol(v.c_str(), nullptr, 10));
        } else if (k == "pid") {
            s.pid = uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        } else if (k == "current") {
            s.current = v;
        } else if (k == "message") {
            s.message = v;
        }
    }
    if (state) out = std::move(s);
    return state;
}

}  // namespace uagfx::pack
