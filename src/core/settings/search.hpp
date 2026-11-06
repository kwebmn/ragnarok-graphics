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
// 3.0.11-beta.301 (the review «Ревизия конфигуратора»: «травы» found 4 of «трава»'s 13, «шрифт» none): the app's
// search - each word of the query by its stem (a word of 4 letters less its last, of 5 or more less its last two)
// or one of its synonyms («шрифт» - text, «плавность» - smoothing, «лаги» - frames and the connection)
bool queryMatches(const std::string& text, const std::string& query);
// 2.6.22: a row of the schema by its label, its hint and its name before (old_names) in a language - the app's search
// and the console's letters
bool settingMatches(const IniSetting& s, const std::string& query, Lang l);

enum class HitKind : uint8_t { Ini, Reg, Action };
struct Hit {
    HitKind kind;
    size_t index;  // into iniSettings(), regSettings() or actions()
    Place place;
    Mode level = Mode::Simple;  // .301: the mode that shows it (found from a simpler one too)
};

// A button or a part of a page worth finding by its name (making the pack, the backups, the gamepad...).
struct Action {
    Text label;
    Place place;
    Mode level;
    const char* words = "";  // .301: more words that find it, any language («оконный режим»)
};
const std::vector<Action>& actions();

// What a mode shows of the query's matches: in the order of the mode's list, then the schema's; at most 80; a query
// under two letters finds nothing. In the Simple mode the Main page's rows are found there. 3.0.11-beta.301: the
// labels, hints and old names in all three languages; the Detailed mode's rows found from the Simple one too (after
// its own, Hit::level telling the mode to switch to), the Developer's only in it.
std::vector<Hit> findSettings(const std::string& query, Lang l, Mode m);

}  // namespace uagfx::settings
