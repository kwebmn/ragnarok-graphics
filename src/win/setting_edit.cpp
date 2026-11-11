#include "win/setting_edit.hpp"

#include <algorithm>
#include <vector>

#include "core/effect_groups.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/schema.hpp"
#include "win/context.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::settingedit {

using settings::IniEdit;

Config shown(const Config& c) {
    Config s = c;
    s.effects = effect_groups::shown(c.effects, Context::get().cardLevel());
    s.ai = c.aiNow;  // 2.2.1: the AI pictures as the file says now (shown so at once)
    return s;
}

bool set(const std::string& iniPath, const Config& config, const std::string& key, const std::string& token) {
    const settings::IniSetting* s = settings::findSetting(key);
    if (!s || !settings::validToken(*s, token) || settings::tokenOf(*s, shown(config)) == token) return false;
    std::vector<IniEdit> edits;
    // 2.6.22: an old file's ten groups written into the effects with this first change (as the settings app does)
    if (effect_groups::hasLegacy(config.effects))
        for (const IniEdit& e : effect_groups::settleLegacy(config.effects, Context::get().cardLevel()))
            edits.push_back(e);
    // 2.2: an effect's own switch or the mode while a set decides them - the set made the player's own first; 2.2.2:
    // custom chosen here as well, as on the Effects page
    const effect_groups::Member* member = effect_groups::memberOf(key);
    if (member || key == "effects.tier" || (key == "effects.preset" && token == "custom")) {
        const std::string except = member ? member->key : key.substr(8);
        for (const auto& [k, v] : effect_groups::materialize(config.effects, Context::get().cardLevel(), except))
            edits.push_back({"effects", k, v});
    }
    edits.push_back({s->section, s->key, token});
    if (!editIniFile(iniPath, edits)) {  // 2.2.2: read again when the settings app wrote it meanwhile
        LOG_ERROR("settings: uaro-graphics.ini could not be written");
        return false;
    }
    LOG_INFO("settings: %s = %s", key.c_str(), token.c_str());
    return true;
}

bool setLook(const std::string& iniPath, settings::presets::Choice c, int variant,
             const std::vector<std::string>& keep) {
    std::vector<IniEdit> edits;
    for (const auto& [key, token] : settings::presets::keysOf(c, variant)) {
        if (std::find(keep.begin(), keep.end(), key) != keep.end()) continue;
        const size_t dot = key.find('.');
        edits.push_back({key.substr(0, dot), key.substr(dot + 1), token});
    }
    if (!editIniFile(iniPath, edits)) {
        LOG_ERROR("settings: uaro-graphics.ini could not be written");
        return false;
    }
    LOG_INFO("settings: «%d» - variant %d", int(c), variant);
    return true;
}

}  // namespace uagfx::win::settingedit
