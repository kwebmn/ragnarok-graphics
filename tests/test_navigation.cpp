#include <algorithm>

#include "core/settings/navigation.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

bool listed(const std::vector<Page>& pages, Page p) { return std::find(pages.begin(), pages.end(), p) != pages.end(); }

}  // namespace

TEST(nav_simple_lists_main_effects_pad) {
    CHECK(viewsOf(Mode::Simple) == std::vector<View>({View::Main, View::Effects, View::Pad}));
}

TEST(nav_detailed_lists_eight_pages) {
    CHECK(viewsOf(Mode::Detailed) == std::vector<View>({View::Graphics, View::Screen, View::Effects, View::Ai,
                                                        View::Sound, View::Pad, View::GameChat, View::Service}));
}

TEST(nav_developer_adds_debug_last) {
    std::vector<View> dev = viewsOf(Mode::Detailed);
    dev.push_back(View::Debug);
    CHECK(viewsOf(Mode::Developer) == dev);
    CHECK(developerView(View::Debug));
    CHECK(!developerView(View::Graphics));
}

TEST(nav_every_page_has_a_place) {
    CHECK(placeOf(Page::Pack).view == View::Ai && placeOf(Page::Pack).tab == 2);
    CHECK(placeOf(Page::AiCache).view == View::Ai && placeOf(Page::AiCache).tab == 3);
    CHECK(placeOf(Page::Ai).view == View::Ai && placeOf(Page::Ai).tab == 0);
    CHECK(placeOf(Page::Frames).view == View::Graphics);
    CHECK(placeOf(Page::Chat).view == View::GameChat);
    CHECK(placeOf(Page::Diagnostics).view == View::Service);
    for (int i = 0; i < kPageCount; ++i) {
        const Place p = placeOf(Page(i));
        CHECK(p.tab >= 0 && p.tab < tabCount(p.view));
        CHECK(listed(partsOf(p.view, p.tab), Page(i)));
    }
}

TEST(nav_graphics_shows_four_pages) {
    CHECK(partsOf(View::Graphics, 0) == std::vector<Page>({Page::Render, Page::Quality, Page::Smoothing,
                                                           Page::Frames}));
    CHECK(partsOf(View::GameChat, 0) == std::vector<Page>({Page::Game, Page::Chat}));
    CHECK(tabCount(View::Ai) == 4 && tabCount(View::Graphics) == 1);
    CHECK(tabName(View::Ai, 3) == Text::PageAiCache);
    CHECK(partsOf(View::Main, 0).empty());  // its own page, rows of others
}

TEST(nav_start_mode) {
    CHECK(startMode(std::nullopt, false) == Mode::Simple);
    CHECK(startMode(std::nullopt, true) == Mode::Detailed);
    CHECK(startMode(Mode::Developer, false) == Mode::Developer);
    for (int i = 0; i < kModeCount; ++i) CHECK(parseMode(modeToken(Mode(i))) == Mode(i));
    CHECK(parseMode("DETAILED") == Mode::Detailed);
    CHECK(!parseMode("x"));
}

TEST(nav_levels) {
    CHECK(levelOf(*findSetting("debug.profile")) == Mode::Developer);
    CHECK(levelOf(*findSetting("renderer.render_thread")) == Mode::Developer);
    CHECK(levelOf(*findSetting("quality.dlss")) == Mode::Simple);
    CHECK(levelOf(*findSetting("effects.preset")) == Mode::Simple);
    CHECK(levelOf(*findSetting("effects.group_lights")) == Mode::Simple);
    CHECK(levelOf(*findSetting("effects.torch")) == Mode::Detailed);
    CHECK(levelOf(*findSetting("quality.msaa")) == Mode::Detailed);
    CHECK(mainKey("frames.fps_limit") && !mainKey("frames.keep_fps"));
    // every key the lists name is a row of the schema: a renamed key would quietly fall back to Detailed
    for (const std::string& k : simpleKeys()) CHECK(findSetting(k) != nullptr);
    for (const std::string& k : developerKeys()) CHECK(findSetting(k) != nullptr);
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
    CHECK(placeOf(*findSetting("quality.msaa")).view == View::Graphics);
}

TEST(nav_resettable_views_are_made_of_rows) {
    CHECK(resettable(View::Graphics, 0) && resettable(View::Effects, 0) && resettable(View::Sound, 0));
    CHECK(resettable(View::Ai, 0) && resettable(View::Ai, 1) && !resettable(View::Ai, 2));
    CHECK(!resettable(View::Main, 0) && !resettable(View::Service, 0) && !resettable(View::Screen, 0));
}
