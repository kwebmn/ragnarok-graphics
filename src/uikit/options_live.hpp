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
    Rml::String warn;  // 3.0.21-beta.4: what keeps the row from doing anything now, "" nothing
    bool toggle = false, on = false, slider = false;
    int value = 0, min = 0, max = 0, step = 1;
    std::vector<ChoiceView> choices;
    std::vector<int> sliders;  // 3.0.21-beta.8: one when the row is a slider (fit): its part is made only then
    bool more = false, sub = false;  // the line that opens a group's details or folds them; a part of the row over it
    int opens = 0;                   // more: the catalogue's group
};
struct NavView {
    Rml::String name;
    int count = 0;  // beside the name when over 0: its rows, or what the search found in it
};
struct Model {
    int sec = 0, shownRows = 0;  // the entry shown; its rows' count (0: «nothing found» in their place)
    Rml::String query, title, hint, searchHint, none, consoleBtn, ground = "#10141cf0";
    Rml::String layoutBtn;  // 3.0.21-beta.7: «Розмістити інтерфейс» (the console's action before)
    std::vector<NavView> nav;
    std::vector<RowView> rows;
};

// 3.0.21-beta.8 (R-SPPSF3: «Налаштування» open - the game from 222 frames a second to 136, to 49 on a long entry):
// the list is as long as the entry shown, a row's choices as many as it has. 3.0.18-beta.132 (R-4BXZPM
// «srows[1].choices[10].on») padded both - every row ever shown kept, each with twelve choices: 56 elements a row,
// 11 000 after a long entry, and RmlUi walks them all each frame. The padding hid a read past the end of a list just
// made shorter: RmlUi updates the views of one depth in no fixed order, and a list's own elements carried views
// (data-if, data-class) at the depth of the list's view. Now a list's elements are plain wrappers, their views a
// level down - updated after the list has let its spare elements go. fit(): each row's parts as its kind asks
void fit(std::vector<RowView>& rows);

// 3.0.21-beta.9 (R-MSAM96 «картинка сильно поменялась, не могу разобратся что включил»: «Яскравість» 100 -> 50,
// «Контраст» 108 -> 100, «Насиченість» 115 -> 100 written with no hand on them; R-SPPSF3: «Масштаб рендера» -> 50):
// a slider's «change» is the person's move only outside RmlUi's update (uikit/applying) and when it names another
// value than the row holds - true with that value (within the row's range). A row's slider is the element of the
// row that stood at its place before: the new row's least or most cut the value left in it, RmlUi told «change»,
// and the window wrote that value under the new row's name
bool sliderMoved(const Rml::Event& e, const Model& m, int row, int& value);

const char* rml();
// The window's variables. Its events are the caller's: sec_go(i), set_toggle(i), set_slider(i), set_step(i, by),
// set_choice(i, j), set_more(i), open_layout(), open_console(), opt_close()
void bind(Rml::DataModelConstructor& c, Model& m);

}  // namespace uagfx::uikit::options
