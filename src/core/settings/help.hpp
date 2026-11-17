#pragma once

#include <vector>

#include "core/settings/auto_setup.hpp"
#include "core/settings/catalogue.hpp"
#include "core/settings/navigation.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// One line of «Довідка»: a function's name and what it does (the name itself when its row has no hint)
struct HelpRow {
    Text label;
    Text hint;
};

// 3.0.2-beta.6 (the user: a description of every function, so that it is clear what each is): the functions a view
// shows in a mode, each once, in its page's order - the schema's rows and the ones the pages build themselves
std::vector<HelpRow> helpRows(View v, Mode mode, const Machine& machine);

// A view's one line of what it is for
Text helpOf(View v);

// 3.0.21-beta.6 (variant 4, step 4: the app holds what is set before the game starts, the rest lives in the game's
// «Налаштування»): the main of an area of the game's window - its basic rows, each once, in the catalogue's order;
// «Картинка» as the Effects page told it (the set, the clock, the groups). None for an area with nothing in the game
std::vector<HelpRow> helpRowsInGame(catalogue::Area area, const Machine& machine);
// An area's one line of what it is for; Text::Count - none
Text helpOfArea(catalogue::Area area);

}  // namespace uagfx::settings
