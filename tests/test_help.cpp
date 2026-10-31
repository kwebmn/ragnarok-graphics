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
    const std::vector<Text> screen = labels(View::Screen, Mode::Detailed);
    CHECK(screen == std::vector<Text>({Text::ScreenShow, Text::ScreenMonitor, Text::ScreenUi, Text::ScreenWindowSize}));
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
    const std::vector<Text> play = labels(View::Play, Mode::Simple);
    CHECK_EQ(at(play, Text::UpdateChannel), 0);
    CHECK_EQ(at(play, labelOf("launcher.direct_start")), 1);
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
