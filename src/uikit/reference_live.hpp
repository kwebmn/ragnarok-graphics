#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/reference_pages.hpp"

namespace uagfx::uikit::reference {

// 3.0.18-beta.239 (docs/superpowers/plans/2026-11-13-reference-rewrite.md, the owner's concept of 13.11): «Довідник»
// a live window of the new interface. The chrome (the header with «‹ Назад», «Головна», the title and its way, the
// search field, the star, ✕; the rail of sections; the bottom's chips and keys) is the model's; the page's rows are
// markup made by body() - blocks in two columns (the next into the shorter one), a list of more than 20 rows across
// in two or three columns, one column in a narrow window; every element that leads somewhere has class «ctl» and
// data-ln="<the row's index>" (the window's click listener maps it to the row's link), a jump chip data-jump="r<i>".
struct RailV {
    Rml::String name, icon, count;
    bool on = false;
    bool operator==(const RailV& o) const;
};
// The site's live data in the bottom's chip: not asked for, fresh, no link; stale - came, but past its minutes
enum class Live : int { None, Fresh, Off, Stale };
struct Model {
    Rml::String title, back, query, find, keys, pack, live, star, home_icon, body, note, ground = "#10141cf0";
    Rml::String w = "1270px", h = "850px";  // the window's box (the window sets them from the screen and its corner)
    int live_state = 0;                     // Live: 0 none, 1 fresh, 2 off, 3 stale
    bool home = false, narrow = false, starred = false;  // star: "" none, «☆» or «★» (starred)
    std::vector<RailV> rail;
    std::vector<Rml::String> crumbs;
};
const char* rml();
// The model's names bound (the events are the window's: ref_back, ref_home, ref_star, ref_close, ref_rail, ref_chip)
void bind(Rml::DataModelConstructor& c, Model& m);

struct Look {
    bool narrow = false;
    bool wide = false;  // 3.0.18-beta.248: a body wide enough for a long list in three columns (else two)
    // A picture's source by its key ("" while it is not made yet): a Portrait's row.picture ("mob:1002",
    // "item:512", "map:prontera"), a row's icon ("icon:item:512", "icon:mob:1002"; "ui:<name>" the core's line icons),
    // "world" the world map's sheet at worldW x worldH
    std::function<std::string(const std::string& key)> picture;
    int worldW = 0, worldH = 0;  // the world map's box on its page, in the target's pixels
    int selected = -1;           // the row the arrows picked (class «sel»)
    // 3.0.18-beta.240 (the reference's «одна карта»): a location's map - its box, its layers' chips (name, on), the
    // words over it (made by the window); its picture is picture("mapview")
    int mapW = 0, mapH = 0;
    std::vector<std::pair<std::string, bool>> layers;
    std::string mapWords;
};
// The page's rows but its rail, as RML (UTF-8, every text escaped)
std::string body(const refpages::Page& p, const Look& look);
std::string escape(const std::string& utf8);
// 3.0.18-beta.241 (the concept's pad: the gold frame on rows): the controls the pad walks - the body's «ctl» elements
// shown (rows, folded headings, jump chips, the maps, their chips and tools), in the markup's order
std::vector<Rml::Element*> padTargets(Rml::Element* body);
// 3.0.18-beta.239: a section of more than kLong rows goes across the body in columns (two; three past kLonger)
constexpr size_t kLong = 20, kLonger = 60;

}  // namespace uagfx::uikit::reference
