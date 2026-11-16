#include "core/console_tiles.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "core/settings/navigation.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::consoletiles {

const std::vector<const char*>& quickKeys() {
    static const std::vector<const char*> keys{"effects.group_time", "effects.group_weather", "effects.grass",
                                               "effects.torch_shadows", "effects.mage_effects", "frames.keep_fps",
                                               "frames.fps_counter", "interface.net_badge", "interface.mob_hp",
                                               "view3d.own_models",
                                               "view3d.shadows",        // 3.0.20-beta.50: while the trial is tried
                                               "view3d.dense_crowns",   // 3.0.20-beta.51
                                               "view3d.vault_rock"};    // 3.0.20-beta.54: to tell in a dungeon
    return keys;
}

std::vector<std::string> sectionRows(const char* section) {
    std::vector<std::string> out;
    for (const settings::IniSetting& s : settings::iniSettings())
        if (std::strcmp(s.section, section) == 0 && settings::levelOf(s) != settings::Mode::Developer)
            out.push_back(settings::sectionKey(s));
    return out;
}


std::vector<std::vector<std::string>> rows(const std::vector<std::string>& keys) {
    std::vector<std::vector<std::string>> out;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i % kPerRow == 0) out.emplace_back();
        out.back().push_back(keys[i]);
    }
    return out;
}

std::string join(const std::vector<std::string>& keys) {
    std::string s;
    for (const std::string& k : keys) s += (s.empty() ? "" : "|") + k;
    return s;
}

std::vector<std::string> split(const std::string& joined) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at <= joined.size() && !joined.empty()) {
        const size_t bar = joined.find('|', at);
        out.push_back(joined.substr(at, bar == std::string::npos ? std::string::npos : bar - at));
        if (bar == std::string::npos) break;
        at = bar + 1;
    }
    return out;
}

int move(int col, int step, int count) {
    if (count <= 0) return 0;
    return std::clamp(col + (step < 0 ? -1 : step > 0 ? 1 : 0), 0, count - 1);
}

int at(int x, int left, int width, int count) {
    if (count <= 0 || width <= 0) return 0;
    return std::clamp((x - left) * kPerRow / width, 0, count - 1);
}

std::string gist(const std::string& hint, size_t most) {
    size_t end = hint.size();
    for (size_t i = 0; i < hint.size(); ++i) {
        if (hint[i] == '\n') {
            end = i;
            break;
        }
        if (hint[i] == '.' && (i + 1 == hint.size() || hint[i + 1] == ' ')) {
            end = i + 1;
            break;
        }
    }
    std::string s = hint.substr(0, end);
    // letters, not bytes: a UTF-8 letter's continuation bytes are 10xxxxxx
    size_t letters = 0, cut = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i)
        if ((uint8_t(s[i]) & 0xC0) != 0x80 && ++letters > most) {
            cut = i;
            break;
        }
    if (cut == std::string::npos) return s;
    const size_t space = s.rfind(' ', cut);
    s = s.substr(0, space == std::string::npos || space == 0 ? cut : space);
    while (!s.empty() && (s.back() == ' ' || s.back() == ',')) s.pop_back();
    return s + "\xE2\x80\xA6";
}

}  // namespace uagfx::consoletiles
