#pragma once
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::options {

// 3.0.18-beta.82 (item 4): the game's options a live window - its volumes the library's sliders, its switches
// ticks. 3.0.18-beta.122 (the concept «Ігровий інтерфейс», stages 5-6: «Налаштування» in place of Alt+O): a list on
// the left, a search over it all, each row a switch, a slider or choices; ours written into uaro-graphics.ini as the
// console writes them (win/setting_edit).
// 3.0.21-beta.3 (the concept «Настройки uaRO Graphics», the owner: «Вариант 4. Каждому месту своё дело», step 3):
// the player's one place in the game - the list's entries are the catalogue's areas, each with its rows' count; the
// pane an entry's groups one under another: a group's name over its first row, a part indented under its row, the
// group's details under a small heading (3.0.21-beta.11: at once - 3.0.21-beta.3's line that opened them is gone).
// Here its markup and what it shows (win/options_card fills it from core settingswin and takes its events); laid
// out by RmlUi in the host's tests too.
struct ChoiceView {
    Rml::String label;
    bool on = false;
    // 3.0.24-beta.19 (the themes): its colour - the accent's chips' dot, a palette's cell: "#rrggbb" as it is (the
    // theme never repaints a model's colour), never empty (an empty style value: "#00000000")
    Rml::String dot = "#00000000";
};
// 3.0.24-beta.2 (the simple mode): a capability card - its name, its words, its group's way in («Детально · N ›», ""
// none), its switch
struct CardView {
    Rml::String label, desc, more;
    bool on = false;
    Rml::String key;             // not shown: its row's key
    int entry = -1, group = -1;  // not shown: where «Детально» goes (the list's entry, the catalogue's group)
    // 3.0.24-beta.3: «Перевіряємо зараз» - a check's card: its switch only when it names one (`toggle`), «Працює ✓» and
    // «Є проблема — звіт» (`ok`, `bad`) under its steps; `id` not shown
    bool toggle = true, check = false;
    Rml::String ok, bad, id;
};
struct RowView {
    Rml::String key, group, label, hint, vtext;  // group: the group's name over this row (its first), "" none
    Rml::String warn;  // 3.0.21-beta.4: what keeps the row from doing anything now, "" nothing
    // 3.0.21-beta.10: on the row that heads its group - «Типові» when the group has rows off their defaults (a click:
    // set_reset(i)), "" none
    Rml::String reset;
    bool toggle = false, on = false, slider = false;
    int value = 0, min = 0, max = 0, step = 1;
    std::vector<ChoiceView> choices;
    // 3.0.21-beta.8: one when the row is a slider (fit): its part is made only then; 3.0.24-beta.19: the part's kind
    // (0 a slider, 1 the theme's pictures, 2 the accent's chips and palette)
    std::vector<int> sliders;
    // 3.0.21-beta.11 (the owner: «раскрытие в настройках лучше убрать … можно както выделить внутри что это
    // подгрупа»): no line that opens or folds - subhead: the small heading over a group's details (its words in
    // `label`, no control); det: a row of those details (set in, a rule on its left); sub: a part of the row over it
    bool subhead = false, det = false, sub = false;
    int groupIndex = 0;  // on the row that heads its group: the catalogue's group («Типові» brings it back)
    // 3.0.24-beta.2: one or two capability cards side by side in place of a row (`ncards` their count)
    std::vector<CardView> cards;
    int ncards = 0;
    // 3.0.24-beta.19 (the themes): inside the row's own part (`sliders`, fit(): 1 the pictures, 2 the accent) -
    // nothing more in the other rows (an empty list leaves an element of its own in each row, a hidden data-if one
    // its whole self): the choices as pictures of a window (interface.theme), as chips with their colour's dot
    // (interface.accent), «свій колір»'s palette under them - `cells` its 30, `pals` one when they are there (fit)
    std::vector<ChoiceView> swatches, accents, cells;
    std::vector<int> pals;
};
struct NavView {
    Rml::String name;
    int count = 0;  // beside the name when over 0: its rows, or what the search found in it
};
struct Model {
    int sec = 0, shownRows = 0;  // the entry shown; its rows' count (0: «nothing found» in their place)
    Rml::String query, title, hint, searchHint, none, consoleBtn, ground = "#10141cf0";
    Rml::String layoutBtn;  // 3.0.21-beta.7: «Розмістити інтерфейс» (the console's action before)
    Rml::String resetBtn;   // 3.0.24-beta.5: «Усе за замовчуванням» (pressed once: «Ще раз — скинути все»)
    Rml::String note;       // 3.0.21-beta.10: what the entry says over its rows («3D-режим» in the usual view), "" none
    // 3.0.24-beta.2: the simple page (no list on the left) or the full window (win/options_card sets it from the
    // file as the window opens); the switch's two words
    bool simple = false;
    Rml::String modeSimple, modeFull;
    Rml::String ownHex;  // 3.0.24-beta.19: «свій колір»'s field (typed in: set_own_hex())
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

// 3.0.21-beta.11: with no folding every entry is long, and RmlUi walks, clips and hands over every shown element
// each frame, in the pane's view or not. Called each frame after the layout: the rows further than a view's height
// from the pane's view are hidden from its drawing (visibility: their place kept, so the scroll's length stays),
// the rest shown; the property is set only when a row crosses that line. `all`: every row shown - for the frame the
// rows are told anew (the places are the layout's before: a row hidden by them could stand in the view now). The
// rows hidden
int cull(Rml::ElementDocument* doc, bool all = false);

const char* rml();
// The window's variables. Its events are the caller's: sec_go(i), set_toggle(i), set_slider(i), set_step(i, by),
// set_choice(i, j), set_reset(i), open_layout(), open_console(), opt_close(); 3.0.24-beta.2: set_mode(0 simple /
// 1 full), card_toggle(i, j), card_more(i, j); 3.0.24-beta.3: check_ok(i, j), check_bad(i, j); 3.0.24-beta.5:
// reset_all(); 3.0.24-beta.19: set_choice(i, j) from the pictures and the accent's chips too, set_colour(i, k) (the
// palette's cell k of row i), set_own_hex()
void bind(Rml::DataModelConstructor& c, Model& m);

}  // namespace uagfx::uikit::options
