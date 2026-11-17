#include <algorithm>
#include <cstdio>

#include "core/settings/help.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

std::vector<Text> labels(View v, Mode m) {
    std::vector<Text> out;
    for (const HelpRow& r : helpRows(v, m, Machine{})) out.push_back(r.label);
    return out;
}

int count(const std::vector<Text>& list, Text t) { return int(std::count(list.begin(), list.end(), t)); }

int at(const std::vector<Text>& list, Text t) {
    const auto it = std::find(list.begin(), list.end(), t);
    return it == list.end() ? -1 : int(it - list.begin());
}

Text labelOf(const char* key) { return findSetting(key)->label; }

Text hintOf(View v, Mode m, Text label) {
    for (const HelpRow& r : helpRows(v, m, Machine{}))
        if (r.label == label) return r.hint;
    return Text::Count;
}

}  // namespace

// The page's order: the set, the mode (not in Simple), the clock and its night, the season, the weather, the groups
TEST(help_effects_in_the_pages_order) {
    const std::vector<Text> simple = labels(View::Effects, Mode::Simple);
    CHECK_EQ(at(simple, labelOf("effects.preset")), 0);
    CHECK_EQ(at(simple, labelOf("effects.clock")), 1);
    CHECK_EQ(at(simple, labelOf("effects.night_strength")), 2);
    CHECK_EQ(at(simple, labelOf("effects.season")), 3);
    CHECK_EQ(at(simple, labelOf("effects.weather_kind")), 4);
    CHECK_EQ(at(simple, labelOf("effects.group_time")), 5);  // the first group
    CHECK_EQ(at(simple, labelOf("effects.tier")), -1);
    CHECK(at(simple, labelOf("effects.group_game_fixes")) > 5);
    const std::vector<Text> detailed = labels(View::Effects, Mode::Detailed);
    CHECK_EQ(at(detailed, labelOf("effects.tier")), 1);
    // the effects themselves are on the page, under their groups
    CHECK_EQ(at(detailed, labelOf("effects.water")), -1);
}

// Each function once: the groups' strength sliders share one name, the chat's ten macros too
TEST(help_each_function_once) {
    for (Mode m : {Mode::Simple, Mode::Detailed, Mode::Developer})
        for (View v : viewsOf(m)) {
            const std::vector<Text> list = labels(v, m);
            for (Text t : list) CHECK_EQ(count(list, t), 1);
        }
    CHECK_EQ(count(labels(View::Effects, Mode::Simple), labelOf("effects.group_weather_strength")), 1);
    CHECK_EQ(count(labels(View::GameChat, Mode::Detailed), Text::ChatMacrosHeading), 1);
    CHECK(hintOf(View::GameChat, Mode::Detailed, Text::ChatMacrosHeading) == Text::ChatMacrosHint);
}

// The rows the pages build themselves are there too, with the hints they show under the mouse
TEST(help_holds_the_pages_own_rows) {
    // 3.0.21-beta.6: «Екран і рендер» - the screen's own four first, then the renderer's and the frames' rows that
    // wait for a start (what applies at once lives in the game)
    const std::vector<Text> screen = labels(View::Screen, Mode::Detailed);
    CHECK(screen.size() > 4);
    CHECK(std::vector<Text>(screen.begin(), screen.begin() + std::min<size_t>(4, screen.size())) ==
          std::vector<Text>({Text::ScreenShow, Text::ScreenMonitor, Text::ScreenUi, Text::ScreenWindowSize}));
    CHECK(at(screen, labelOf("renderer.api")) > 3 && at(screen, labelOf("quality.dlss")) > 3);
    CHECK_EQ(at(screen, labelOf("quality.sharpen")), -1);
    CHECK_EQ(at(screen, labelOf("frames.vsync")), -1);
    CHECK(hintOf(View::Screen, Mode::Detailed, Text::ScreenShow) == Text::ScreenShowHint);
    CHECK(hintOf(View::Screen, Mode::Detailed, Text::ScreenMonitor) == Text::ScreenMonitorHint);
    CHECK(hintOf(View::Screen, Mode::Detailed, Text::ScreenUi) == Text::ScreenUiHint);
    const std::vector<Text> pad = labels(View::Pad, Mode::Simple);
    CHECK_EQ(at(pad, Text::PadEnabled), 0);
    CHECK_EQ(at(pad, Text::PadType), 1);
    for (Text t : {Text::PadLightbar, Text::PadTouchpad, Text::PadRumble, Text::PadStartsOn, Text::PadWalkRadius,
                   Text::PadNpcHint, Text::PadWho, Text::PadButtons, Text::PadMark})  // 3.0.11-beta.89: the page's own
        CHECK(at(pad, t) > 1);
    CHECK(hintOf(View::Pad, Mode::Simple, Text::PadNpcHint) == Text::PadNpcHintHint);
    // 3.0.18-beta.193: «Гра»'s two rows in «Сервіс» (the launcher took «Гра»'s place) - the start a detailed row
    CHECK_EQ(at(labels(View::Service, Mode::Simple), Text::UpdateChannel), 0);
    CHECK(at(labels(View::Service, Mode::Detailed), labelOf("launcher.direct_start")) > 0);
    CHECK_EQ(at(labels(View::Main, Mode::Simple), Text::PageQuality), 0);
    // «Головне» builds its rows by name in every mode: the gamepad's switch among them
    const std::vector<Text> main = labels(View::Main, Mode::Simple);
    CHECK_EQ(at(main, labelOf("quality.dlss")), 1);
    CHECK(at(main, Text::PadEnabled) > at(main, labelOf("sound.engine")));
    CHECK_EQ(at(main, labelOf("ai.enabled")), int(main.size()) - 1);
    CHECK(hintOf(View::Main, Mode::Simple, Text::PageQuality) == Text::MainQualityHint);
    const std::vector<Text> ai = labels(View::Ai, Mode::Detailed);
    CHECK_EQ(at(ai, Text::AiPreset), at(ai, labelOf("ai.enabled")) + 1);
}

// Making the pack is the developer's (the pack comes with the full client)
TEST(help_pack_rows_the_developers) {
    CHECK_EQ(at(labels(View::Ai, Mode::Detailed), Text::PackQuality), -1);
    CHECK(at(labels(View::Ai, Mode::Developer), Text::PackQuality) >= 0);
    CHECK(at(labels(View::Ai, Mode::Developer), Text::PackWorld) >= 0);
}

// Every view but «Довідка» itself has a line of what it is for
TEST(help_every_view_said) {
    for (View v : viewsOf(Mode::Developer))
        if (v != View::Help) CHECK(helpOf(v) != Text::HelpIntro);
}

// Every function has a description, not just its name: the game's own options too
TEST(help_every_row_described) {
    for (Mode m : {Mode::Simple, Mode::Detailed, Mode::Developer})
        for (View v : viewsOf(m))
            for (const HelpRow& r : helpRows(v, m, Machine{})) {
                if (r.hint == r.label) std::printf("  no hint: %s\n", tr(Lang::En, r.label));
                CHECK(r.hint != r.label);
            }
}

// 3.0.21-beta.6 (variant 4, step 4): what lives in the game's «Налаштування» is told in «Довідка» too - the main of
// each area, each function once and described; «Картинка» as the Effects page told it
TEST(help_tells_the_main_of_what_lives_in_the_game) {
    using uagfx::settings::catalogue::Area;
    const auto has = [](const std::vector<HelpRow>& rows, const char* key) {
        const Text label = findSetting(key)->label;
        return std::any_of(rows.begin(), rows.end(), [&](const HelpRow& r) { return r.label == label; });
    };
    const std::vector<HelpRow> picture = helpRowsInGame(Area::Picture, Machine{});
    CHECK(has(picture, "effects.preset") && has(picture, "effects.clock") && has(picture, "effects.group_waters"));
    CHECK(!has(picture, "effects.water_foam"));  // a part: on its row in the game
    const std::vector<HelpRow> ui = helpRowsInGame(Area::Interface, Machine{});
    CHECK(has(ui, "interface.minimap") && has(ui, "interface.mob_hp"));
    CHECK(!has(ui, "interface.items"));  // a detail
    const std::vector<HelpRow> sound = helpRowsInGame(Area::Sound, Machine{});
    CHECK(has(sound, "notify.whisper") && has(sound, "sound.output") && !has(sound, "sound.engine"));
    CHECK(helpRowsInGame(Area::Textures, Machine{}).empty());     // all of it waits for a start: the app's
    CHECK(helpRowsInGame(Area::Diagnostics, Machine{}).empty());
    for (int a = 0; a < uagfx::settings::catalogue::kAreaCount; ++a) {
        const std::vector<HelpRow> rows = helpRowsInGame(Area(a), Machine{});
        for (const HelpRow& r : rows) {
            CHECK(r.hint != r.label);
            CHECK_EQ(int(std::count_if(rows.begin(), rows.end(), [&](const HelpRow& x) { return x.label == r.label; })),
                     1);
        }
    }
    CHECK(helpOfArea(Area::Picture) == Text::HelpViewEffects && helpOfArea(Area::Sound) == Text::Count);
}
