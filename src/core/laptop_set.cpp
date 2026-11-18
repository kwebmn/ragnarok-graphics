#include "core/laptop_set.hpp"

#include <cstdio>
#include <sstream>

#include "core/settings/choice_presets.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::laptopset {

std::vector<std::pair<std::string, std::string>> keys() {
    namespace presets = settings::presets;
    std::vector<std::pair<std::string, std::string>> k =
        presets::keysOf(presets::Choice::Picture, int(presets::Picture::Auto));
    for (const auto& t : presets::keysOf(presets::Choice::Textures, int(presets::Textures::HdLight))) k.push_back(t);
    k.emplace_back("frames.battery_saver", "1");
    return k;
}

bool on(const Config& c) {
    for (const auto& [key, token] : keys()) {
        const settings::IniSetting* s = settings::findSetting(key);
        if (!s || settings::tokenOf(*s, c) != token) return false;
    }
    return true;
}

void Meter::frame(double seconds, bool laptopSet) {
    if (seconds <= 0.0) return;
    Sum& s = sums_[laptopSet ? 1 : 0];
    s.frames += 1.0;
    s.seconds += seconds;
    if (s.seconds > kMostMinutes * 60.0) {  // the oldest let go, the rate kept
        const double k = kMostMinutes * 60.0 / s.seconds;
        s.frames *= k;
        s.seconds *= k;
    }
    Frames& f = laptopSet ? record_.laptop : record_.other;
    f.minutes = s.seconds / 60.0;
    f.fps = s.seconds > 0.0 ? s.frames / s.seconds : 0.0;
}

void Meter::set(const Record& r) {
    record_ = r;
    const Frames* f[2] = {&r.other, &r.laptop};
    for (int i = 0; i < 2; ++i) {
        sums_[i].seconds = f[i]->minutes * 60.0;
        sums_[i].frames = f[i]->fps * sums_[i].seconds;
    }
}

std::string format(const Record& r) {
    char line[96];
    std::snprintf(line, sizeof line, "other %.1f %.1f\nlaptop %.1f %.1f\n", r.other.fps, r.other.minutes,
                  r.laptop.fps, r.laptop.minutes);
    return line;
}

bool parse(const std::string& text, Record& out) {
    Record r;
    bool any = false;
    std::istringstream in(text);
    std::string kind;
    double fps = 0, minutes = 0;
    while (in >> kind >> fps >> minutes) {
        if (fps < 0 || fps > 10000 || minutes < 0 || minutes > kMostMinutes + 1) continue;
        if (kind == "other") r.other = {fps, minutes}, any = true;
        if (kind == "laptop") r.laptop = {fps, minutes}, any = true;
    }
    if (any) out = r;
    return any;
}

std::string format(const Try& t) {
    char head[96];
    std::snprintf(head, sizeof head, "before %.1f\ntold %d\ndeclined %d\n", t.before, int(t.told), int(t.declined));
    std::string out = head;
    for (const auto& [key, token] : t.undo) out += "undo " + key + " " + token + "\n";
    return out;
}

bool parse(const std::string& text, Try& out) {
    Try t;
    bool any = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream l(line);
        std::string word;
        l >> word;
        if (word == "before") any = bool(l >> t.before);
        else if (word == "told") l >> t.told;
        else if (word == "declined") l >> t.declined;
        else if (word == "undo") {
            std::string key, token;
            l >> key;
            std::getline(l >> std::ws, token);
            if (!key.empty()) t.undo.emplace_back(key, token);
        }
    }
    if (any) out = t;
    return any;
}

Plate plate(bool laptop, bool setOn, const Record& r, const Try& t) {
    if (!laptop) return Plate::None;
    if (t.before >= 0) {  // tried: what it gave, once
        if (t.told || !setOn || r.laptop.minutes < kEnoughMinutes) return Plate::None;
        return r.laptop.fps >= t.before + kBetterBy ? Plate::Better : Plate::NoBetter;
    }
    if (setOn || t.declined || r.other.minutes < kEnoughMinutes || r.other.fps >= kSlowFps) return Plate::None;
    return Plate::Offer;
}

}  // namespace uagfx::laptopset
