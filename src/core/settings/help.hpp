#pragma once

#include <vector>

#include "core/settings/auto_setup.hpp"
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

}  // namespace uagfx::settings
