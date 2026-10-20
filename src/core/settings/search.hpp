#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/settings/navigation.hpp"

namespace uagfx::settings {

// 2.6: the settings app's search - the rows of uaro-graphics.ini's schema, of the game's key and a few buttons and
// pages, by their labels and hints in the app's language.

// Lower case of Latin, Russian and Ukrainian letters, ё read as е; '&' (a label's Alt letter) left out.
std::string foldCase(const std::string& utf8);
// Every word of the query is a part of the text, both folded.
bool textMatches(const std::string& text, const std::string& query);
// 2.6.22: a row of the schema by its label, its hint and its name before (old_names) in a language - the app's search
// and the console's letters
bool settingMatches(const IniSetting& s, const std::string& query, Lang l);

enum class HitKind : uint8_t { Ini, Reg, Action };
struct Hit {
    HitKind kind;
    size_t index;  // into iniSettings(), regSettings() or actions()
    Place place;
};

// A button or a part of a page worth finding by its name (making the pack, the backups, the gamepad...).
struct Action {
    Text label;
    Place place;
    Mode level;
};
const std::vector<Action>& actions();

// What a mode shows of the query's matches: in the order of the mode's list, then the schema's; at most 80; a query
// under two letters finds nothing. In the Simple mode the Main page's rows are found there.
std::vector<Hit> findSettings(const std::string& query, Lang l, Mode m);

}  // namespace uagfx::settings
