#include <algorithm>

#include "core/settings/catalogue.hpp"
#include "core/settings/navigation.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

bool listed(const std::vector<Page>& pages, Page p) { return std::find(pages.begin(), pages.end(), p) != pages.end(); }

}  // namespace

TEST(nav_simple_lists_every_page) {
    // 3.0.17-beta.5 (the concept «простой старт»): one list in every mode, «Головна» second (the three choices);
    // the simple mode folds the detailed rows instead of hiding pages. 3.0.18-beta.193: «Головна» first («Гра» out)
    CHECK(viewsOf(Mode::Simple) == viewsOf(Mode::Detailed));
    CHECK(viewsOf(Mode::Simple).at(0) == View::Main);
}

// 3.0.21-beta.6 (the concept «Настройки uaRO Graphics», variant 4, step 4; the owner: «делай»): «Інтерфейс», «Ефекти»
// and «Графіка» are no entries of the app's list any more - their rows that apply at once live in the game's
// «Налаштування» (Alt+O), «Графіка»'s that wait for a start are «Екран і рендер»'s. The views stay for their pages'
// places (a row found by the search)
TEST(nav_the_games_areas_are_not_the_apps_entries) {
    for (Mode m : {Mode::Simple, Mode::Detailed, Mode::Developer}) {
        const std::vector<View> list = viewsOf(m);
        for (View v : {View::Interface, View::Effects, View::Graphics})
            CHECK(std::find(list.begin(), list.end(), v) == list.end());
    }
    CHECK(viewName(View::Interface) == Text::ViewInterface);
    CHECK(partsOf(View::Interface, 0) == std::vector<Page>({Page::Interface}));
    CHECK(placeOf(Page::Interface).view == View::Interface && tabCount(View::Interface) == 1);
    const IniSetting* s = findSetting("interface.minimap");
    CHECK(s && s->page == Page::Interface && !inApp(*s));
}

// 3.0.2-beta.6 (the user: a description of every function): «Довідка» last in every mode's list, before Debug
TEST(nav_help_in_every_mode) {
    for (Mode m : {Mode::Simple, Mode::Detailed, Mode::Developer}) {
        const std::vector<View> list = viewsOf(m);
        CHECK(std::find(list.begin(), list.end(), View::Help) != list.end());
    }
    CHECK(viewName(View::Help) == Text::ViewHelp);
    CHECK(partsOf(View::Help, 0).empty() && tabCount(View::Help) == 1 && !developerView(View::Help));
}

TEST(nav_detailed_lists_eight_pages) {
    CHECK(viewsOf(Mode::Detailed) == std::vector<View>({View::Main, View::Screen, View::Ai, View::Sound, View::Pad,
                                                        View::GameChat, View::Service, View::Help}));
}

// 3.0.18-beta.193 (the launcher, docs/superpowers/specs/2026-11-12-launcher-design.md): the app opens on the launcher;
// «Гра» leaves the settings' list (the launcher is it), the settings behind the gear open on «Головна»; its row
// («Запускати гру одразу») is «Сервіс»'s
TEST(views_open_on_main_the_launcher_out_of_the_list) {
    for (Mode m : {Mode::Simple, Mode::Detailed, Mode::Developer}) {
        const std::vector<View> list = viewsOf(m);
        CHECK(list.front() == View::Main);
        CHECK(std::find(list.begin(), list.end(), View::Play) == list.end());
    }
    CHECK(placeOf(Page::Launcher).view == View::Service);
    const std::vector<Page> service = partsOf(View::Service, 0);
    CHECK(std::find(service.begin(), service.end(), Page::Launcher) != service.end());
    CHECK(partsOf(View::Play, 0).empty());
}

TEST(nav_developer_adds_debug_last) {
    std::vector<View> dev = viewsOf(Mode::Detailed);
    dev.push_back(View::Debug);
    CHECK(viewsOf(Mode::Developer) == dev);
    CHECK(developerView(View::Debug));
    CHECK(!developerView(View::Screen));
}

TEST(nav_every_page_has_a_place) {
    CHECK(placeOf(Page::Pack).view == View::Ai && placeOf(Page::Pack).tab == 2);
    CHECK(placeOf(Page::AiCache).view == View::Ai && placeOf(Page::AiCache).tab == 3);
    CHECK(placeOf(Page::Ai).view == View::Ai && placeOf(Page::Ai).tab == 0);
    CHECK(placeOf(Page::Frames).view == View::Screen);  // 3.0.21-beta.6: «Екран і рендер»
    CHECK(placeOf(Page::Chat).view == View::GameChat);
    CHECK(placeOf(Page::Diagnostics).view == View::Service);
    for (int i = 0; i < kPageCount; ++i) {
        const Place p = placeOf(Page(i));
        CHECK(p.tab >= 0 && p.tab < tabCount(p.view));
        CHECK(listed(partsOf(p.view, p.tab), Page(i)));
    }
}

// 3.0.21-beta.6: the screen and what draws the game - one entry, «Екран і рендер»
TEST(nav_screen_and_renderer_are_one_entry) {
    CHECK(partsOf(View::Screen, 0) == std::vector<Page>({Page::Screen, Page::Render, Page::Quality, Page::Smoothing,
                                                         Page::Frames}));
    CHECK(viewName(View::Screen) == Text::ViewScreenRender);
    CHECK_EQ(std::string(tr(Lang::Uk, viewName(View::Screen))), std::string("Екран і рендер"));
    CHECK(partsOf(View::GameChat, 0) == std::vector<Page>({Page::Game, Page::Chat}));
    CHECK(tabCount(View::Ai) == 4 && tabCount(View::Screen) == 1);
    CHECK(tabName(View::Ai, 3) == Text::PageAiCache);
    CHECK(partsOf(View::Main, 0).empty());  // its own page, rows of others
}

// 3.0.21-beta.6: one mode for every player - what «Докладний» was: every page's own parts, the rows by their levels
// (the basic ones shown, the rest behind «Ще N налаштувань ›»); the developer's stays (Ctrl+Shift+D)
TEST(nav_start_mode) {
    CHECK(startMode(std::nullopt, false) == Mode::Detailed);
    CHECK(startMode(std::nullopt, true) == Mode::Detailed);
    CHECK(startMode(Mode::Simple, true) == Mode::Detailed);  // a file of before
    CHECK(startMode(Mode::Detailed, false) == Mode::Detailed);
    CHECK(startMode(Mode::Developer, false) == Mode::Developer);
    for (int i = 0; i < kModeCount; ++i) CHECK(parseMode(modeToken(Mode(i))) == Mode(i));
    CHECK(parseMode("DETAILED") == Mode::Detailed);
    CHECK(!parseMode("x"));
}

TEST(nav_debug_rows_are_developer) {
    int rows = 0;
    for (const IniSetting& s : iniSettings()) {
        if (s.page != Page::Debug) continue;
        ++rows;
        CHECK(levelOf(s) == Mode::Developer);
    }
    CHECK(rows >= 15);
    CHECK(partsOf(View::Debug, 0) == std::vector<Page>({Page::Debug}));
    CHECK(placeOf(Page::Debug).view == View::Debug);
    for (const char* k : {"renderer.render_thread", "renderer.remix", "debug.profile", "debug.log",
                          "renderer.bridge_present", "quality.dlss_jitter", "debug.shader_variants"})
        CHECK(findSetting(k) && findSetting(k)->page == Page::Debug);
}

TEST(nav_ai_rows_by_tab) {
    CHECK(placeOf(*findSetting("ai.model_trees")).tab == 1);
    CHECK(placeOf(*findSetting("ai.small_model")).tab == 1);
    CHECK(placeOf(*findSetting("ai.denoise")).tab == 0);
    CHECK(placeOf(*findSetting("quality.msaa")).view == View::Screen);
}

TEST(nav_resettable_views_are_made_of_rows) {
    CHECK(resettable(View::Screen, 0) && resettable(View::Sound, 0));  // 3.0.21-beta.6: «Екран і рендер»'s rows
    CHECK(resettable(View::Ai, 0) && resettable(View::Ai, 1) && !resettable(View::Ai, 2));
    CHECK(!resettable(View::Main, 0) && !resettable(View::Service, 0));
    CHECK(!resettable(View::Graphics, 0) && !resettable(View::Effects, 0));  // no entries of the list
}

TEST(rows_hide_what_the_card_cannot_do) {
    // 2.15: DLSS, frame generation and Reflex on NVIDIA cards only, HDR with an HDR screen only
    Machine amd;
    amd.vendor = 0x1002;
    Machine rtx;
    rtx.vendor = 0x10de;
    rtx.hdr = true;
    for (const char* k : {"quality.dlss", "quality.dlss_scale", "quality.dlss_width", "quality.dlss_height",
                          "quality.dlss_model", "quality.dlss_jitter", "quality.dlss_motion", "frames.frame_generation",
                          "frames.reflex"}) {
        CHECK(!fitsMachine(k, amd));
        CHECK(fitsMachine(k, rtx));
    }
    CHECK(!fitsMachine("display.hdr", amd));
    CHECK(fitsMachine("display.hdr", rtx));
    CHECK(fitsMachine("quality.msaa", amd));  // the rest everywhere
    CHECK(fitsMachine("quality.dlss", Machine{}));  // no card known (no probe): nothing hidden for it
}

// 3.0.21-beta.6 (variant 4, step 4): the app's pages hold what is set before the game starts - the rows that wait
// for a start, the developers' that wait for one and their own page's; never a row that lives in the game's
// «Налаштування»
TEST(nav_the_app_holds_what_waits_for_a_start) {
    const auto in = [](const char* key) {
        const IniSetting* s = findSetting(key);
        return s && inApp(*s);
    };
    for (const char* k : {"quality.dlss", "frames.frame_generation", "display.hdr", "renderer.api", "renderer.gpu",
                          "renderer.cross_present", "sound.engine", "sound.rate", "ai.enabled", "ai.model_trees",
                          "roext.window_lock", "client.cps", "report.auto", "report.disconnects", "stats.send",
                          "windows.start_count", "launcher.direct_start", "pad.enabled"})
        CHECK(in(k));
    for (const char* k : {"effects.grass", "effects.preset", "interface.minimap", "interface.items", "notify.whisper",
                          "view3d.vault", "quality.sharpen", "quality.msaa", "frames.vsync", "sound.output",
                          "camera.tilt", "windows.title_name", "panel.show", "connection.patient_entry"})
        CHECK(!in(k));
    // the developers': those that wait for a start, and their own page's
    for (const char* k : {"renderer.remix", "display.dpi_aware", "debug.state_cache", "debug.log", "debug.profile"})
        CHECK(in(k));
    CHECK(!in("view3d.crown_volume"));      // a trial that applies at once: the console's «Проби»
    CHECK(!in("interface.uaui_showcase"));  // the console's «Діагностика»
    CHECK(!in("effects.mage_effects"));     // the file's alone
    for (const IniSetting& s : iniSettings()) {  // one home: nothing of the app's pages lives in the game's window
        const catalogue::Row* r = catalogue::find(sectionKey(s));
        CHECK(r != nullptr);
        if (r && inApp(s)) CHECK(catalogue::homeOf(*r) != catalogue::Home::Game);
    }
}

// The rows' levels in the place of the modes: a basic row is shown, a detail or the technique folds behind «Ще N
// налаштувань ›», a trial or a diagnostic is the developers'
TEST(nav_levels_in_the_place_of_modes) {
    const auto level = [](const char* key) { return levelOf(*findSetting(key)); };
    CHECK(level("quality.dlss") == Mode::Simple);
    CHECK(level("quality.dlss_model") == Mode::Detailed);
    CHECK(level("sound.engine") == Mode::Simple && level("sound.rate") == Mode::Detailed);
    CHECK(level("roext.window_lock") == Mode::Simple && level("roext.code_page") == Mode::Detailed);
    CHECK(level("interface.minimap") == Mode::Simple && level("interface.items") == Mode::Detailed);
    CHECK(level("renderer.remix") == Mode::Developer && level("debug.log") == Mode::Developer);
    CHECK(level("view3d.crown_volume") == Mode::Developer && level("ai.generate") == Mode::Developer);
}

// Where a row lives when it is not the app's: the search says it under the row
TEST(nav_where_a_row_lives_in_the_game) {
    const auto place = [](const char* key, Lang l) { return gamePlace(*findSetting(key), l); };
    CHECK_EQ(place("effects.water_foam", Lang::Uk), std::string("Alt+O → Картинка → Вода"));
    CHECK_EQ(place("notify.whisper", Lang::Ru), std::string("Alt+O → Звук и уведомления → Уведомления"));
    CHECK_EQ(place("interface.minimap_npcs", Lang::En), std::string("Alt+O → Interface → Minimap"));
    CHECK_EQ(place("view3d.crown_volume", Lang::Uk), std::string("Ctrl+~ → Проби"));
    CHECK_EQ(place("interface.uaui_showcase", Lang::Uk), std::string("Ctrl+~ → Діагностика"));
    CHECK_EQ(place("quality.dlss", Lang::Uk), std::string());          // the app's own
    CHECK_EQ(place("debug.state_cache", Lang::Uk), std::string());     // the app's developer page
    CHECK_EQ(place("effects.mage_effects", Lang::Uk), std::string());  // the file's alone
}
