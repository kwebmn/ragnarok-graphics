#include "win/setting_edit.hpp"

#include <algorithm>
#include <vector>

#include "core/effect_groups.hpp"
#include "core/setting_change.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/schema.hpp"
#include "core/theme_choice.hpp"
#include "win/context.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::settingedit {

using settings::IniEdit;

Config shown(const Config& c) { return settingchange::shown(c, Context::get().cardLevel()); }

bool direct3d12() {
    const RendererChoice& c = startup().choice;
    return c.renderer == Renderer::Uaro && c.api != Api::D3d11;
}

bool set(const std::string& iniPath, const Config& config, const std::string& key, const std::string& token) {
    std::vector<IniEdit> edits;  // 3.0.24-beta.37: its lines in core/setting_change (the MCP's «setting» too)
    if (settingchange::edits(config, Context::get().cardLevel(), key, token, edits) != settingchange::Outcome::Edits)
        return false;
    if (!editIniFile(iniPath, edits)) {  // 2.2.2: read again when the settings app wrote it meanwhile
        LOG_ERROR("settings: uaro-graphics.ini could not be written");
        return false;
    }
    LOG_INFO("settings: %s = %s", key.c_str(), token.c_str());
    return true;
}

bool setOwnAccent(const std::string& iniPath, const std::string& hex) {
    if (!editIniFile(iniPath, themechoice::ownEdits(hex))) {
        LOG_ERROR("settings: uaro-graphics.ini could not be written");
        return false;
    }
    LOG_INFO("settings: interface.accent = own, accent_colour = %s", hex.c_str());
    return true;
}

bool setLook(const std::string& iniPath, settings::presets::Choice c, int variant) {
    std::vector<IniEdit> edits;
    for (const auto& [key, token] : settings::presets::keysOf(c, variant)) {
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
