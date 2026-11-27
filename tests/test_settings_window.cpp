#include <algorithm>
#include <set>

#include <fstream>
#include <sstream>

#include "core/effect_groups.hpp"
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
    CHECK_EQ(int(nav.size()), 9);  // 3.0.21-beta.10: «Змінені» closes the list
    if (nav.size() != 9) return;
    CHECK(nav[8].name == settings::Text::SettingsChanged && nav[8].area == settingswin::kChangedArea);
    CHECK(settingswin::lines(8).empty());  // its lines are changedLines(): they go by the settings
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
    for (size_t i = 1; i + 1 < nav.size(); ++i) rows += nav[i].rows;
    for (const settings::catalogue::Group& g : settings::catalogue::groups())
        for (const settings::catalogue::Row& r : g.rows)
            inGame += settings::catalogue::homeOf(r) == settings::catalogue::Home::Game ? 1 : 0;
    CHECK_EQ(rows, inGame);
    CHECK(rows > 400);
}

// The first entry: the simple start's three choices, then the game's own six - what the game's Alt+O had
TEST(settings_window_first_entry_keeps_the_game_s_own) {
    const std::vector<settingswin::Line> first = settingswin::lines(0);
    CHECK_EQ(int(first.size()), 9);
    if (first.size() != 9) return;
    CHECK(first[0].kind == settingswin::Kind::Picture && first[0].key == "picture.0" && first[0].heads);
    CHECK(first[0].groupName == settings::Text::LookHead && !first[1].heads && !first[2].heads);
    CHECK(first[3].kind == settingswin::Kind::Game && first[3].key == "game.0" && first[3].heads);
    CHECK(first[3].groupName == settings::Text::OptionsTitle && first[8].key == "game.5");
    for (const settingswin::Line& l : first) CHECK(l.what == settingswin::Line::What::Row);
    CHECK(settingswin::lines(-1).empty() && settingswin::lines(99).empty());
}

// The pane: an area's groups one under another - the group's name over its first line, its basic rows, then its
// details at once. 3.0.21-beta.11 (the owner: «раскрытие в настройках лучше убрать, пускай сразу будет все вместе,
// можно както выделить внутри что это подгрупа, но раскрытие делать не стоит»): no line that opens or folds - the
// details stand under a small heading of their own («Додатково»), marked as the group's inner part
TEST(settings_window_a_group_shows_all_its_rows) {
    const int sound = entryNamed(settings::Text::CatSound);
    CHECK(sound > 0);
    if (sound <= 0) return;
    const std::vector<settingswin::Line> all = settingswin::lines(sound);
    // «Звук»: the output, the world's sounds with their volume - and not the engine (it waits for the game's start)
    CHECK(holds(all, "sound.output") && holds(all, "sound.ambience") && holds(all, "sound.ambience_volume"));
    CHECK(!holds(all, "sound.engine") && !holds(all, "sound.rate"));
    // «Сповіщення»: its seven basic rows, the small heading, its fourteen details
    CHECK(holds(all, "notify.on") && holds(all, "notify.whisper") && holds(all, "notify.sound"));
    CHECK(holds(all, "notify.trade") && holds(all, "notify.buff_seconds") && holds(all, "notify.report_answer"));
    const settings::catalogue::Group* notify = settings::catalogue::groupOf("notify.on");
    int notifyGroup = -1, heads = 0;
    for (const settingswin::Line& l : all) {
        if (l.key == "notify.on") notifyGroup = l.group;
        heads += l.heads ? 1 : 0;
    }
    CHECK_EQ(heads, 2);  // two groups: «Звук», «Сповіщення»
    int before = 0, after = 0, subs = 0, tops = 0;
    bool afterSub = false, order = true;
    for (const settingswin::Line& l : all) {
        if (l.group != notifyGroup) continue;
        CHECK(notify && l.groupName == notify->name);
        if (l.what == settingswin::Line::What::Sub) {
            ++subs;
            afterSub = true;
            CHECK(l.key.empty() && !l.heads);
            continue;
        }
        (afterSub ? after : before) += 1;
        tops += !afterSub && !l.part ? 1 : 0;
        order = order && l.detail == afterSub;  // the basic rows (their parts under them), the heading, the details
    }
    CHECK_EQ(tops, 7);            // the seven basic rows
    CHECK_EQ(before + after, 21);  // every row of the group, once
    CHECK(after > 0);
    CHECK_EQ(subs, 1);
    CHECK(order);
    // a part stands right under its row, in the catalogue's order
    size_t ending = 0, seconds = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i].key == "notify.buff_ending") ending = i;
        if (all[i].key == "notify.buff_seconds") seconds = i;
    }
    CHECK(ending > 0 && seconds == ending + 1 && all[seconds].part && !all[ending].part);
}

// Everything it lists applies at once; the trials and the diagnostics are the console's, the hidden effects the file's
TEST(settings_window_lists_only_what_applies_at_once) {
    const std::vector<settingswin::Nav>& nav = settingswin::nav();
    std::set<std::string> seen;
    for (size_t e = 1; e < nav.size(); ++e) {
        int rows = 0;
        for (const settingswin::Line& l : settingswin::lines(int(e))) {
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

// A group with no basic row (each window's own switch; what our renderer draws): its name and its rows at once - no
// small heading (nothing stands over them), none marked as an inner part
TEST(settings_window_a_group_of_details_alone) {
    const int ui = entryNamed(settings::Text::ViewInterface);
    CHECK(ui > 0);
    if (ui <= 0) return;
    const settings::catalogue::Group* windows = settings::catalogue::groupOf("interface.items");
    int rows = 0, subs = 0, marked = 0, heads = 0;
    for (const settingswin::Line& l : settingswin::lines(ui)) {
        if (!windows || l.groupName != windows->name) continue;
        if (l.what == settingswin::Line::What::Sub) ++subs;
        else ++rows;
        marked += l.detail ? 1 : 0;
        heads += l.heads ? 1 : 0;
    }
    CHECK_EQ(rows, 35);
    CHECK_EQ(subs, 0);
    CHECK_EQ(marked, 0);
    CHECK_EQ(heads, 1);
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

// 3.0.21-beta.10 (the owner after R-MSAM96, eight sliders to bring back by hand: «надо кнопку по умолчанию сделать.
// чтобы не искать»): the window knows what is not as shipped. The default of a row is the shipped file's value - the
// driver's own but «Якість ефектів» (the file says auto), each effect's switch as that set gives it on this card
TEST(settings_window_defaults_are_the_shipped_file_s) {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/uaro-graphics.ini");
    std::stringstream text;
    text << f.rdbuf();
    CHECK(text.str().size() > 1000);
    for (int level : {1, 2, 3}) {
        Config shipped = parseConfig(text.str());
        shipped.effects = effect_groups::shown(shipped.effects, level);
        const Config defaults = settingswin::defaults(level);
        int rows = 0;
        for (const settings::catalogue::Group& g : settings::catalogue::groups())
            for (const settings::catalogue::Row& r : g.rows) {
                if (settings::catalogue::homeOf(r) != settings::catalogue::Home::Game) continue;
                const settings::IniSetting* st = settings::findSetting(r.key);
                if (!st) continue;
                ++rows;
                if (settings::tokenOf(*st, defaults) != settings::tokenOf(*st, shipped))
                    std::printf("    %s: default %s, shipped %s (level %d)\n", r.key.c_str(),
                                settings::tokenOf(*st, defaults).c_str(), settings::tokenOf(*st, shipped).c_str(),
                                level);
                CHECK_EQ(settings::tokenOf(*st, defaults), settings::tokenOf(*st, shipped));
            }
        CHECK(rows > 400);
    }
}

TEST(settings_window_tells_what_is_off_its_default) {
    const int level = 3;
    const Config defaults = settingswin::defaults(level);
    const auto shownOf = [&](const std::string& ini) {
        Config c = parseConfig("[effects]\npreset = auto\n" + ini);
        c.effects = effect_groups::shown(c.effects, level);
        return c;
    };
    // as shipped: nothing to bring back
    CHECK(settingswin::changedLines(defaults, defaults).empty());
    CHECK(!settingswin::changed("effects.grade_brightness", defaults, defaults));
    // R-MSAM96's file: the colour's sliders and the 3D view's
    const Config hurt = shownOf("grade_brightness = 50\ngrade_contrast = 100\ngrade_saturation = 100\n"
                                "[view3d]\nmouse = 25\nfov_first = 50\n[quality]\nrender_scale = 50\n");
    CHECK(settingswin::changed("effects.grade_brightness", hurt, defaults));
    CHECK(settingswin::changed("view3d.mouse", hurt, defaults));
    CHECK(!settingswin::changed("effects.grade_vignette", hurt, defaults));
    const std::vector<settingswin::Line> lines = settingswin::changedLines(hurt, defaults);
    CHECK_EQ(lines.size(), size_t(6));
    CHECK(holds(lines, "effects.grade_brightness") && holds(lines, "view3d.fov_first") &&
          holds(lines, "quality.render_scale"));
    int heads = 0;
    for (const settingswin::Line& l : lines) heads += l.heads ? 1 : 0;
    CHECK_EQ(heads, 3);  // «Весь екран», «Камера й приціл», «Якість» - each named over its first row
    for (const settingswin::Line& l : lines) CHECK(l.what == settingswin::Line::What::Row && l.entry > 0);
    // a group's way back: its rows off their defaults with the values that bring them back
    const settings::catalogue::Group* colour = settings::catalogue::groupOf("effects.grade_brightness");
    CHECK(colour != nullptr);
    int group = -1;
    for (size_t g = 0; g < settings::catalogue::groups().size(); ++g)
        if (&settings::catalogue::groups()[g] == colour) group = int(g);
    const std::vector<settingswin::Reset> back = settingswin::resetsOf(group, hurt, defaults);
    CHECK_EQ(back.size(), size_t(3));
    std::set<std::string> to;
    for (const settingswin::Reset& r : back) to.insert(r.key + "=" + r.token);
    CHECK(to == (std::set<std::string>{"effects.grade_brightness=100", "effects.grade_contrast=108",
                                       "effects.grade_saturation=115"}));
    CHECK(settingswin::resetsOf(group, defaults, defaults).empty());
    // an effect's own switch: a set decides it - no row of the player's is off while «Якість ефектів» is a set...
    const Config strong = shownOf("preset = strong\n");
    const std::vector<settingswin::Line> set = settingswin::changedLines(strong, defaults);
    CHECK(holds(set, "effects.preset"));
    for (const settingswin::Line& l : set) CHECK(effect_groups::memberOf(l.key) == nullptr);
    // ... with «Свої» each switch is the player's own: off when it is not as the shipped set has it
    const Config own = shownOf("preset = custom\ngrass = 0\n");
    CHECK(settingswin::changed("effects.preset", own, defaults));
    CHECK(settingswin::changed("effects.grass", own, defaults) ==
          (settings::tokenOf(*settings::findSetting("effects.grass"), defaults) == std::string("1")));
}

// 3.0.24-beta.2 (the owner: the simple mode - «разширь больше», «включать групами», «там нужно видить возможности»):
// the simple page - the three looks, then the possibilities by sections; every key a real row of the window, every
// card's words in each language, a card's «Детально» on a group of its own area
TEST(settings_window_simple_page_of_possibilities) {
    using namespace uagfx::settingswin;
    namespace cat = uagfx::settings::catalogue;
    const std::vector<Line> s = simpleLines();
    CHECK_EQ(s.size(), size_t(38));  // 3.0.24-beta.19: the theme (a plain row) first under «Інтерфейс»
    const auto theme = std::find_if(s.begin(), s.end(), [](const Line& l) { return l.key == "interface.theme"; });
    CHECK(theme != s.end() && !theme->card && theme->heads);
    CHECK(s[0].key == "picture.0" && s[0].heads && s[0].kind == Kind::Picture);
    int cards = 0, sections = 0;
    for (const Line& l : s) {
        sections += l.heads ? 1 : 0;
        if (l.kind == Kind::Keys) CHECK(uagfx::settings::findSetting(l.key) != nullptr);
        if (!l.card) continue;
        ++cards;
        for (uagfx::settings::Lang lang : {uagfx::settings::Lang::En, uagfx::settings::Lang::Uk,
                                           uagfx::settings::Lang::Ru})
            CHECK(std::string(uagfx::settings::tr(lang, l.desc)).size() > 10);
        if (l.opens >= 0) {
            CHECK(l.inside > 1);
            CHECK(l.entry == entryOfGroup(l.opens) && l.entry > 0);
            const cat::Group* g = cat::groupOf(l.key);
            CHECK(g && int(g - cat::groups().data()) == l.opens);
        }
    }
    CHECK_EQ(cards, 30);
    CHECK_EQ(sections, 9);
    // the water's card switches its group: «Детально» on it with its rows
    const auto water = std::find_if(s.begin(), s.end(), [](const Line& l) { return l.key == "effects.group_waters"; });
    CHECK(water != s.end() && water->opens >= 0 && water->inside >= 10);
    // the new interface's card: all of «Інтерфейс» behind it
    const auto ui = std::find_if(s.begin(), s.end(), [](const Line& l) { return l.key == "interface.minimap"; });
    CHECK(ui != s.end() && ui->card && ui->opens < 0 && ui->entry > 0 && ui->inside > 100);
}

// 3.0.24-beta.5 (the owner: «нету кнопки с альт-о сбросить настройки в по умолчанию»): every row of the window off
// its default, with the value that brings it back - none when all are as shipped
TEST(settings_window_resets_all_back_to_the_defaults) {
    const Config d = settingswin::defaults(2);
    CHECK(settingswin::resetsAll(d, d).empty());
    Config shown = d;
    shown.storageWindow = !d.storageWindow;
    shown.settingsFull = !d.settingsFull;
    const std::vector<settingswin::Reset> back = settingswin::resetsAll(shown, d);
    CHECK_EQ(back.size(), size_t(2));
    for (const settingswin::Reset& r : back) CHECK(r.key == "interface.storage" || r.key == "interface.settings_full");
}
