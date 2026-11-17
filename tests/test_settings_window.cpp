#include <algorithm>
#include <set>

#include "core/live_config.hpp"
#include "core/settings/catalogue.hpp"
#include "core/settings/schema.hpp"
#include "core/settings_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool holds(const std::vector<settingswin::Line>& lines, const char* key) {
    return std::any_of(lines.begin(), lines.end(), [&](const settingswin::Line& l) { return l.key == key; });
}

int entryNamed(settings::Text name) {
    const std::vector<settingswin::Nav>& nav = settingswin::nav();
    for (size_t i = 0; i < nav.size(); ++i)
        if (nav[i].name == name) return int(i);
    return -1;
}

}  // namespace

// 3.0.21-beta.3 (the concept «Настройки uaRO Graphics», the owner: «Вариант 4. Каждому месту своё дело», step 3):
// «Налаштування» (Alt+O) - the player's one place in the game. The list on the left: «Як виглядає гра» first, then the
// catalogue's areas that hold rows living in the game; nothing that waits for the game's start, no trial, no
// diagnostic
TEST(settings_window_lists_the_areas_that_live_in_the_game) {
    const std::vector<settingswin::Nav>& nav = settingswin::nav();
    CHECK_EQ(int(nav.size()), 8);
    if (nav.size() != 8) return;
    CHECK(nav[0].name == settings::Text::LookHead && nav[0].area == -1 && nav[0].rows == 9);
    const settings::Text names[7] = {settings::Text::LookPicture, settings::Text::View3dHeading,
                                     settings::Text::ViewInterface, settings::Text::CatControls,
                                     settings::Text::CatSound,     settings::Text::CatSpeed,
                                     settings::Text::CatGame};
    for (int i = 0; i < 7; ++i) CHECK(nav[size_t(i + 1)].name == names[i]);
    // «Текстури» and «Діагностика» hold nothing that lives in the game: not listed
    CHECK(entryNamed(settings::Text::LookTextures) < 0);
    CHECK(entryNamed(settings::Text::PageDiagnostics) < 0);
    // an entry's count: its rows living in the game - every one of them once over the list
    int rows = 0, inGame = 0;
    for (size_t i = 1; i < nav.size(); ++i) rows += nav[i].rows;
    for (const settings::catalogue::Group& g : settings::catalogue::groups())
        for (const settings::catalogue::Row& r : g.rows)
            inGame += settings::catalogue::homeOf(r) == settings::catalogue::Home::Game ? 1 : 0;
    CHECK_EQ(rows, inGame);
    CHECK(rows > 400);
}

// The first entry: the simple start's three choices, then the game's own six - what the game's Alt+O had
TEST(settings_window_first_entry_keeps_the_game_s_own) {
    const std::vector<settingswin::Line> first = settingswin::lines(0, {});
    CHECK_EQ(int(first.size()), 9);
    if (first.size() != 9) return;
    CHECK(first[0].kind == settingswin::Kind::Picture && first[0].key == "picture.0" && first[0].heads);
    CHECK(first[0].groupName == settings::Text::LookHead && !first[1].heads && !first[2].heads);
    CHECK(first[3].kind == settingswin::Kind::Game && first[3].key == "game.0" && first[3].heads);
    CHECK(first[3].groupName == settings::Text::OptionsTitle && first[8].key == "game.5");
    for (const settingswin::Line& l : first) CHECK(l.what == settingswin::Line::What::Row);
    CHECK(settingswin::lines(-1, {}).empty() && settingswin::lines(99, {}).empty());
}

// The pane: an area's groups one under another - the group's name over its first line, its basic rows, then a line
// that opens its details
TEST(settings_window_a_group_shows_its_basic_rows_then_more) {
    const int sound = entryNamed(settings::Text::CatSound);
    CHECK(sound > 0);
    if (sound <= 0) return;
    const std::vector<settingswin::Line> folded = settingswin::lines(sound, {});
    // «Звук»: the output, the world's sounds - and not the engine (it waits for the game's start)
    CHECK(holds(folded, "sound.output") && holds(folded, "sound.ambience"));
    CHECK(!holds(folded, "sound.engine") && !holds(folded, "sound.rate"));
    CHECK(!holds(folded, "sound.ambience_volume"));  // a detail: behind «Ще»
    // «Сповіщення»: its seven basic rows, then «Ще 14»
    CHECK(holds(folded, "notify.on") && holds(folded, "notify.whisper") && holds(folded, "notify.sound"));
    CHECK(!holds(folded, "notify.trade") && !holds(folded, "notify.buff_seconds"));
    const settings::catalogue::Group* notify = settings::catalogue::groupOf("notify.on");
    int notifyGroup = -1, basics = 0, heads = 0;
    const settingswin::Line* more = nullptr;
    for (const settingswin::Line& l : folded) {
        if (l.key == "notify.on") notifyGroup = l.group;
        heads += l.heads ? 1 : 0;
    }
    for (const settingswin::Line& l : folded) {
        if (l.group != notifyGroup) continue;
        CHECK(notify && l.groupName == notify->name);
        if (l.what == settingswin::Line::What::More) more = &l;
        else ++basics;
    }
    CHECK_EQ(basics, 7);
    CHECK(more && more->more == 14);
    CHECK_EQ(heads, 2);  // two groups: «Звук», «Сповіщення»
    CHECK(folded.back().what == settingswin::Line::What::More);  // the line after a group's rows
    // opened: every row of the group in the catalogue's order - a part right under its row -, then the folding line
    const std::vector<settingswin::Line> opened = settingswin::lines(sound, {notifyGroup});
    CHECK(holds(opened, "notify.trade") && holds(opened, "notify.report_answer"));
    CHECK(!holds(opened, "sound.ambience_volume"));  // the other group stays folded
    size_t ending = 0, seconds = 0;
    for (size_t i = 0; i < opened.size(); ++i) {
        if (opened[i].key == "notify.buff_ending") ending = i;
        if (opened[i].key == "notify.buff_seconds") seconds = i;
    }
    CHECK(ending > 0 && seconds == ending + 1 && opened[seconds].part && !opened[ending].part);
    int opens = 0;
    for (const settingswin::Line& l : opened)
        if (l.group == notifyGroup && l.what == settingswin::Line::What::More) opens += l.more == 0 ? 1 : 100;
    CHECK_EQ(opens, 1);  // one line, folding
    CHECK_EQ(int(opened.size()), int(folded.size()) + 14);
}

// Everything it lists applies at once; the trials and the diagnostics are the console's, the hidden effects the file's
TEST(settings_window_lists_only_what_applies_at_once) {
    const std::vector<settingswin::Nav>& nav = settingswin::nav();
    std::set<int> all;
    for (int g = 0; g < int(settings::catalogue::groups().size()); ++g) all.insert(g);
    std::set<std::string> seen;
    for (size_t e = 1; e < nav.size(); ++e) {
        int rows = 0;
        for (const settingswin::Line& l : settingswin::lines(int(e), all)) {
            if (l.what != settingswin::Line::What::Row) continue;
            ++rows;
            CHECK(l.kind == settingswin::Kind::Keys && settings::findSetting(l.key) != nullptr);
            CHECK(appliesNow(l.key));
            CHECK(seen.insert(l.key).second);  // once over the whole list
            const settings::catalogue::Row* r = settings::catalogue::find(l.key);
            CHECK(r && r->status == settings::catalogue::Status::Permanent);
        }
        CHECK_EQ(rows, nav[e].rows);  // the count beside the entry's name
    }
    CHECK(seen.count("effects.grass") && seen.count("view3d.vault") && seen.count("interface.far_bars"));
    CHECK(seen.count("notify.whisper") && seen.count("quality.sharpen") && seen.count("windows.title_name"));
    CHECK(!seen.count("view3d.crown_volume") && !seen.count("debug.log") && !seen.count("effects.mage_effects"));
    CHECK(!seen.count("quality.dlss") && !seen.count("ai.enabled") && !seen.count("windows.start_count"));
}

// A group with no basic row (each window's own switch; what our renderer draws) is its name and the opening line
TEST(settings_window_a_group_of_details_is_one_line) {
    const int ui = entryNamed(settings::Text::ViewInterface);
    CHECK(ui > 0);
    if (ui <= 0) return;
    const settings::catalogue::Group* windows = settings::catalogue::groupOf("interface.items");
    int lines = 0, more = 0;
    for (const settingswin::Line& l : settingswin::lines(ui, {})) {
        if (!windows || l.groupName != windows->name) continue;
        ++lines;
        CHECK(l.heads && l.what == settingswin::Line::What::More);
        more = l.more;
    }
    CHECK_EQ(lines, 1);
    CHECK_EQ(more, 36);
}

// The search: the rows of every entry whose words match - details too -, under their groups' names; the game's own
// and the choices by their names
TEST(settings_window_search) {
    const std::vector<settingswin::Line> music = settingswin::find("музика", settings::Lang::Uk);
    CHECK(!music.empty());
    if (!music.empty()) CHECK(music[0].kind == settingswin::Kind::Game && music[0].key == "game.0" && music[0].heads);
    const std::vector<settingswin::Line> map = settingswin::find("мінікарт", settings::Lang::Uk);
    CHECK(map.size() >= 3);
    CHECK(holds(map, "interface.minimap_npcs") && holds(map, "interface.net_badge"));  // a detail found too
    for (size_t i = 1; i < map.size(); ++i) CHECK(map[i].entry >= map[i - 1].entry);
    for (const settingswin::Line& l : map) CHECK(l.what == settingswin::Line::What::Row);
    const std::vector<settingswin::Line> grass = settingswin::find("трава", settings::Lang::Uk);
    CHECK(holds(grass, "effects.grass") && holds(grass, "view3d.grass"));
    const int picture = entryNamed(settings::Text::LookPicture), view3d = entryNamed(settings::Text::View3dHeading);
    for (const settingswin::Line& l : grass) {
        if (l.key == "effects.grass") CHECK_EQ(l.entry, picture);
        if (l.key == "view3d.grass") CHECK_EQ(l.entry, view3d);
    }
    // not what lives elsewhere: DLSS waits for a start, the crowns' trial is the console's
    CHECK(!holds(settingswin::find("dlss", settings::Lang::Uk), "quality.dlss"));
    CHECK(!holds(settingswin::find("крони", settings::Lang::Uk), "view3d.crown_volume"));
    CHECK(settingswin::find("", settings::Lang::Uk).empty());
    CHECK(settingswin::find("zzzzqqq", settings::Lang::En).empty());
}
