#include "core/setting_change.hpp"

#include "core/key_binds.hpp"

#include "core/effect_groups.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::settingchange {

Config shown(const Config& c, int cardLevel) {
    Config s = c;
    s.effects = effect_groups::shown(c.effects, cardLevel);
    s.ai = c.aiNow;  // 2.2.1: the AI pictures as the file says now (shown so at once)
    return s;
}

Outcome edits(const Config& c, int cardLevel, const std::string& key, const std::string& token,
              std::vector<settings::IniEdit>& out) {
    const settings::IniSetting* s = settings::findSetting(key);
    if (!s) return Outcome::Unknown;
    if (!settings::validToken(*s, token)) return Outcome::Invalid;
    if (settings::tokenOf(*s, shown(c, cardLevel)) == token) return Outcome::Same;
    // 2.6.22: an old file's ten groups written into the effects with this first change (as the settings app does)
    if (effect_groups::hasLegacy(c.effects))
        for (const settings::IniEdit& e : effect_groups::settleLegacy(c.effects, cardLevel)) out.push_back(e);
    // 2.2: an effect's own switch or the mode while a set decides them - the set made the player's own first; 2.2.2:
    // custom chosen here as well, as on the Effects page
    const effect_groups::Member* member = effect_groups::memberOf(key);
    if (member || key == "effects.tier" || (key == "effects.preset" && token == "custom")) {
        const std::string except = member ? member->key : key.substr(8);
        for (const auto& [k, v] : effect_groups::materialize(c.effects, cardLevel, except))
            out.push_back({"effects", k, v});
    }
    out.push_back({s->section, s->key, token});
    // 3.0.24-beta.55 (item 12): a walk mode key another action has - the two swap (never one key for two actions)
    if (std::string(s->section) == "keys") {
        bool mine = false, theirs = false;
        const unsigned now = keybinds::scanOf(token);
        int me = -1;
        for (int b = 0; b < keybinds::kBinds; ++b)
            if (std::string(s->key) == keybinds::settingKey(keybinds::Bind(b))) me = b, mine = true;
        const keybinds::Bind other = keybinds::boundTo(c.keys, now, theirs);
        if (mine && theirs && int(other) != me)
            out.push_back({"keys", keybinds::settingKey(other), keybinds::tokenOf(c.keys.scan[me])});
    }
    return Outcome::Edits;
}

}  // namespace uagfx::settingchange
