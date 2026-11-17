#include <algorithm>
#include <map>
#include <string>

#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/settings/catalogue.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/navigation.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

// 3.0.21-beta.1 (the concept «Настройки uaRO Graphics», Claude Doc JzcE4xZDus82grUxsQjgy7; the owner: «Вариант 4.
// Каждому месту своё дело»): one catalogue of uaro-graphics.ini's rows in place of the three lists made by hand (the
// settings app's, the console's, «Налаштування»'s) - a row's area, group, level, status and needs said once

TEST(catalogue_holds_every_row_of_the_schema_once) {
    std::map<std::string, int> seen;
    for (const catalogue::Group& g : catalogue::groups())
        for (const catalogue::Row& r : g.rows) ++seen[r.key];
    for (const IniSetting& s : iniSettings()) {
        const std::string key = sectionKey(s);
        if (seen[key] != 1) std::printf("    %s: %d time(s) in the catalogue\n", key.c_str(), seen[key]);
        CHECK_EQ(seen[key], 1);
    }
    for (const auto& [key, count] : seen) {  // and none of a key the schema has not
        if (!findSetting(key)) std::printf("    %s: not a row of the schema\n", key.c_str());
        CHECK(findSetting(key) != nullptr);
    }
}

TEST(catalogue_a_row_lives_where_its_nature_puts_it) {
    const auto home = [](const char* key) {
        const catalogue::Row* r = catalogue::find(key);
        return r ? catalogue::homeOf(*r) : catalogue::Home::File;
    };
    CHECK(catalogue::find("interface.far_bars") != nullptr);
    CHECK(home("interface.far_bars") == catalogue::Home::Game);    // applies at once: in the game
    CHECK(home("notify.whisper") == catalogue::Home::Game);
    CHECK(home("effects.grass") == catalogue::Home::Game);
    CHECK(home("quality.dlss") == catalogue::Home::Start);         // waits for the game's start: the settings app
    CHECK(home("sound.engine") == catalogue::Home::Start);
    CHECK(home("ai.enabled") == catalogue::Home::Start);
    CHECK(home("windows.start_count") == catalogue::Home::Start);  // the launcher's «Грати» reads it
    CHECK(home("view3d.crown_volume") == catalogue::Home::Lab);    // a trial
    CHECK(home("debug.log") == catalogue::Home::Lab);              // a diagnostic
    CHECK(home("effects.mage_effects") == catalogue::Home::File);  // hidden behind «Вигляд умінь»
    // every row in the game applies at once; nothing that waits for a start is offered there
    for (const catalogue::Group& g : catalogue::groups())
        for (const catalogue::Row& r : g.rows)
            if (catalogue::homeOf(r) == catalogue::Home::Game) CHECK(appliesNow(r.key));
}

TEST(catalogue_trials_and_diagnostics_are_marked) {
    const catalogue::Row* crown = catalogue::find("view3d.crown_volume");
    CHECK(crown && crown->status == catalogue::Status::Trial);
    if (crown) CHECK_EQ(std::string(crown->since), std::string("3.0.20-beta.73"));
    for (const char* k : {"view3d.shadows", "view3d.dense_crowns", "view3d.crown_light", "interface.minimap_reach",
                          "interface.predict_walk", "renderer.remix", "renderer.render_thread", "display.dpi_aware"}) {
        const catalogue::Row* r = catalogue::find(k);
        CHECK(r && r->status == catalogue::Status::Trial);
    }
    for (const char* k : {"debug.log", "debug.profile", "debug.xray", "debug.state_cache", "quality.dlss_jitter",
                          "interface.uaui_showcase", "ai.generate", "roext.window_x"}) {
        const catalogue::Row* r = catalogue::find(k);
        CHECK(r && r->status == catalogue::Status::Diagnostic);
    }
    const catalogue::Row* bars = catalogue::find("interface.far_bars");
    CHECK(bars && bars->status == catalogue::Status::Permanent);
}

TEST(catalogue_one_address_for_a_row) {
    const auto group = [](const char* key) { return catalogue::groupOf(key); };
    CHECK(group("interface.minimap_npcs") && group("interface.minimap_npcs")->area == catalogue::Area::Interface);
    CHECK(group("interface.minimap_npcs") && group("interface.minimap_npcs")->name == Text::InterfaceSecMinimap);
    CHECK(group("interface.net_badge") == group("interface.minimap_npcs"));  // what the eye is on: the minimap
    CHECK(group("interface.arrows_walk") && group("interface.arrows_walk")->area == catalogue::Area::Controls);
    CHECK(group("camera.tilt") && group("camera.tilt")->area == catalogue::Area::Controls);
    CHECK(group("notify.whisper") && group("notify.whisper")->area == catalogue::Area::Sound);
    CHECK(group("sound.output") && group("sound.output")->area == catalogue::Area::Sound);
    // «Моделі нашим рендером» works in the top view too: not a row of «3D-режим»
    CHECK(group("view3d.own_models") && group("view3d.own_models")->area == catalogue::Area::Speed);
    CHECK(group("view3d.vault") && group("view3d.vault")->area == catalogue::Area::View3d);
    CHECK(group("ai.model_trees") && group("ai.model_trees")->area == catalogue::Area::Textures);
    CHECK(group("debug.log") && group("debug.log")->area == catalogue::Area::Diagnostics);
    // an effect in its group of «Ефекти», its parts and sliders under it; the clock with the day and the night
    CHECK(group("effects.water") && group("effects.water")->area == catalogue::Area::Picture);
    CHECK(group("effects.water") == group("effects.group_waters"));
    CHECK(group("effects.clock") == group("effects.group_time"));
    const catalogue::Row* foam = catalogue::find("effects.water_foam");
    CHECK(foam && foam->under == "effects.water" && foam->level == catalogue::Level::Detail);
    const catalogue::Row* waves = catalogue::find("effects.water_waves");
    CHECK(waves && waves->under == "effects.water" && waves->level == catalogue::Level::Detail);
}

TEST(catalogue_a_group_shows_seven_basic_rows_at_most) {
    for (const catalogue::Group& g : catalogue::groups()) {
        int basic = 0;
        for (const catalogue::Row& r : g.rows)
            basic += r.level == catalogue::Level::Basic && r.status == catalogue::Status::Permanent ? 1 : 0;
        if (basic > 7) std::printf("    «%s»: %d basic rows\n", tr(Lang::Uk, g.name), basic);
        CHECK(basic <= 7);
        CHECK(!g.rows.empty());
    }
}

TEST(catalogue_a_part_sits_under_a_row_of_its_group) {
    for (const catalogue::Group& g : catalogue::groups())
        for (size_t i = 0; i < g.rows.size(); ++i) {
            const catalogue::Row& r = g.rows[i];
            if (r.under.empty()) continue;
            bool before = false;
            for (size_t k = 0; k < i; ++k) before = before || g.rows[k].key == r.under;
            if (!before) std::printf("    %s under %s: not before it in its group\n", r.key.c_str(), r.under.c_str());
            CHECK(before);
            CHECK(r.level != catalogue::Level::Basic);  // a part is a detail
        }
}

TEST(catalogue_a_row_says_what_it_needs) {
    for (const catalogue::Group& g : catalogue::groups())
        for (const catalogue::Row& r : g.rows)
            for (const catalogue::Need& n : r.needs)
                if (n.kind == catalogue::NeedKind::Row) CHECK(n.key && findSetting(n.key) != nullptr);
    const auto has = [](const char* key, catalogue::NeedKind kind, const char* other = nullptr) {
        const catalogue::Row* r = catalogue::find(key);
        if (!r) return false;
        for (const catalogue::Need& n : r->needs)
            if (n.kind == kind && (!other || std::string(n.key ? n.key : "") == other)) return true;
        return false;
    };
    CHECK(has("effects.grass", catalogue::NeedKind::Direct3D12));
    CHECK(!has("effects.torch", catalogue::NeedKind::Direct3D12));
    CHECK(has("view3d.vault", catalogue::NeedKind::View3d));
    CHECK(!has("view3d.own_models", catalogue::NeedKind::View3d));  // the top view's too
    CHECK(has("view3d.dense_crowns", catalogue::NeedKind::Row, "view3d.own_models"));
    CHECK(has("view3d.dense_crowns", catalogue::NeedKind::Row, "effects.leaves_card"));
    CHECK(has("view3d.one_room", catalogue::NeedKind::Row, "view3d.ceiling"));
    CHECK(has("effects.dew", catalogue::NeedKind::Row, "effects.grass"));
    CHECK(has("frames.frame_generation", catalogue::NeedKind::Row, "quality.dlss"));
    const catalogue::Row* gaps = catalogue::find("view3d.close_gaps");
    CHECK(gaps && gaps->nextMap);
    // met by the settings shown, the renderer and the view
    const Config on = parseConfig("");
    const Config off = parseConfig("[view3d]\nown_models = 0\n");
    const catalogue::Need models{catalogue::NeedKind::Row, "view3d.own_models"};
    CHECK(catalogue::met(models, on, true, true));
    CHECK(!catalogue::met(models, off, true, true));
    const catalogue::Need dlss{catalogue::NeedKind::Row, "quality.dlss"};  // a list: not at its first word
    CHECK(!catalogue::met(dlss, parseConfig("[quality]\ndlss = off\n"), true, false));
    CHECK(catalogue::met(dlss, parseConfig("[quality]\ndlss = quality\n"), true, false));
    CHECK(catalogue::met(catalogue::Need{catalogue::NeedKind::Direct3D12}, on, true, false));
    CHECK(!catalogue::met(catalogue::Need{catalogue::NeedKind::Direct3D12}, on, false, false));
    CHECK(!catalogue::met(catalogue::Need{catalogue::NeedKind::View3d}, on, true, false));
    CHECK(catalogue::met(catalogue::Need{catalogue::NeedKind::View3d}, on, true, true));
}

TEST(catalogue_the_areas_have_names_and_rows) {
    for (int a = 0; a < catalogue::kAreaCount; ++a) {
        CHECK(std::string(tr(Lang::Uk, catalogue::areaName(catalogue::Area(a)))).size() > 2);
        int rows = 0;
        for (const catalogue::Group& g : catalogue::groups())
            rows += g.area == catalogue::Area(a) ? int(g.rows.size()) : 0;
        CHECK(rows > 0);
    }
    CHECK_EQ(std::string(tr(Lang::Uk, catalogue::areaName(catalogue::Area::Speed))), std::string("Швидкість і екран"));
    CHECK_EQ(std::string(tr(Lang::Ru, catalogue::areaName(catalogue::Area::Sound))), std::string("Звук и уведомления"));
}

// the lists made by hand before - «Інтерфейс»'s sections and the controller's rows - now read from the catalogue: the
// groups that hold [interface] rows, in its order; the windows' group (no basic row) folded behind a button
TEST(catalogue_feeds_the_interface_sections) {
    const auto holds = [](const InterfaceSection& s, const char* key) {
        return std::any_of(s.keys.begin(), s.keys.end(), [&](const char* k) { return std::string(k) == key; });
    };
    const std::vector<InterfaceSection>& secs = interfaceSections();
    CHECK(secs.size() >= 10);
    bool minimap = false, windows = false, chat = false;
    for (const InterfaceSection& s : secs) {
        minimap = minimap || (s.heading == Text::InterfaceSecMinimap && holds(s, "interface.minimap_npcs") && !s.folded);
        windows = windows || (s.heading == Text::InterfaceSecWindows && holds(s, "interface.items") && s.folded);
        // the chat's rows a group of their own (the panels' section held 32 rows)
        chat = chat || (s.heading == Text::InterfaceChat && holds(s, "interface.chat_fade") && !s.folded);
        for (const char* k : s.keys) CHECK(std::string(k).rfind("interface.", 0) == 0);
        if (s.heading != Text::InterfaceSecWindows) CHECK(!s.folded);  // one folded list: the windows'
    }
    CHECK(minimap);
    CHECK(windows);
    CHECK(chat);
    const std::vector<const char*>& pad = padInterfaceKeys();
    CHECK(std::any_of(pad.begin(), pad.end(), [](const char* k) { return std::string(k) == "interface.pad_walk"; }));
    CHECK(std::none_of(pad.begin(), pad.end(), [](const char* k) { return std::string(k) == "pad.enabled"; }));
}

// 3.0.21-beta.2 (step 2: the console's laboratory): the trials by area, the diagnostics that apply at once, what a
// row says about itself
TEST(catalogue_the_laboratory_lists_the_trials_by_area) {
    const std::vector<catalogue::LabSection> lab = catalogue::trials();
    CHECK(lab.size() >= 3);
    if (lab.empty()) return;
    CHECK(lab[0].area == catalogue::Area::View3d);  // the catalogue's order
    CHECK_EQ(int(lab[0].rows.size()), 4);
    CHECK_EQ(lab[0].rows[0]->key, std::string("view3d.shadows"));
    int listed = 0, trials = 0;
    for (const catalogue::LabSection& s : lab)
        for (const catalogue::Row* r : s.rows) {
            CHECK(r->status == catalogue::Status::Trial);
            ++listed;
        }
    for (const catalogue::Group& g : catalogue::groups())
        for (const catalogue::Row& r : g.rows) trials += r.status == catalogue::Status::Trial ? 1 : 0;
    CHECK_EQ(listed, trials);  // each once
    for (size_t i = 1; i < lab.size(); ++i) CHECK(lab[i].area != lab[i - 1].area);  // an area one section
    // the diagnostics the console can switch: those that apply at once
    const std::vector<const catalogue::Row*> diag = catalogue::liveDiagnostics();
    const auto has = [&](const char* key) {
        return std::any_of(diag.begin(), diag.end(), [&](const catalogue::Row* r) { return r->key == key; });
    };
    CHECK(has("debug.log") && has("debug.xray") && has("debug.quick_acks") && has("interface.uaui_showcase"));
    CHECK(!has("debug.state_cache"));  // waits for the game's start: the settings app's developer page
    CHECK(!has("roext.window_x"));
    CHECK(!has("view3d.shadows"));     // a trial is not a diagnostic
}

TEST(catalogue_a_row_tells_about_itself) {
    const Config on = parseConfig("");
    const catalogue::Row* crown = catalogue::find("view3d.crown_volume");
    CHECK(crown != nullptr);
    if (!crown) return;
    catalogue::About a = catalogue::about(*crown, on, true, true, Lang::Uk);
    CHECK_EQ(a.since, std::string("3.0.20-beta.73"));
    CHECK_EQ(int(a.needs.size()), 3);
    if (a.needs.size() != 3) return;
    CHECK_EQ(a.needs[0].name, std::string("Моделі нашим рендером"));
    CHECK_EQ(a.needs[1].name, std::string("Листя на відеокарті"));
    CHECK_EQ(a.needs[2].name, std::string("3D-режим"));
    CHECK(a.needs[0].met && a.needs[1].met && a.needs[2].met && a.allMet());
    CHECK(!a.afterRestart && !a.nextMap && !a.empty());
    CHECK_EQ(catalogue::aboutText(a, Lang::Uk, "+", "-"),
             std::string("з 3.0.20-beta.73 · потрібні: +Моделі нашим рендером, +Листя на відеокарті, +3D-режим"));
    // the top view with the models off: two needs not met
    a = catalogue::about(*crown, parseConfig("[view3d]\nown_models = 0\n"), true, false, Lang::Ru);
    CHECK(!a.needs[0].met && a.needs[1].met && !a.needs[2].met && !a.allMet());
    CHECK_EQ(catalogue::aboutText(a, Lang::Ru, "+", "-"),
             std::string("с 3.0.20-beta.73 · нужны: -Модели нашим рендером, +Листва на видеокарте, -3D-режим"));
    // a row that waits for the game's start; one whose change shows from the next map; Direct3D 12
    const catalogue::Row* dpi = catalogue::find("display.dpi_aware");
    CHECK(dpi && catalogue::about(*dpi, on, true, false, Lang::Uk).afterRestart);
    if (dpi)
        CHECK_EQ(catalogue::aboutText(catalogue::about(*dpi, on, true, false, Lang::Uk), Lang::Uk, "+", "-"),
                 std::string("з 3.0.11-beta.292 · після перезапуску гри"));
    const catalogue::Row* gaps = catalogue::find("view3d.close_gaps");
    CHECK(gaps && catalogue::about(*gaps, on, true, true, Lang::Uk).nextMap);
    const catalogue::Row* grass = catalogue::find("effects.grass");
    CHECK(grass != nullptr);
    if (grass) {
        // 3.0.21-beta.4: an effect needs its group on too - «Трава й дерева» (on), then Direct3D 12 (not here)
        const catalogue::About g11 = catalogue::about(*grass, on, false, false, Lang::Uk);
        CHECK_EQ(int(g11.needs.size()), 2);
        if (g11.needs.size() == 2) {
            CHECK(g11.needs[0].name == "Трава й дерева" && g11.needs[0].met);
            CHECK(g11.needs[1].name == "Direct3D 12" && !g11.needs[1].met);
        }
    }
    // a row with nothing to say: no line under it
    const catalogue::Row* bars = catalogue::find("interface.far_bars");
    CHECK(bars && catalogue::about(*bars, on, true, false, Lang::Uk).empty());
    if (bars) CHECK_EQ(catalogue::aboutText(catalogue::about(*bars, on, true, false, Lang::Uk), Lang::Uk, "+", "-"),
                       std::string());
}

// «Швидко»'s tiles are the catalogue's rows marked «switched most»: settings for good that apply at once, each a
// switch (a tile) or a list (a row of its own); never a trial (those are the laboratory's)
TEST(catalogue_the_rows_switched_most) {
    const std::vector<const catalogue::Row*> rows = catalogue::often();
    CHECK(rows.size() >= 7);
    const auto has = [&](const char* key) {
        return std::any_of(rows.begin(), rows.end(), [&](const catalogue::Row* r) { return r->key == key; });
    };
    CHECK(has("effects.grass") && has("interface.mob_hp") && has("frames.fps_counter"));
    CHECK(!has("view3d.crown_volume") && !has("view3d.shadows") && !has("effects.mage_effects"));
    for (const catalogue::Row* r : rows) {
        CHECK(r->status == catalogue::Status::Permanent);
        CHECK(catalogue::homeOf(*r) == catalogue::Home::Game);
        const IniSetting* s = findSetting(r->key);
        CHECK(s && (s->control == Control::Check || s->control == Control::Choice));
    }
}

// 3.0.21-beta.4 («строка сама говорит, почему не работает»): what keeps a row from doing anything now - the renderer,
// the view, a row it needs that is off (an effect's part needs its effect, an effect its group); nothing said when
// nothing is in its way
TEST(catalogue_a_row_says_why_it_does_nothing) {
    const Config on = parseConfig("");
    const auto why = [&](const char* key, const Config& c, bool d3d12, bool view3d, Lang l = Lang::Uk) {
        const catalogue::Row* r = catalogue::find(key);
        return r ? catalogue::whyNot(*r, c, d3d12, view3d, l) : std::string("?");
    };
    CHECK_EQ(why("interface.far_bars", on, true, false), std::string());
    CHECK_EQ(why("effects.grass", on, true, false), std::string());
    CHECK_EQ(why("effects.grass", on, false, false), std::string("Працює лише на Direct3D 12"));
    CHECK_EQ(why("effects.grass", on, false, false, Lang::Ru), std::string("Работает только на Direct3D 12"));
    // a part of an effect needs the effect on; an effect needs its group on
    CHECK_EQ(why("effects.torch_flame", on, true, false), std::string());
    CHECK_EQ(why("effects.torch_flame", parseConfig("[effects]\ntorch = 0\n"), true, false),
             std::string("Щоб працювало, увімкніть: «Живі смолоскипи»"));
    CHECK_EQ(why("effects.torch", parseConfig("[effects]\ngroup_fire = 0\n"), true, false),
             std::string("Щоб працювало, увімкніть: «Вогонь і дим»"));
    CHECK_EQ(why("effects.group_fire_strength", parseConfig("[effects]\ngroup_fire = 0\n"), true, false),
             std::string("Щоб працювало, увімкніть: «Вогонь і дим»"));
    CHECK_EQ(why("effects.torch_flame", parseConfig("[effects]\ngroup_fire = 0\ntorch = 0\n"), true, false),
             std::string("Щоб працювало, увімкніть: «Вогонь і дим», «Живі смолоскипи»"));
    CHECK_EQ(why("effects.group_fire", parseConfig("[effects]\ngroup_fire = 0\n"), true, false), std::string());
    // another row it needs, and the renderer, in one line
    CHECK_EQ(why("effects.dew", on, true, false), std::string("Щоб працювало, увімкніть: «Трава»"));
    CHECK_EQ(why("effects.footprints", parseConfig("[effects]\nactor_shadows = 0\n"), false, false),
             std::string("Працює лише на Direct3D 12 \xC2\xB7 Щоб працювало, увімкніть: «Тіні від сонця й вогню»"));
    // the 3D view's own area needs no telling that it is the 3D view's; a row of another area does
    CHECK_EQ(why("view3d.vault", on, true, false), std::string());
    CHECK_EQ(why("view3d.fast_ground", on, true, false), std::string("Працює лише у 3D-режимі"));
    CHECK_EQ(why("view3d.fast_ground", on, true, true), std::string());
    CHECK_EQ(why("view3d.one_room", parseConfig("[view3d]\nceiling = 0\n"), true, true),
             std::string("Щоб працювало, увімкніть: «Стеля в приміщеннях»"));
    // when a change shows
    CHECK_EQ(why("view3d.close_gaps", on, true, true), std::string("Зміна діє з наступної карти"));
}
