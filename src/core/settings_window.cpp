#include "core/settings_window.hpp"

#include "core/effect_groups.hpp"
#include "core/settings/catalogue.hpp"
#include "core/settings/choice_presets.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/search.hpp"

namespace uagfx::settingswin {
namespace {

namespace cat = settings::catalogue;

const settings::Text kGameNames[6] = {settings::Text::OptionsMusic,      settings::Text::OptionsSounds,
                                      settings::Text::OptionsAttackSnap, settings::Text::OptionsSkillSnap,
                                      settings::Text::OptionsItemSnap,   settings::Text::OptionsOpaque};

bool inGame(const cat::Row& r) { return cat::homeOf(r) == cat::Home::Game && settings::findSetting(r.key); }

Line row(Kind kind, const std::string& key, int group, int entry, settings::Text name) {
    Line l;
    l.kind = kind;
    l.key = key;
    l.group = group;
    l.entry = entry;
    l.groupName = name;
    return l;
}

// The first entry's rows: the three choices, then the game's own six
std::vector<Line> firstLines() {
    std::vector<Line> out;
    for (int i = 0; i < settings::presets::kChoices; ++i)
        out.push_back(row(Kind::Picture, "picture." + std::to_string(i), kLookGroup, 0, settings::Text::LookHead));
    for (int i = 0; i < 6; ++i)
        out.push_back(row(Kind::Game, "game." + std::to_string(i), kGameGroup, 0, settings::Text::OptionsTitle));
    return out;
}

void head(std::vector<Line>& lines) {  // a group's name over its first line
    for (size_t i = 0; i < lines.size(); ++i) lines[i].heads = i == 0 || lines[i].group != lines[i - 1].group;
}

int entryOf(cat::Area area) {
    const std::vector<Nav>& list = nav();
    for (size_t i = 1; i < list.size(); ++i)
        if (list[i].area == int(area)) return int(i);
    return -1;
}

bool setDecides(const Config& shown) { return shown.effects.preset != EffectPreset::Custom; }

}  // namespace

const std::vector<Nav>& nav() {
    static const std::vector<Nav> list = [] {
        std::vector<Nav> out;
        out.push_back(Nav{settings::Text::LookHead, -1, settings::presets::kChoices + 6});
        for (int a = 0; a < cat::kAreaCount; ++a) {
            int rows = 0;
            for (const cat::Group& g : cat::groups())
                if (g.area == cat::Area(a))
                    for (const cat::Row& r : g.rows) rows += inGame(r) ? 1 : 0;
            if (rows > 0) out.push_back(Nav{cat::areaName(cat::Area(a)), a, rows});
        }
        out.push_back(Nav{settings::Text::SettingsChanged, kChangedArea, 0});  // 3.0.21-beta.10
        return out;
    }();
    return list;
}

std::vector<Line> lines(int entry, const std::set<int>& open) {
    std::vector<Line> out;
    const std::vector<Nav>& list = nav();
    if (entry < 0 || size_t(entry) >= list.size()) return out;
    if (list[size_t(entry)].area == kChangedArea) return out;  // changedLines(): they go by the settings
    if (list[size_t(entry)].area < 0) {
        out = firstLines();
        head(out);
        return out;
    }
    const std::vector<cat::Group>& groups = cat::groups();
    for (size_t g = 0; g < groups.size(); ++g) {
        if (int(groups[g].area) != list[size_t(entry)].area) continue;
        int details = 0;
        for (const cat::Row& r : groups[g].rows) details += inGame(r) && r.level != cat::Level::Basic ? 1 : 0;
        const bool opened = open.count(int(g)) != 0;
        for (const cat::Row& r : groups[g].rows) {
            if (!inGame(r) || (r.level != cat::Level::Basic && !opened)) continue;
            Line l = row(Kind::Keys, r.key, int(g), entry, groups[g].name);
            l.part = !r.under.empty();
            out.push_back(std::move(l));
        }
        if (details > 0) {
            Line more = row(Kind::Keys, "", int(g), entry, groups[g].name);
            more.what = Line::What::More;
            more.more = opened ? 0 : details;
            out.push_back(std::move(more));
        }
    }
    head(out);
    return out;
}

std::vector<Line> find(const std::string& query, settings::Lang l) {
    std::vector<Line> out;
    if (query.find_first_not_of(' ') == std::string::npos) return out;
    for (Line& line : firstLines()) {
        const int i = line.key.back() - '0';
        const settings::Text name = line.kind == Kind::Game
                                        ? kGameNames[i]
                                        : settings::presets::headOf(settings::presets::Choice(i));
        if (settings::queryMatches(settings::tr(l, name), query)) out.push_back(std::move(line));
    }
    const std::vector<cat::Group>& groups = cat::groups();
    for (size_t g = 0; g < groups.size(); ++g) {
        const int entry = entryOf(groups[g].area);
        if (entry < 0) continue;
        for (const cat::Row& r : groups[g].rows) {
            const settings::IniSetting* s = inGame(r) ? settings::findSetting(r.key) : nullptr;
            if (!s || !settings::settingMatches(*s, query, l)) continue;
            Line found = row(Kind::Keys, r.key, int(g), entry, groups[g].name);
            found.part = !r.under.empty();
            out.push_back(std::move(found));
        }
    }
    head(out);
    return out;
}

Config defaults(int cardLevel) {
    Config d = parseConfig("[effects]\npreset = auto\n");
    d.effects = effect_groups::shown(d.effects, cardLevel);
    return d;
}

bool changed(const std::string& key, const Config& shown, const Config& defaults) {
    const settings::IniSetting* s = settings::findSetting(key);
    if (!s) return false;
    if (effect_groups::memberOf(key) && setDecides(shown)) return false;
    return settings::tokenOf(*s, shown) != settings::tokenOf(*s, defaults);
}

std::vector<Reset> resetsOf(int group, const Config& shown, const Config& defaults) {
    std::vector<Reset> out;
    const std::vector<cat::Group>& groups = cat::groups();
    if (group < 0 || size_t(group) >= groups.size()) return out;
    for (const cat::Row& r : groups[size_t(group)].rows) {
        if (!inGame(r) || !changed(r.key, shown, defaults)) continue;
        out.push_back(Reset{r.key, settings::tokenOf(*settings::findSetting(r.key), defaults)});
    }
    return out;
}

std::vector<Line> changedLines(const Config& shown, const Config& defaults) {
    std::vector<Line> out;
    const std::vector<cat::Group>& groups = cat::groups();
    for (size_t g = 0; g < groups.size(); ++g) {
        const int entry = entryOf(groups[g].area);
        if (entry < 0) continue;
        for (const cat::Row& r : groups[g].rows) {
            if (!inGame(r) || !changed(r.key, shown, defaults)) continue;
            Line off = row(Kind::Keys, r.key, int(g), entry, groups[g].name);
            off.part = !r.under.empty();
            out.push_back(std::move(off));
        }
    }
    head(out);
    return out;
}

settings::Text noteOf(int entry, bool view3d) {
    const std::vector<Nav>& list = nav();
    if (entry < 0 || size_t(entry) >= list.size()) return settings::Text::Count;
    if (list[size_t(entry)].area == int(cat::Area::View3d) && !view3d) return settings::Text::SettingsNote3d;
    return settings::Text::Count;
}

}  // namespace uagfx::settingswin
