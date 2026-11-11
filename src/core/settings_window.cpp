#include "core/settings_window.hpp"

#include "core/settings/choice_presets.hpp"
#include "core/settings/navigation.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/search.hpp"

namespace uagfx::settingswin {
namespace {

const settings::Text kGameNames[6] = {settings::Text::OptionsMusic,      settings::Text::OptionsSounds,
                                      settings::Text::OptionsAttackSnap, settings::Text::OptionsSkillSnap,
                                      settings::Text::OptionsItemSnap,   settings::Text::OptionsOpaque};

}  // namespace

const std::vector<Section>& sections() {
    static const std::vector<Section> list = [] {
        std::vector<Section> out;
        out.push_back(Section{settings::Text::OptionsTitle, Kind::Game,
                              {"game.0", "game.1", "game.2", "game.3", "game.4", "game.5"}});
        out.push_back(Section{settings::Text::LookHead, Kind::Picture, {"picture.0", "picture.1", "picture.2"}});
        for (const settings::InterfaceSection& s : settings::interfaceSections()) {
            Section sec{s.heading, Kind::Keys, {}};
            for (const char* k : s.keys)
                if (settings::findSetting(k)) sec.keys.push_back(k);
            if (!sec.keys.empty()) out.push_back(std::move(sec));
        }
        return out;
    }();
    return list;
}

std::vector<Found> find(const std::string& query, settings::Lang l) {
    std::vector<Found> out;
    if (query.find_first_not_of(' ') == std::string::npos) return out;
    const std::vector<Section>& list = sections();
    for (size_t i = 0; i < list.size(); ++i)
        for (const std::string& key : list[i].keys) {
            bool hit = false;
            if (list[i].kind == Kind::Game) {
                hit = settings::queryMatches(settings::tr(l, kGameNames[key.back() - '0']), query);
            } else if (list[i].kind == Kind::Picture) {
                const auto c = settings::presets::Choice(key.back() - '0');
                hit = settings::queryMatches(settings::tr(l, settings::presets::headOf(c)), query);
            } else if (const settings::IniSetting* s = settings::findSetting(key)) {
                hit = settings::settingMatches(*s, query, l);
            }
            if (hit) out.push_back(Found{int(i), key});
        }
    return out;
}

}  // namespace uagfx::settingswin
