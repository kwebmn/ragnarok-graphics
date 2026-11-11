#pragma once
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace uagfx::uikit::social {

// 3.0.18-beta.87 (live windows, group 2): friends and the party a live window - tabs, rows and buttons by
// flexbox, RmlUi's clicks. 3.0.18-beta.141 (the owner: all ten): a person's row a card (face, class and level,
// HP, near or the map), quick buttons on hover, a right click (the pad's A) opens their menu under the row, the
// friends online first, the party's header (online of all, the experience, its settings), a friend added by name,
// the empty tabs saying what to do
struct RowV {
    Rml::String name, job, where, face, letter, q0, q1;
    int idx = 0, hp = -1;
    bool online = false, leader = false, dead = false;
    bool operator==(const RowV& o) const;
};
struct Model {
    std::vector<RowV> rows;
    std::vector<Rml::String> tabs, menu;
    Rml::String header, count, exp, settings, empty, addName, nameHint, add, hint, ground = "#10141cf0";
    int tab = 0, selected = -1, menuRow = -1, menuFocus = 0, padRow = -1;
};
const char* rml();
// The model's names bound (the events are the window's: so_tab, so_choose, so_down, so_whisper, so_quick, so_item,
// so_settings, so_add, so_close)
void bind(Rml::DataModelConstructor& c, Model& m);

}  // namespace uagfx::uikit::social
