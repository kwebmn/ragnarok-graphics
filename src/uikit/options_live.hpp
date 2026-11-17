#pragma once
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace uagfx::uikit::options {

// 3.0.18-beta.82 (item 4): the game's options a live window - its volumes the library's sliders, its switches
// ticks. 3.0.18-beta.122 (the concept «Ігровий інтерфейс», stages 5-6: «Налаштування» in place of Alt+O): a list on
// the left, a search over it all, each row a switch, a slider or choices; ours written into uaro-graphics.ini as the
// console writes them (win/setting_edit).
// 3.0.21-beta.3 (the concept «Настройки uaRO Graphics», the owner: «Вариант 4. Каждому месту своё дело», step 3):
// the player's one place in the game - the list's entries are the catalogue's areas, each with its rows' count; the
// pane an entry's groups one under another: a group's name over its first row, a part indented under its row, a line
// that opens the group's details in a row's place. Here its markup and what it shows (win/options_card fills it from
// core settingswin and takes its events); laid out by RmlUi in the host's tests too.
struct ChoiceView {
    Rml::String label;
    bool on = false;
};
struct RowView {
    Rml::String key, group, label, hint, vtext;  // group: the group's name over this row (its first), "" none
    bool toggle = false, on = false, slider = false;
    int value = 0, min = 0, max = 0, step = 1;
    std::vector<ChoiceView> choices;
    bool more = false, sub = false;  // the line that opens a group's details or folds them; a part of the row over it
    int opens = 0;                   // more: the catalogue's group
};
struct NavView {
    Rml::String name;
    int count = 0;  // beside the name when over 0: its rows, or what the search found in it
};
struct Model {
    int sec = 0, shownRows = 0;  // the entry shown; the rows with words (the list is padded: never shorter)
    Rml::String query, title, hint, searchHint, none, consoleBtn, ground = "#10141cf0";
    std::vector<NavView> nav;
    std::vector<RowView> rows;
};

// 3.0.18-beta.132 (R-4BXZPM «srows[1].choices[10].on»): never shorter lists than before - a shorter one left RmlUi
// rows past its end for a frame. The rows padded to `shownBefore` (kLeastRows at least) with empty hidden ones, every
// row's choices to kMostChoices (the markup reads a row's first choice)
constexpr size_t kMostChoices = 12, kLeastRows = 24;
void pad(std::vector<RowView>& rows, size_t shownBefore);

const char* rml();
// The window's variables. Its events are the caller's: sec_go(i), set_toggle(i), set_slider(i), set_step(i, by),
// set_choice(i, j), set_more(i), open_console(), opt_close()
void bind(Rml::DataModelConstructor& c, Model& m);

}  // namespace uagfx::uikit::options
