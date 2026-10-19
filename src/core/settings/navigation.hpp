#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/settings/schema.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// 2.6: the settings app's modes - each shows its own list of pages and the rows of its level and below; the rows it
// hides keep their values and their keys in the ini.
enum class Mode : uint8_t { Simple, Detailed, Developer };
constexpr int kModeCount = 3;

const char* modeToken(Mode m);  // [utility] mode: "simple", "detailed", "developer"
std::optional<Mode> parseMode(const std::string& token);  // any case
Text modeName(Mode m);
// The mode the app opens in: the one chosen in it, else Simple for a new install and Detailed for one the app ran in
// before (it holds the language or the statistics' answer).
Mode startMode(std::optional<Mode> saved, bool usedBefore);

// 2.6: an entry of the list on the left - one or more of the old pages (Page), Main a page of its own made of rows of
// the others.
enum class View : uint8_t { Main, Graphics, Screen, Effects, Ai, Sound, Pad, GameChat, Service, Debug };
constexpr int kViewCount = 10;

Text viewName(View v);
std::vector<View> viewsOf(Mode m);  // the list of a mode, in its order
bool developerView(View v);         // listed under "Developer only"
int tabCount(View v);               // the AI view's four, one elsewhere
Text tabName(View v, int tab);
std::vector<Page> partsOf(View v, int tab);  // the old pages a view's tab shows, in their order

struct Place {
    View view;
    int tab;
};
Place placeOf(Page p);  // where an old page is now (the AI page's "Make again" opens the pack's tab)
Place placeOf(const IniSetting& s);  // its page's, the AI models' on the tab of the models

// The lowest mode that shows a row of uaro-graphics.ini's schema.
Mode levelOf(const IniSetting& s);
bool mainKey(const std::string& sectionKey);  // a row the Main page shows (the Simple ones but the effects')
std::vector<std::string> simpleKeys();        // the rows of the Simple level
std::vector<std::string> developerKeys();     // the rows of the Developer level
// A view made of schema rows: "Reset the page to the recommended values" sets them.
bool resettable(View v, int tab);

}  // namespace uagfx::settings
