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
