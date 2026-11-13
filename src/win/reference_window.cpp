#include "win/code_page.hpp"
#include "win/reference_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/layout.hpp"
#include "core/minimap_proto.hpp"
#include "core/pack_source.hpp"
#include "core/pad_nav.hpp"
#include "core/reference_pages.hpp"
#include "core/reference_route.hpp"
#include "core/reference_skills.hpp"
#include "core/reference_world_paint.hpp"
#include "core/settings/strings.hpp"
#include "core/skill_info.hpp"
#include "core/sprite_file.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"
#include "core/walk_grid.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "uikit/reference_live.hpp"
#include "win/chat_window.hpp"
#include "win/client_tables.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/hotbar.hpp"
#include "win/info_card.hpp"
#include "win/item_lists.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/monster_picture.hpp"
#include "win/mvp_panel.hpp"
#include "win/osk_window.hpp"
#include "win/pack_pictures.hpp"
#include "win/party_panel.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/tracker.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/updates_hook.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::referencewin {
namespace {

// 3.0.18-beta.239 (docs/superpowers/plans/2026-11-13-reference-rewrite.md, the owner: «начинай делать в инлайн
// режиме, без остановки»): «Довідник» a live window of the new interface - uikit/reference_live's markup, laid out by
// RmlUi: moved by its ground, sized by its corner (its box, not a zoom: a narrow one stacks its blocks), a real
// scroll and a real search field. Pages: core/reference_pages; the world map's picture and words: core
// reference_window's painter; data: win/reference_load.
using settings::Text;
namespace ur = uikit::reference;

constexpr size_t kMostRecent = 8, kMostKept = 40;  // 3.0.11-beta.18: «Нещодавні», «Обране» at most
constexpr size_t kMostPictures = 2000;             // 3.0.11-beta.22: the cards' pictures and icons kept at most
constexpr float kWideW = 1270, kWideH = 850;       // the concept's window at 1080p, in dp
constexpr float kNarrowBelow = 900;                // dp: narrower - the rail its icons, the blocks one column
constexpr float kPanelW = 314;                     // dp: the world map's panel and its gap
constexpr uint32_t kBaseLevel = 0x771F94;          // 3.0.18-beta.242: the character's base level (CSession)

struct Entry {  // a page left for another: what «Назад» brings back
    refpages::Link at;
    std::string query, name;
    float scroll = 0;
    unsigned opened = 0;  // its long sections opened
    int sel = -1;         // 3.0.11-beta.28: the row that led away (picked again on the way back)
};

struct State {
    uilive::Window win;  // 3.0.18-beta.239
    ur::Model model;
    WindowShadow shadow;
    bool enabled = false, open = false, shown = false, wantPage = true;
    bool bodyDirty = false, bodyNow = false;  // the body's markup to make again (now: a new page)
    ULONGLONG bodyAt = 0;
    int applyIn = -1;     // frames until the new body is laid out: then its scroll and its picked row
    float scrollTo = 0;   // the scroll to set then
    bool listened = false;
    int markedSel = -1;   // the row whose element has «sel»
    int style = -1;       // 3.0.11-beta.47: core/ui_style's generation the ground was made for
    int iconsMade = -2;   // 3.0.11-beta.74: win/pack_pictures' iconsStamp() the pictures were made for
    WPARAM skipChar = 0;  // the character of the key that opened or closed it: neither typed nor the game's
    std::string query;    // typed
    refpages::Link at;    // the page: Go::None - the search's (the first screen when nothing is typed)
    unsigned opened = 0;  // the card's long sections opened (refpages::Part bits)
    std::vector<Entry> back;
    refpages::Page page;
    std::shared_ptr<const refindex::Index> index;
    std::shared_ptr<const reflive::Online> online;  // 3.0.11-beta.10: the players now (null: none fresh)
    std::shared_ptr<const reflive::Live> live;      // 3.0.11-beta.12: MVP timers, the market (null: none fresh)
    ULONGLONG madeAt = 0;                           // the page's minutes: made again each minute with timers
    std::shared_ptr<const reference::World> world;  // 3.0.11-beta.11: the world map (null: not made yet)
    panel::Picture worldPicture;                    // its picture at the map's box (its shapes)
    int worldPainted = 0;
    std::string worldSrc, worldKey, worldWordsRml;
    std::string worldHover;  // the map under the pointer there
    // 3.0.11-beta.29: the world map's view - its zoom and centre, the drag that moves it (a press not moved: a click)
    float zoom = 1, cx = -1, cy = -1;
    std::string viewOf;
    ULONGLONG worldMadeAt = 0;
    bool dragging = false, dragged = false;
    float dragX = 0, dragY = 0, dragCx = 0, dragCy = 0;
    int dragTile = -1, dragChain = -1;
    refpages::WorldWay way;
    std::map<std::string, panel::Picture> chainPictures;  // 3.0.11-beta.64: the dungeon box's minimaps
    int chainSide = 0, chainLogged = 0;
    std::vector<std::pair<std::string, std::string>> labels;  // the towns' names (made with the index)
    const refindex::Index* labelsOf = nullptr;
    std::unique_ptr<gamefiles::Reader> files;
    std::map<std::string, panel::Picture> pictures;  // 3.0.11-beta.14: the cards' pictures, icons; ui icons
    // the ones let go, kept a few frames: RmlUi reads a «pic:» source's pixels when it draws it the first time - the
    // markup that names them must be gone before they are
    std::vector<std::pair<int, std::map<std::string, panel::Picture>>> retired;
    std::map<std::string, std::string> sources;      // their sources given to RmlUi (the previous of each)
    unsigned folded = 0;  // 3.0.11-beta.17: the sections the player folded (refpages::Part bits), kept in a file
    bool foldedRead = false;
    std::vector<refpages::Link> recent, favourites;  // 3.0.11-beta.18
    int sel = -1;  // 3.0.11-beta.21: the row picked by the arrows (or the pad's D-pad), -1 none
    std::string here;
    int targetW = 0, targetH = 0;
    float k = 0, dp = 1;
    int w = 0, h = 0;  // the window's box, the target's pixels
    bool narrow = false;
    int mapW = 0, mapH = 0;  // the world map's box on its page
    std::vector<barhint::Skill> bar;  // 3.0.11-beta.68: the hotbar's skills, read with each page
    // 3.0.18-beta.240 (the reference's «одна карта», the owner 13.11): a location's map - the big map's look at any
    // zoom; its layers the minimap's (one setting), the monster zones the window's own
    std::string locCode;               // the map shown ("" none)
    minimap::MapView mv;               // its zoom and centre
    minimap::MapPicture locPicture;    // its picture: the game's, or «Схема»
    std::string locPictureOf;
    bool locSharp = false;             // «Схема»: zoom up to 4
    minimap::Scene locScene;
    std::vector<int> locNpc;           // each mark's NPC (its place in the map's list), -1 a warp
    std::vector<std::string> locDest;  // each mark's warp's map ("" an NPC)
    std::string locMembers;            // the party's members on it (their names, cells, faces) as last drawn
    panel::Picture locImage;
    int locPainted = 0, locHover = -1;
    std::string locSrc, locKey, locWordsRml;
    int locW = 0, locH = 0;            // its box
    bool layerMobs = true;             // the owner: all the layers on at first
    bool layerHere = true;             // «Ви тут»: the player and the party's faces (the window's own, as the zones)
    bool locDrag = false, locDragged = false;
    float locDragX = 0, locDragY = 0;
    int locDragMark = -1;
    // «Вести сюди» to an NPC's cell: the way to its map now, the cell once the player is on it
    std::string plotMap;
    int plotX = 0, plotY = 0;
    // 3.0.18-beta.241 (the concept's pad): the control its gold frame is on (its key - the element's id or data
    // attribute - on the page padPageOf; none: the first shown one), the cross on a map it took (pixels from the map's
    // corner), the keyboard typing into the search field
    bool padActive = false, padCross = false, crossWorld = false, oskOurs = false;
    std::string padKey, padPageOf, ringWas, crossWas;
    float crossX = 0, crossY = 0;
    // 3.0.18-beta.242 (the concept's cards): what the game says, read with each page - the bag's and the cart's counts
    // (known: the game has the list), the storage's as last seen, the character's level, the bosses' own timers
    std::map<int, int> bag, cart, storageSeen;
    bool bagKnown = false, cartKnown = false;
    int level = 0;
    std::vector<refpages::OwnTimer> own;
    // 3.0.18-beta.244 («Навички»): the site's skills' words (the watcher's file), read once a window's opening
    refskills::File skillWords;
    bool skillWordsRead = false;
    bool asTyped = false;  // 3.0.18-beta.245: the search's Cyrillic as typed («шукати як набрано»)
    // 3.0.18-beta.247 (R-ZW43KT «лагає довідник»: every few icons made the whole page's markup again - 29 ms a frame on
    // «Монстри»): the body's placeholders of icons not made yet (by their pictures' keys), looked for once a new body
    // is laid out; a few made a frame, those in view first, each put on its placeholders in place (a decorator)
    std::vector<std::pair<std::string, Rml::Element*>> waiting;
    int waitingIn = -1;  // frames until a new body's placeholders are looked for
    float waitingScroll = -1;  // the scroll the last look found none in view at: none looked for till it moves
};

State& state() {
    static State s;
    return s;
}

std::string say(Text t) { return settings::tr(language(), t); }

// 3.0.11-beta.14: a card's picture - a monster's standing frame from its sprite, an item's icon, a location's minimap
const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스

bool readFile(State& s, const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // 3.0.11-beta.15: as the game finds it (its zips too)
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!s.files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

// 3.0.11-beta.64: a map's minimap for the world map's dungeon box (the client's own picture), `side` square
const panel::Picture* chainPicture(State& s, const std::string& map, int side) {
    if (side != s.chainSide) {
        s.chainPictures.clear();
        s.chainSide = side;
    }
    auto it = s.chainPictures.find(map);
    if (it == s.chainPictures.end()) {
        std::string bytes;
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        const std::string entry = "data\\texture\\" + kUi + "\\map\\" + map + ".bmp";
        const bool read = readFile(s, entry, bytes) &&
                          pack::decodeBmp(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), argb, w, h);
        it = s.chainPictures.emplace(map, read ? worldmap::thumbnail(argb, w, h, side) : panel::Picture{}).first;
        if (s.chainLogged++ < 20)
            LOG_INFO("reference: the dungeon box's %s - %s", map.c_str(),
                     read ? "its minimap" : "no minimap in the game's files, the sea's colour");
    }
    return it->second.width > 0 ? &it->second : nullptr;
}

panel::Picture asPicture(const spritefile::Image& im) {
    panel::Picture p;
    p.width = im.w;
    p.height = im.h;
    p.px = im.argb;
    return p;
}

int px(float dp, const State& s) { return std::max(1, int(std::lround(dp * s.dp))); }

// The pictures let go (a new size, the pack's icons, too many): kept until a body without them is laid out
void retirePictures(State& s) {
    if (s.pictures.empty()) return;
    s.retired.emplace_back(8, std::move(s.pictures));
    s.pictures.clear();
    s.waiting.clear();  // 3.0.18-beta.247: the body is made again - its placeholders anew
    s.bodyNow = true;
}

// 3.0.11-beta.26: a row's «ui:<name>» icon (the tiles', the rail's, the chips' of a location and a quest)
bool iconNamed(const std::string& key, uiicons::Icon& icon) {
    static const std::pair<const char*, uiicons::Icon> kNames[] = {
        {"ui:monster", uiicons::Icon::Monster}, {"ui:item", uiicons::Icon::Item},
        {"ui:location", uiicons::Icon::Location}, {"ui:quest", uiicons::Icon::Quest},
        {"ui:npc", uiicons::Icon::Npc}, {"ui:mvp", uiicons::Icon::Mvp},
        {"ui:pet", uiicons::Icon::Pet}, {"ui:craft", uiicons::Icon::Craft},
        {"ui:world", uiicons::Icon::World}, {"ui:tables", uiicons::Icon::Tables},
        {"ui:skill", uiicons::Icon::Skill},  // 3.0.18-beta.244: «Навички»
        {"ui:search", uiicons::Icon::Search}, {"ui:dungeon", uiicons::Icon::Dungeon},
        {"ui:minus", uiicons::Icon::Minus}, {"ui:plus", uiicons::Icon::Plus},
        {"ui:expand", uiicons::Icon::Expand}, {"ui:home", uiicons::Icon::Home},
        {"ui:back", uiicons::Icon::Back}, {"ui:route", uiicons::Icon::Route},
    };
    for (const auto& [name, i] : kNames)
        if (key == name) {
            icon = i;
            return true;
        }
    return false;
}

void loadPicture(State& s, const std::string& key) {
    if (s.pictures.count(key) || !s.index) return;
    if (s.pictures.size() >= kMostPictures) retirePictures(s);  // 3.0.11-beta.22: a long browse never piles up
    panel::Picture& out = s.pictures[key];  // empty until found: not looked for again
    if (key.rfind("ui:", 0) == 0) {          // the core's line icon, painted at its size
        uiicons::Icon icon;
        if (!iconNamed(key, icon)) return;
        const int side = px(24, s);
        panel::blank(out, side, side);
        uiicons::draw(out, icon, 0, 0, float(side), 0xC9D1DE, 1.0f, 2.0f);
        return;
    }
    const bool mobIcon = key.rfind("icon:mob:", 0) == 0;  // 3.0.11-beta.26: a chip's monster, the icon's size
    const int side = mobIcon || key.rfind("icon:", 0) == 0 ? px(24, s) : px(104, s);
    if (key.rfind("mob:", 0) == 0 || mobIcon) {
        const auto m = s.index->mobs.find(std::atoi(key.c_str() + (mobIcon ? 9 : 4)));
        if (m == s.index->mobs.end()) return;
        std::string from;
        if (monsterPicture(m->second, side, out, &from)) {  // 3.0.11-beta.38: win/monster_picture
            LOG_INFO("reference: %s's picture - %s", m->second.name.c_str(), from.c_str());
            return;
        }
        LOG_INFO("reference: no sprite for %s (#%d) among %zu names", m->second.name.c_str(), m->first,
                 refindex::spriteNames(m->second).size());
        return;
    }
    if (key.rfind("map:", 0) == 0) {  // 3.0.11-beta.26: a location's - its minimap (the client's)
        std::string bmp;
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        if (!readFile(s, "data\\texture\\" + kUi + "\\map\\" + key.substr(4) + ".bmp", bmp) ||
            !pack::decodeBmp(reinterpret_cast<const uint8_t*>(bmp.data()), bmp.size(), argb, w, h))
            return;
        minimap::keyPink(argb);  // 3.0.18-beta.220: the game's magenta round it see-through
        spritefile::Image im;
        im.w = w;
        im.h = h;
        im.argb = std::move(argb);
        out = asPicture(spritefile::fitted(im, side, side));
        return;
    }
    // 3.0.18-beta.244 («Навички»): a skill's icon by its AEGIS name (the game's, as the hotbar's), a row's or the
    // card's
    const bool skillIcon = key.rfind("icon:skill:", 0) == 0;
    if (key.rfind("skill:", 0) == 0 || skillIcon) {
        const std::string aegis = key.substr(skillIcon ? 11 : 6);
        spritefile::Image im;
        if (aegis.empty() || !clienttables::iconPixels(hotbar::lowered(aegis), im.argb, im.w, im.h)) return;
        out = skillIcon ? asPicture(spritefile::fitted(im, side, side, 1))
                        : asPicture(spritefile::fitted(im, std::min(side, px(72, s)), std::min(side, px(72, s))));
        return;
    }
    const bool icon = key.rfind("icon:item:", 0) == 0;  // 3.0.11-beta.15: a row's icon, its own size
    if (key.rfind("item:", 0) == 0 || icon) {
        const auto& files = clienttables::itemFiles();  // 3.0.18-beta.217: the client tables' item -> icon
        const auto f = files.find(std::to_string(std::atoi(key.c_str() + (icon ? 10 : 5))));
        if (f == files.end()) return;
        spritefile::Image im;  // 3.0.11-beta.74: the AI texture pack's icon when it has one, else the game's
        if (!clienttables::iconPixels(f->second, im.argb, im.w, im.h)) return;
        out = icon ? asPicture(spritefile::fitted(im, side, side, 1))
                   : asPicture(spritefile::fitted(im, std::min(side, px(72, s)), std::min(side, px(72, s))));
    }
}

// A picture's source for the markup ("" while it is not made, or there is none): the body asks; the icons not made
// yet are made a few a frame and the body said again
std::string pictureSrc(State& s, const std::string& key) {
    if (key == "world") {
        if (s.worldPicture.width <= 0) return {};
        return uaui::picture(&s.worldPicture, s.worldPainted, s.worldSrc);
    }
    if (key == "mapview") {  // 3.0.18-beta.240: the location's map
        if (s.locImage.width <= 0) return {};
        return uaui::picture(&s.locImage, s.locPainted, s.locSrc);
    }
    auto it = s.pictures.find(key);
    if (it == s.pictures.end()) {
        if (key.rfind("icon:", 0) == 0) return {};  // a row's icon: its placeholder, made later (in view first)
        loadPicture(s, key);  // a portrait, a line icon: at once
        it = s.pictures.find(key);
        if (it == s.pictures.end()) return {};
    }
    if (it->second.width <= 0) return {};
    return uaui::picture(&it->second, uaui::stamp(&it->second), s.sources[key]);
}

// What a page is made with: the player's place, the minimap's way, the live data, the world map
// 3.0.18-beta.242: the game's things for the cards - the bag, the cart, the storage (kept as last seen), the level,
// the HUD «Боси» panel's timers
void readGameThings(State& s) {
    s.bag = itemlists::counts(itemlists::List::Bag, &s.bagKnown);
    s.cart = itemlists::counts(itemlists::List::Cart, &s.cartKnown);
    if (std::map<int, int> seen = itemlists::counts(itemlists::List::Storage); !seen.empty())
        s.storageSeen = std::move(seen);
    uint32_t level = 0;
    s.level = readGame(kBaseLevel, &level, 4) && level > 0 && level < 1000 ? int(level) : 0;
    s.own.clear();
    for (const mvptimers::Timer& t : mvpwin::timers()) s.own.push_back(refpages::OwnTimer{t.mob, t.map, t.back()});
    if (!s.skillWordsRead) {  // 3.0.18-beta.244
        s.skillWordsRead = true;
        std::string text;
        refskills::File f;
        if (readTextFile(layout::join(gameDir(), layout::kReferenceSkills), text) && refskills::read(text, f))
            s.skillWords = std::move(f);
        LOG_INFO("reference: the site's skills' words - %zu (uk), %zu (ru), %zu (en)",
                 s.skillWords.words[int(settings::Lang::Uk)].size(), s.skillWords.words[int(settings::Lang::Ru)].size(),
                 s.skillWords.words[int(settings::Lang::En)].size());
    }
}

refpages::Context contextOf(const State& s) {
    refpages::Context c;
    c.here = s.here;
    int w = 0, h = 0;  // 3.0.11-beta.8: the routes start from the part of the map one is in
    if (!readPlayerCell(c.x, c.y, w, h)) c.x = c.y = -1;
    c.follow.can = minimapproto::canFollow();  // 3.0.11-beta.9: «вести ›» with the new minimap on
    minimapproto::following(c.follow.map, c.follow.walk);
    if (s.online) c.players = &s.online->maps;  // 3.0.11-beta.10
    if (s.world) c.world = &s.world->sheet;     // 3.0.11-beta.11
    c.hover = s.worldHover;
    c.live = s.live.get();  // 3.0.11-beta.12
    c.now = static_cast<long long>(std::time(nullptr));
    c.folded = s.folded;  // 3.0.11-beta.17
    c.recent = &s.recent;  // 3.0.11-beta.18
    c.favourites = &s.favourites;
    c.bar = &s.bar;        // 3.0.11-beta.68
    c.liveAt = reference::onlineSeen();  // 3.0.11-beta.69
    c.trackedQuests = &trackerwin::quests();  // 3.0.11-beta.70
    c.trackedItems = &trackerwin::items();
    c.describe = [](int id) { return infocardwin::itemDescription(id); };  // 3.0.18-beta.242
    c.bag = s.bagKnown ? &s.bag : nullptr;
    c.cart = s.cartKnown ? &s.cart : nullptr;
    c.storage = s.storageSeen.empty() ? nullptr : &s.storageSeen;
    c.level = s.level;
    c.own = &s.own;
    // 3.0.18-beta.244: a skill's words - the site's in the player's language, else the game's own table's
    c.skillText = [&words = s.skillWords](int id) {
        std::vector<std::string> lines = refskills::lines(words, language(), id);
        if (lines.empty()) lines = infocardwin::skillDescription(skillinfo::aegis(id), skillinfo::name(id));
        return lines;
    };
    c.asTyped = s.asTyped;  // 3.0.18-beta.245
    return c;
}

bool worldPage(const State& s) {
    for (const refpages::Row& r : s.page.rows)
        if (r.style == refpages::Style::World) return true;
    return false;
}

// 3.0.11-beta.26: the chips' and the way's words measured as drawWords draws them
float measured(const std::string& utf8, float px, bool bold) { return float(wordsWidth(utf8Wide(utf8), px, bold)); }

refwin::Layout worldLayout(const State& s) {  // the world map's painter's scale: the document's dp
    refwin::Layout L;
    L.s = s.dp;
    L.measure = measured;
    return L;
}

refwin::WorldLook worldLook(State& s, bool forPaint) {
    refwin::WorldLook w;
    if (!s.world || !s.index) return w;
    w.sheet = &s.world->sheet;
    w.zoom = s.zoom;
    w.cx = s.cx;
    w.cy = s.cy;
    const int side = refwin::chainSide(worldLayout(s));
    for (const auto& [map, name] : s.way.chain)  // 3.0.11-beta.64: a dungeon's way - its maps in a box
        w.chain.push_back(refwin::ChainMap{map, name, forPaint ? chainPicture(s, map, side) : nullptr});
    w.chainTitle = s.way.chainTitle;
    if (!forPaint) return w;
    w.picture = &s.worldPicture;
    w.here = s.here;
    w.hover = s.worldHover;
    if (s.at.go == refpages::Go::World) w.marks = refpages::worldMarks(*s.index, s.at.map);  // 3.0.11-beta.19
    if (s.online) w.players = &s.online->maps;
    int x = -1, y = -1, mw = 0, mh = 0;
    const bool cell = readPlayerCell(x, y, mw, mh);
    if (const worldmap::Tile* t = worldmap::tileOf(s.world->sheet, s.here); t && cell)
        worldmap::cellOnSheet(*t, mw, mh, x, y, w.hereX, w.hereY);
    w.hops = s.way.hops;
    w.target = s.way.target;
    w.targetLabel = s.way.targetLabel;
    w.hereLabel = s.way.hereLabel;
    if (s.labelsOf != s.index.get()) {
        s.labels = refpages::worldLabels(*s.index);
        s.labelsOf = s.index.get();
    }
    w.labels = s.labels;
    if (!s.worldHover.empty()) w.tip = refpages::worldTip(*s.index, language(), s.worldHover, contextOf(s));
    return w;
}

// The world map's words over its picture as markup (spans placed in its box)
std::string worldWordsRml(const State& s, const refwin::WorldLook& w);

// 3.0.11-beta.29: the world map's picture at the view - painted again when what it shows changed; its element's
// picture and words then put in place (not the whole body)
void worldPaint(State& s) {
    if (!worldPage(s) || !s.world || s.mapW <= 0 || s.mapH <= 0) return;
    const refwin::WorldLook w = worldLook(s, true);
    char head[200];
    std::snprintf(head, sizeof head, "%d %d %.1f %.1f %.4f %.3f %.3f %.3f|", s.mapW, s.mapH, double(w.hereX),
                  double(w.hereY), double(w.zoom), double(w.cx), double(w.cy), double(s.dp));
    std::string key = head + w.here + '|' + w.target + '|' + w.hover + '|';
    for (const auto& hop : w.hops) key += hop.from + '>' + hop.to + (hop.jump ? "j" : "") + hop.label + ';';
    if (w.players)
        for (const auto& [m, n] : *w.players) key += m + '=' + std::to_string(n) + ',';
    for (const auto& [m, n] : w.marks) key += m + '#' + std::to_string(n) + ',';
    for (const std::string& l : w.tip) key += l + '\n';
    for (const auto& c : w.chain) key += c.map + (c.picture ? "+" : "-");
    key += std::to_string(uistyle::generation());
    if (key == s.worldKey) return;
    s.worldKey = std::move(key);
    refwin::paintWorldPicture(s.worldPicture, worldLayout(s), panel::Box{0, 0, float(s.mapW), float(s.mapH)}, w);
    ++s.worldPainted;
    s.worldMadeAt = GetTickCount64();
    s.worldWordsRml = worldWordsRml(s, w);
    if (Rml::Element* map = s.win.doc.byId("wmap"); map && !s.bodyDirty) {
        const std::string src = pictureSrc(s, "world");
        char size[64];
        std::snprintf(size, sizeof size, "width: %dpx; height: %dpx;", s.mapW, s.mapH);
        map->SetInnerRML("<img id=\"wimg\" src=\"" + ur::escape(src) + "\" style=\"" + size + "\"/>" +
                         s.worldWordsRml);
    }
}

// The view moved or zoomed: its centre kept where the view puts it, the picture made again
void worldChanged(State& s) {
    if (!s.world || s.mapW <= 0) return;
    const worldmap::Sheet& sh = s.world->sheet;
    const worldmap::Fit v = worldmap::view(sh.w, sh.h, s.mapW, s.mapH, s.zoom, s.cx, s.cy);
    if (s.zoom > 1 && v.s > 0) {
        s.cx = (float(s.mapW) / 2 - v.x) / v.s;
        s.cy = (float(s.mapH) / 2 - v.y) / v.s;
    }
    worldPaint(s);
}

// --- 3.0.18-beta.240 (the reference's «одна карта»): a location's map

bool wrapOf(const refwin::Words& w) { return w.wrap; }
bool wrapOf(const minimap::Words&) { return false; }

// Words of a painter (the world map's, the minimap's) as spans placed over its picture
template <typename W>
std::string spansOf(const std::vector<W>& words) {
    std::string o;
    char style[256];
    for (const W& x : words) {
        const char* align = x.align == decltype(x.align)::Right ? "right" : x.align == decltype(x.align)::Center
                                                                                  ? "center"
                                                                                  : "left";
        const bool wrap = wrapOf(x);
        std::snprintf(style, sizeof style,
                      "left: %.1fpx; top: %.1fpx; width: %.1fpx; height: %.1fpx; font-size: %.1fpx; "
                      "line-height: %.1fpx; color: #%06x; text-align: %s;",
                      double(x.box.x), double(x.box.y), double(x.box.w), double(x.box.h), double(x.px),
                      double(wrap ? std::round(x.px * 1.3f) : x.box.h), unsigned(x.rgb & 0xFFFFFF), align);
        o += std::string("<span class=\"ww") + (x.bold ? " b" : "") + (wrap ? " wrap" : "") + "\" style=\"" + style +
             "\">" + ur::escape(x.text) + "</span>";
    }
    return o;
}

std::string worldWordsRml(const State& s, const refwin::WorldLook& w) {
    return spansOf(refwin::worldWordsIn(worldLayout(s), w, panel::Box{0, 0, float(s.mapW), float(s.mapH)}));
}

bool locationPage(const State& s) {
    for (const refpages::Row& r : s.page.rows)
        if (r.style == refpages::Style::MapView) return true;
    return false;
}

std::string locationOf(const State& s) {
    for (const refpages::Row& r : s.page.rows)
        if (r.style == refpages::Style::MapView) return r.left;
    return {};
}

const walkgrid::Grid* gridOf(const State& s, const std::string& code) {
    if (!s.index || !s.index->walk) return nullptr;
    const auto it = s.index->walk->find(code);
    return it == s.index->walk->end() ? nullptr : &it->second;
}

// Its picture: «Схема» when the minimap shows it (the walkable cells), else the game's own (its warps' red dots out)
void loadLocationPicture(State& s, const std::string& code) {
    const walkgrid::Grid* g = gridOf(s, code);
    const bool scheme = minimapproto::schemeShown() && g;
    const std::string key = code + (scheme ? "|scheme" : "|picture");
    if (key == s.locPictureOf) return;
    s.locPictureOf = key;
    s.locPicture = minimap::MapPicture{};
    s.locSharp = scheme;
    if (scheme) {
        int w = 0, h = 0;
        const std::vector<uint32_t> px = minimap::schemePixels(*g, nullptr, 4, w, h);
        s.locPicture = minimap::prepare(px, w, h);
        return;
    }
    std::string bmp;
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    if (readFile(s, "data\\texture\\" + kUi + "\\map\\" + code + ".bmp", bmp) &&
        pack::decodeBmp(reinterpret_cast<const uint8_t*>(bmp.data()), bmp.size(), argb, w, h)) {
        minimap::dropWarpDots(argb, w, h);
        s.locPicture = minimap::prepare(argb, w, h);
    }
    LOG_INFO("reference: %s's map - %s", code.c_str(), s.locPicture.levels.empty() ? "no picture (the ground)"
                                                                                       : "the game's picture");
}

int memberSide(const State& s) { return std::max(8, int(std::lround(12 * s.dp))); }

std::string membersKeyOf(const partywin::OnMap& p) {
    char face[24];
    std::snprintf(face, sizeof face, "%p", static_cast<const void*>(p.face));
    return p.name + ':' + std::to_string(p.x) + ',' + std::to_string(p.y) + (p.dead ? "d" : "") + face + ';';
}

// Its marks by the layers: the NPCs (their colours, «K» «$» «!»), the warps (the exits' words always), the monster
// zones, the player and the party's members on his own map
void buildLocationScene(State& s) {
    minimap::Scene sc;
    s.locNpc.clear();
    s.locDest.clear();
    if (const walkgrid::Grid* g = gridOf(s, s.locCode)) sc.map = minimap::MapSize{g->w, g->h};
    sc.picture = s.locPicture.levels.empty() ? nullptr : &s.locPicture;
    const bool npcs = minimapproto::layerShown(int(minimap::Layer::Npcs)) ||
                      minimapproto::layerShown(int(minimap::Layer::Kafra));
    const bool warps = minimapproto::layerShown(int(minimap::Layer::Warps));
    const bool here = s.layerHere;
    const refindex::MapHints* hints = nullptr;
    if (s.index)
        if (const auto it = s.index->maps.find(s.locCode); it != s.index->maps.end()) hints = &it->second;
    if (hints) {
        const refindex::MapHints& h = *hints;
        if (npcs)
            for (size_t i = 0; i < h.npcs.size(); ++i) {
                const refindex::Npc& n = h.npcs[i];
                const bool kafra = n.kind == refindex::NpcKind::Kafra;
                minimap::Mark m{minimap::Cell{n.x, n.y}, refindex::colorOf(n.kind),
                                kafra ? minimap::Glyph::Kafra : minimap::Glyph::Dot, n.name};
                if (!kafra) m.letter = refindex::letterOf(n.kind);
                sc.marks.push_back(std::move(m));
                s.locNpc.push_back(int(i));
                s.locDest.emplace_back();
            }
        if (warps)
            for (const refindex::Warp& w : h.warps) {
                bool twin = false;  // the same warp's second cell beside it
                for (size_t k = 0; k < sc.marks.size(); ++k)
                    twin = twin || (sc.marks[k].glyph == minimap::Glyph::Warp &&
                                    std::abs(sc.marks[k].cell.x - w.x) <= 2 && std::abs(sc.marks[k].cell.y - w.y) <= 2);
                if (twin) continue;
                minimap::Mark m{minimap::Cell{w.x, w.y}, 0, minimap::Glyph::Warp,
                                "\xE2\x86\x92 " + refindex::mapTitle(*s.index, w.dest)};
                m.exit = w.dest.find("_in") == std::string::npos;  // indoors: its words under the pointer only
                sc.marks.push_back(std::move(m));
                s.locNpc.push_back(-1);
                s.locDest.push_back(w.dest);
            }
        if (s.layerMobs)
            for (const refindex::Zone& z : h.zones)
                sc.zones.push_back(minimap::ZoneMark{minimap::Cell{z.x, z.y}, z.rx, z.ry});
    }
    int x = -1, y = -1, w = 0, hh = 0;
    sc.showPlayer = here && s.locCode == s.here && readPlayerCell(x, y, w, hh);
    if (sc.showPlayer) sc.player = minimap::Cell{x, y};
    s.locMembers.clear();
    if (here && s.locCode == s.here)  // the party's members on it, their faces (as the minimap's), after the rest
        for (const partywin::OnMap& p : partywin::onMap(memberSide(s))) {
            minimap::Mark m{minimap::Cell{p.x, p.y}, 0, minimap::Glyph::Member, p.name};
            m.picture = p.face;
            m.dead = p.dead;
            sc.marks.push_back(std::move(m));
            s.locNpc.push_back(-1);
            s.locDest.emplace_back();
            s.locMembers += membersKeyOf(p);
        }
    sc.hoverMark = s.locHover;
    s.locScene = std::move(sc);
}

// Painted again when what it shows changed; its element's picture and words then put in place (not the whole body)
void locationPaint(State& s) {
    if (s.locCode.empty() || s.locW <= 0 || s.locH <= 0) return;
    s.mv.w = s.locW;
    s.mv.h = s.locH;
    s.mv.s = s.dp;
    s.locScene.hoverMark = s.locHover;
    char head[200];
    std::snprintf(head, sizeof head, "%d %d %.4f %.3f %.3f %.3f %d %d %d %zu %zu %d|", s.locW, s.locH,
                  double(s.mv.zoom), double(s.mv.cx), double(s.mv.cy), double(s.dp), s.locHover,
                  s.locScene.showPlayer ? s.locScene.player.x : -1, s.locScene.showPlayer ? s.locScene.player.y : -1,
                  s.locScene.marks.size(), s.locScene.zones.size(), uistyle::generation());
    const std::string key = head + s.locCode + "|" + s.locPictureOf;
    if (key == s.locKey) return;
    s.locKey = key;
    minimap::paintMapView(s.locImage, s.mv, s.locScene);
    ++s.locPainted;
    s.locWordsRml = spansOf(minimap::mapViewWords(s.mv, s.locScene));
    if (Rml::Element* view = s.win.doc.byId("mview"); view && !s.bodyDirty) {
        const std::string src = uaui::picture(&s.locImage, s.locPainted, s.locSrc);
        char size[64];
        std::snprintf(size, sizeof size, "width: %dpx; height: %dpx;", s.locW, s.locH);
        view->SetInnerRML("<img id=\"mimg\" src=\"" + ur::escape(src) + "\" style=\"" + size + "\"/>" +
                          s.locWordsRml);
    }
}

// The map shown on this page: its picture and marks; a new one at its whole, an NPC's card's «Показати на карті» on
// that NPC at twice the zoom
void locationSetUp(State& s) {
    const std::string code = locationOf(s);
    if (code.empty()) {
        s.locCode.clear();
        return;
    }
    const bool fresh = code != s.locCode;
    s.locCode = code;
    loadLocationPicture(s, code);
    if (fresh) {
        s.mv = minimap::MapView{};
        s.locHover = -1;
    }
    buildLocationScene(s);
    if (s.at.go == refpages::Go::Map && s.at.id > 0) {  // its NPC in the middle, marked
        for (size_t i = 0; i < s.locNpc.size(); ++i)
            if (s.locNpc[i] == s.at.id - 1) {
                const minimap::Cell c = s.locScene.marks[i].cell;
                s.mv.w = s.locW, s.mv.h = s.locH, s.mv.s = s.dp;
                s.mv.zoom = std::min(2.0f, minimap::mapViewMost(s.locSharp));
                s.mv.cx = float(c.x) + 0.5f;
                s.mv.cy = float(c.y) + 0.5f;
                s.locHover = int(i);
                break;
            }
    }
    s.locKey.clear();
    locationPaint(s);
}

// 3.0.11-beta.26: the words around the page - the title, «Назад до …», the way here, the data's chips, the keys
// 3.0.18-beta.241: the bottom line while the pad has the window - its buttons by the controller's names
std::string padKeysLine(const State& s) {
    std::vector<std::string> names;
    for (size_t i = 0; i < 6; ++i) names.push_back(uapad::buttonLabel(i));  // A B X Y LB RB
    const Text t = !s.padCross ? Text::RefPadKeys : s.crossWorld ? Text::RefPadWorldKeys : Text::RefPadCrossKeys;
    return settings::fmt(say(t), names);
}

void fillChrome(State& s) {
    const settings::Lang l = language();
    ur::Model& m = s.model;
    m.title = say(Text::RefTitle);
    m.back = s.back.empty() ? "" : settings::fmt(settings::tr(l, Text::RefBack), {s.back.back().name});
    m.crumbs.clear();
    for (const Entry& e : s.back)
        if (!e.name.empty() && e.name != m.title) m.crumbs.push_back(e.name);
    if (!s.back.empty() && !s.page.name.empty()) m.crumbs.push_back(s.page.name);
    m.find = say(Text::RefFind);
    m.home = s.at.go != refpages::Go::None || !s.query.empty();  // 3.0.11-beta.30: «Головна» but on it
    m.home_icon = pictureSrc(s, "ui:home");
    m.star = s.page.star < 0 ? "" : s.page.star > 0 ? "\xE2\x98\x85" : "\xE2\x98\x86";  // ★ ☆
    m.starred = s.page.star > 0;
    m.rail.clear();
    for (size_t i = 0; i < s.page.rail.size(); ++i) {
        const refpages::Row& r = s.page.rail[i];
        m.rail.push_back(ur::RailV{r.left, r.icon.empty() ? "" : pictureSrc(s, r.icon), r.right,
                                   int(i) == s.page.railOn});
    }
    const long long now = static_cast<long long>(std::time(nullptr));
    m.pack.clear();
    if (s.index && refpages::packStale(*s.index, now))  // 3.0.11-beta.69: past 14 days - it may be out of date
        m.pack = settings::fmt(settings::tr(l, Text::RefFooterStale), {s.index->date.substr(0, 10)});
    else if (s.index)
        m.pack = settings::fmt(settings::tr(l, Text::RefFooter),
                               {s.index->date.substr(0, 10), s.index->commit.substr(0, 8)});
    if (s.online) {  // 3.0.11-beta.20: how fresh the players now (and the timers, the market) are
        const long long age = std::max<long long>(0, now - s.online->at);
        m.live = settings::fmt(settings::tr(l, Text::RefLiveNow),
                               {refpages::number(s.online->online, l),
                                refpages::respawn(int(std::max<long long>(age, 60) * 1000), l)});
        m.live_state = int(ur::Live::Fresh);
    } else if (const long long seen = reference::onlineSeen(); seen > 0) {  // 3.0.11-beta.69: came, now stale
        const long long age = std::min<long long>(std::max<long long>(now - seen, 60), 2000000);
        m.live = settings::fmt(settings::tr(l, Text::RefLiveStale), {refpages::respawn(int(age * 1000), l)});
        m.live_state = int(ur::Live::Stale);
    } else {
        m.live = say(Text::RefLiveOff);
        m.live_state = int(ur::Live::Off);
    }
    const bool card = s.at.go == refpages::Go::Mob || s.at.go == refpages::Go::Item || s.at.go == refpages::Go::Map ||
                      s.at.go == refpages::Go::Quest;
    m.keys = say(card                                                 ? Text::RefKeysCard
                 : s.at.go == refpages::Go::World && s.at.id == 0 ? Text::RefKeysWorld  // 3.0.11-beta.29
                                                                  : Text::RefKeysList);
    if (s.padActive || s.padCross) m.keys = padKeysLine(s);  // 3.0.18-beta.241
    m.ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    m.narrow = s.narrow;
    // not the field's words: said again while one types, they would move its caret
    for (const char* v : {"title", "back", "crumbs", "find", "home", "home_icon", "star", "starred", "rail", "pack",
                          "live", "live_state", "keys", "ground", "narrow"})
        s.win.doc.dirty(v);
}

// The body's markup again (a new page, the pictures that came, the narrow look)
void renderBody(State& s) {
    const ULONGLONG t0 = GetTickCount64();
    ur::Look look;
    look.narrow = s.narrow;
    look.selected = s.sel;
    look.worldW = s.mapW;
    look.worldH = s.mapH;
    look.picture = [&s](const std::string& key) { return pictureSrc(s, key); };
    if (locationPage(s)) {  // 3.0.18-beta.240: the location's map, its layers' chips
        look.mapW = s.locW;
        look.mapH = s.locH;
        look.mapWords = s.locWordsRml;
        const bool npcs = minimapproto::layerShown(int(minimap::Layer::Npcs)) ||
                          minimapproto::layerShown(int(minimap::Layer::Kafra));
        look.layers = {{say(Text::RefLayerNpc), npcs},
                       {say(Text::RefLayerWarps), minimapproto::layerShown(int(minimap::Layer::Warps))},
                       {say(Text::RefLayerMobs), s.layerMobs},
                       {say(Text::RefLayerHere), s.layerHere}};
    }
    std::string rml = ur::body(s.page, look);
    if (worldPage(s) && !s.worldWordsRml.empty()) {  // the world map's words inside its box
        const size_t at = rml.find("<div id=\"wmap\"");
        const size_t end = at == std::string::npos ? at : rml.find("</div>", at);
        if (end != std::string::npos) rml.insert(end, s.worldWordsRml);
    }
    s.model.body = rml;
    s.win.doc.dirty("body");
    s.waiting.clear();  // 3.0.18-beta.247: the old body's elements go
    s.waitingIn = 2;
    s.waitingScroll = -1;
    s.bodyDirty = s.bodyNow = false;
    s.bodyAt = GetTickCount64();
    s.markedSel = s.sel;
    static int told = 0;
    if (told++ < 40)
        LOG_INFO("reference: «%s» - %zu rows, %zu bytes of markup in %llu ms", s.page.name.c_str(),
                 s.page.rows.size(), rml.size(), (unsigned long long)(GetTickCount64() - t0));
}

void makePage(State& s) {
    const settings::Lang l = language();
    if (!s.index) {
        s.page = refpages::Page{};
        s.page.name = say(Text::RefTitle);
        refpages::Row r;
        r.style = refpages::Style::Note;
        r.left = say(Text::RefNoData);
        s.page.rows.push_back(r);
    } else {
        s.bar = hotbarwin::barSkills();  // 3.0.11-beta.68: a monster's card says the bar's skills on it
        readGameThings(s);                // 3.0.18-beta.242
        const refpages::Context c = contextOf(s);
        s.page = s.at.go != refpages::Go::None ? refpages::open(*s.index, l, s.at, c, s.opened)
                 : !s.query.empty()            ? refpages::found(*s.index, l, s.query, c, s.opened)
                                               : refpages::home(*s.index, l, c);
    }
    const bool first = s.at.go == refpages::Go::None && s.query.empty();  // 3.0.11-beta.26: the concept's rail
    if (s.index && !first && s.at.go != refpages::Go::World) s.page = refpages::withRail(s.page, *s.index, l, s.at);
    s.madeAt = GetTickCount64();
    if (s.sel >= int(s.page.rows.size()) || (s.sel >= 0 && s.page.rows[size_t(s.sel)].link.go == refpages::Go::None))
        s.sel = -1;
    if (s.sel < 0 && s.at.go == refpages::Go::None && !s.query.empty())  // 3.0.11-beta.28: the first result picked
        for (size_t i = 0; i < s.page.rows.size(); ++i)
            if (s.page.rows[i].style == refpages::Style::Result) {
                s.sel = int(i);
                break;
            }
    if (worldPage(s) && s.world && s.index) {  // 3.0.11-beta.11: the world's picture, its way and its view
        const refpages::Context c = contextOf(s);
        s.way = refpages::worldWayOf(*s.index, l, c, s.at.map);
        const std::string key = refpages::linkText(s.at) + "|" + std::to_string(s.at.id) + "|" + s.at.map + "|" +
                                std::to_string(s.mapW) + "x" + std::to_string(s.mapH);
        if (key != s.viewOf) {  // a page of it opened: the view on its way or its marks, else the whole world
            s.viewOf = key;
            s.zoom = 1;
            s.cx = s.cy = -1;
            float vl = 0, vt = 0, vr = 0, vb = 0;
            const worldmap::Sheet& sh = s.world->sheet;
            if (refpages::worldFocus(sh, s.way, refpages::worldMarks(*s.index, s.at.map), vl, vt, vr, vb))
                worldmap::viewOn(sh.w, sh.h, s.mapW, s.mapH, vl, vt, vr, vb, s.zoom, s.cx, s.cy);
        }
        s.worldKey.clear();
        worldPaint(s);
    }
    locationSetUp(s);  // 3.0.18-beta.240
    s.wantPage = false;
    s.bodyDirty = s.bodyNow = true;
    fillChrome(s);
}

void close(State& s);

// 3.0.11-beta.18: a kept list of cards (one a line: refpages::linkText) - read once, written when it changes
void keepList(const std::vector<refpages::Link>& list, const char* file) {
    std::string text;
    for (const refpages::Link& k : list) text += refpages::linkText(k) + "\n";
    const std::string path = layout::join(gameDir(), file);
    if (!writeFileAtomic(path, text)) LOG_INFO("reference: %s cannot be written", path.c_str());
}

void readList(std::vector<refpages::Link>& list, const char* file) {
    std::string text;
    if (!readTextFile(layout::join(gameDir(), file), text)) return;
    size_t at = 0;
    while (at < text.size() && list.size() < kMostKept) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        refpages::Link k;
        if (refpages::linkOf(line, k)) list.push_back(k);
    }
}

bool isCard(const refpages::Link& k) {
    return k.go == refpages::Go::Mob || k.go == refpages::Go::Item || k.go == refpages::Go::Map ||
           k.go == refpages::Go::Quest;
}

void noteRecent(State& s) {  // the card opened now: first of «Нещодавні»
    if (!isCard(s.at)) return;
    s.recent.erase(std::remove_if(s.recent.begin(), s.recent.end(),
                                  [&](const refpages::Link& k) { return refpages::sameCard(k, s.at); }),
                   s.recent.end());
    s.recent.insert(s.recent.begin(), s.at);
    if (s.recent.size() > kMostRecent) s.recent.resize(kMostRecent);
    keepList(s.recent, layout::kReferenceRecent);
}

Rml::Element* scroller(State& s) { return s.win.doc.byId("scroll"); }
float scrollNow(State& s) {
    Rml::Element* e = scroller(s);
    return e ? e->GetScrollTop() : 0;
}

Rml::Element* field(State& s) { return s.win.doc.byId("rq"); }
bool fieldFocused(State& s) {
    Rml::Element* f = field(s);
    Rml::ElementDocument* d = s.win.doc.doc();
    if (!f || !d || !d->GetContext()) return false;
    return d->GetContext()->GetFocusElement() == f;
}
void blurField(State& s) {
    if (fieldFocused(s))
        if (Rml::Element* f = field(s)) f->Blur();
}

// The field's words said by the window (a page left, Esc) - the model and the window agree, nothing typed anew
void setQuery(State& s, const std::string& q) {
    s.query = q;
    s.model.query = q;
    s.win.doc.dirty("query");
}

// A new page: its top shown (or `scroll`, «Назад»'s) once its markup is laid out
void newPage(State& s, float scroll) {
    s.scrollTo = scroll;
    s.applyIn = 2;
}

void go(State& s, const refpages::Link& to) {
    if (to.go == refpages::Go::AsTyped) {  // 3.0.18-beta.245: «шукати як набрано» - the Cyrillic itself
        s.asTyped = true;
        s.sel = -1;
        makePage(s);
        newPage(s, 0);
        return;
    }
    if (to.go == refpages::Go::Zoom) {  // 3.0.11-beta.29: the world map's «-», «+» round its middle, the whole
        if (!s.world || s.mapW <= 0) return;
        const worldmap::Sheet& sh = s.world->sheet;
        if (to.id == 0) {
            s.zoom = 1;
            s.cx = s.cy = -1;
        } else {
            worldmap::zoomAt(sh.w, sh.h, s.mapW, s.mapH, s.zoom, s.cx, s.cy, float(s.mapW) / 2, float(s.mapH) / 2,
                             to.id > 0 ? 1.5f : 1 / 1.5f);
        }
        worldChanged(s);
        return;
    }
    if (to.go == refpages::Go::Expand) {  // «усі N ›»: the section in full, in place
        s.opened |= 1u << unsigned(to.id);
        const float keep = scrollNow(s);
        makePage(s);
        newPage(s, keep);
        return;
    }
    if (to.go == refpages::Go::Star) {  // 3.0.11-beta.18: the card shown into «Обране» or out of it
        const auto it = std::find_if(s.favourites.begin(), s.favourites.end(),
                                     [&](const refpages::Link& k) { return refpages::sameCard(k, s.at); });
        if (it != s.favourites.end()) s.favourites.erase(it);
        else if (isCard(s.at) && s.favourites.size() < kMostKept) s.favourites.insert(s.favourites.begin(), s.at);
        keepList(s.favourites, layout::kReferenceFavourites);
        const float keep = scrollNow(s);
        makePage(s);
        newPage(s, keep);
        return;
    }
    if (to.go == refpages::Go::Fold) {  // 3.0.11-beta.17: a section folded or back, in every card, kept for later
        s.folded ^= 1u << unsigned(to.id);
        const std::string path = layout::join(gameDir(), layout::kReferenceFolded);
        const std::string text = std::to_string(s.folded) + "\n";
        if (!writeFileAtomic(path, text.data(), text.size()))
            LOG_INFO("reference: %s cannot be written", path.c_str());
        const float keep = scrollNow(s);
        makePage(s);
        newPage(s, keep);
        return;
    }
    if (to.go == refpages::Go::Track) {  // 3.0.11-beta.70: a quest's or an item's pin in the tracker - the card stays
        if (to.id == 0) trackerwin::toggleQuest(to.map);
        else trackerwin::toggleItem(std::atoi(to.map.c_str()));
        const float keep = scrollNow(s);
        makePage(s);
        newPage(s, keep);
        return;
    }
    if (to.go == refpages::Go::Follow && to.id == 2) {  // 3.0.18-beta.240: an NPC's cell - its map's way, then it
        const size_t a = to.map.find(':'), b = a == std::string::npos ? a : to.map.find(':', a + 1);
        if (b == std::string::npos) return;
        const std::string code = to.map.substr(0, a);
        s.plotX = std::atoi(to.map.c_str() + a + 1);
        s.plotY = std::atoi(to.map.c_str() + b + 1);
        if (code == s.here) {
            minimapproto::plotTo(s.plotX, s.plotY);
            s.plotMap.clear();
        } else {
            minimapproto::follow(code, true);
            s.plotMap = code;
        }
        LOG_INFO("reference: led to %s %d, %d", code.c_str(), s.plotX, s.plotY);
        return;
    }
    if (to.go == refpages::Go::Follow) {  // 3.0.11-beta.9: the way on the minimap
        std::string target;
        bool walk = true;
        if (minimapproto::following(target, walk) && target == to.map && walk == (to.id == 0))
            minimapproto::unfollow();
        else
            minimapproto::follow(to.map, to.id == 0);
        const float keep = scrollNow(s);  // 3.0.11-beta.31: the window stays, its page says it is followed
        makePage(s);
        newPage(s, keep);
        return;
    }
    if (to.go == refpages::Go::None) return;
    int from = s.sel;  // 3.0.11-beta.28: a result opened by a click - picked when back at the results
    for (size_t i = 0; i < s.page.rows.size(); ++i)
        if (refpages::sameCard(s.page.rows[i].link, to)) {
            from = int(i);
            break;
        }
    s.back.push_back(Entry{s.at, s.query, s.page.name, scrollNow(s), s.opened, from});
    s.at = to;
    s.opened = 0;
    s.sel = -1;
    blurField(s);  // a card: the hotbar's keys the game's again
    noteRecent(s);  // 3.0.11-beta.18
    makePage(s);
    newPage(s, 0);
}

// 3.0.11-beta.26: «Головна» - the first screen (the search emptied), the page left kept for «Назад»
void goFirst(State& s) {
    if (s.at.go == refpages::Go::None && s.query.empty()) return;
    s.back.push_back(Entry{s.at, s.query, s.page.name, scrollNow(s), s.opened});
    s.at = refpages::Link{};
    setQuery(s, "");
    s.opened = 0;
    s.sel = -1;
    makePage(s);
    newPage(s, 0);
}

// The picked row's element marked (its class «sel»), the old one's let go; `show`: scrolled into view
void markSelected(State& s, bool show) {
    if (s.markedSel >= 0)
        if (Rml::Element* e = s.win.doc.byId(("r" + std::to_string(s.markedSel)).c_str())) e->SetClass("sel", false);
    s.markedSel = -1;
    if (s.sel < 0) return;
    if (Rml::Element* e = s.win.doc.byId(("r" + std::to_string(s.sel)).c_str())) {
        e->SetClass("sel", true);
        if (show) e->ScrollIntoView(false);
        s.markedSel = s.sel;
    }
}

// 3.0.11-beta.21: the arrows' (and the pad's D-pad's) row - the next one that leads somewhere, kept in view
void step(State& s, int by) {
    const int n = int(s.page.rows.size());
    int i = s.sel;
    for (int k = 0; k < n; ++k) {
        i = i < 0 ? (by > 0 ? 0 : n - 1) : i + by;
        if (i < 0 || i >= n) return;
        if (s.page.rows[size_t(i)].link.go != refpages::Go::None) break;
    }
    if (i < 0 || i >= n || s.page.rows[size_t(i)].link.go == refpages::Go::None) return;
    s.sel = i;
    markSelected(s, true);
}

// Enter (the pad's A): the picked row, else the first that leads to a card
void enter(State& s) {
    if (s.sel >= 0 && size_t(s.sel) < s.page.rows.size()) {
        go(s, s.page.rows[size_t(s.sel)].link);
        return;
    }
    for (const refpages::Row& r : s.page.rows)
        if (r.link.go != refpages::Go::None && r.link.go != refpages::Go::Fold && r.link.go != refpages::Go::Expand &&
            r.link.go != refpages::Go::Star) {
            go(s, r.link);
            return;
        }
}

void goBack(State& s) {
    if (s.back.empty()) return;
    const Entry e = s.back.back();
    s.back.pop_back();
    s.at = e.at;
    setQuery(s, e.query);
    s.opened = e.opened;
    s.sel = e.sel;
    makePage(s);
    newPage(s, e.scroll);
}

void typed(State& s) {  // the query changed: its results (a card left for them comes back by «Назад»)
    s.asTyped = false;  // 3.0.18-beta.245: by the keys' Latin again
    if (s.at.go != refpages::Go::None) s.back.push_back(Entry{s.at, "", s.page.name, scrollNow(s), s.opened});
    s.at = refpages::Link{};
    s.opened = 0;  // 3.0.11-beta.28: the groups closed, the first result picked again
    s.sel = -1;
    makePage(s);
    newPage(s, 0);
}

void scrollBy(State& s, float pages) {
    if (Rml::Element* e = scroller(s)) e->SetScrollTop(e->GetScrollTop() + pages * e->GetClientHeight() * 0.9f);
}

void openWindow(State& s, const refpages::Link& at) {
    if (!s.foldedRead) {  // 3.0.11-beta.17: the player's folded sections, from the last games
        s.foldedRead = true;
        std::string text;
        if (readTextFile(layout::join(gameDir(), layout::kReferenceFolded), text))
            s.folded = unsigned(std::strtoul(text.c_str(), nullptr, 10));
        readList(s.recent, layout::kReferenceRecent);  // 3.0.11-beta.18
        readList(s.favourites, layout::kReferenceFavourites);
    }
    s.open = true;
    s.skillWordsRead = false;  // 3.0.18-beta.244: the watcher may have brought them since
    setQuery(s, "");
    s.back.clear();
    s.at = at;
    s.opened = 0;
    s.sel = -1;
    s.wantPage = true;
    newPage(s, 0);
    noteRecent(s);  // 3.0.11-beta.18: a card from the minimap's lines too
}

void close(State& s) {
    s.open = false;
    s.dragging = false;
    blurField(s);
    s.padActive = s.padCross = false;  // 3.0.18-beta.241
    s.padKey.clear();
    s.ringWas = s.crossWas = "";
    if (s.win.nav.ring) s.win.nav.ring(nullptr);
    if (Rml::Element* c = s.win.doc.byId("rcross")) c->SetProperty("display", "none");
    if (s.oskOurs && oskwin::isOpen()) oskwin::close();
    s.oskOurs = false;
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, nullptr, false);
    s.shown = false;
}

// 3.0.18-beta.240: a layer's chip on the location's map - the NPCs (theirs and Kafra's) and the warps the minimap's
// own (one setting); the monster zones and «Ви тут» (the player, the party's faces) the window's
void switchLayer(State& s, int i) {
    if (i == 0) {
        const bool on = !(minimapproto::layerShown(int(minimap::Layer::Npcs)) ||
                          minimapproto::layerShown(int(minimap::Layer::Kafra)));
        for (const minimap::Layer l : {minimap::Layer::Npcs, minimap::Layer::Kafra})
            if (minimapproto::layerShown(int(l)) != on) minimapproto::switchLayerFromReference(int(l));
    } else if (i == 1) {
        minimapproto::switchLayerFromReference(int(minimap::Layer::Warps));
    } else if (i == 2) {
        s.layerMobs = !s.layerMobs;
    } else if (i == 3) {
        s.layerHere = !s.layerHere;
    }
    buildLocationScene(s);
    s.locKey.clear();
    locationPaint(s);
    s.bodyNow = true;  // the chips' looks (the scroll stays)
}

// Its «−», «⤢» (the whole map), «+» round the middle
void zoomLocation(State& s, int by) {
    if (s.locCode.empty() || s.locScene.map.w <= 0) return;
    if (by == 0) {
        s.mv.zoom = 1;
        s.mv.cx = s.mv.cy = -1;
    } else {
        minimap::zoomMapView(s.mv, s.locScene.map, float(s.locW) / 2, float(s.locH) / 2, by > 0 ? 1.5f : 1 / 1.5f,
                             minimap::mapViewMost(s.locSharp));
    }
    locationPaint(s);
}

// The rows' clicks: a data-ln's row's link, a jump chip's group scrolled to the top
// A click on the body's element (the mouse's through the listener; 3.0.18-beta.241 the pad's A): a row's link, a
// jump chip's section, a layer's chip, a zoom tool
void clickOn(State& s, Rml::Element* target) {
    for (Rml::Element* el = target; el; el = el->GetParentNode()) {
        if (el->GetId() == "body") return;
        const Rml::String ln = el->GetAttribute<Rml::String>("data-ln", "");
        if (!ln.empty()) {
            const int i = std::atoi(ln.c_str());
            if (i >= 0 && size_t(i) < s.page.rows.size()) {
                s.sel = i;
                go(s, s.page.rows[size_t(i)].link);
            }
            return;
        }
        const Rml::String jump = el->GetAttribute<Rml::String>("data-jump", "");
        if (!jump.empty()) {
            if (Rml::Element* to = s.win.doc.byId(jump.c_str())) to->ScrollIntoView(true);
            return;
        }
        const Rml::String lay = el->GetAttribute<Rml::String>("data-lay", "");  // 3.0.18-beta.240
        if (!lay.empty()) {
            switchLayer(s, std::atoi(lay.c_str()));
            return;
        }
        const Rml::String mz = el->GetAttribute<Rml::String>("data-mz", "");
        if (!mz.empty()) {
            zoomLocation(s, std::atoi(mz.c_str()));
            return;
        }
    }
}

// 3.0.18-beta.242 (the concept's cards): Shift+click on a name - «[Name]» into the chat line (a monster's, a thing's,
// an NPC's, a quest's name; a map's code), the card's own title too; false: not a name (the click goes on)
bool shiftClick(State& s, Rml::Element* target) {
    if (!s.index) return false;
    refpages::Link at;
    for (Rml::Element* el = target; el && el->GetId() != "body"; el = el->GetParentNode()) {
        const Rml::String ln = el->GetAttribute<Rml::String>("data-ln", "");
        if (!ln.empty()) {
            const int i = std::atoi(ln.c_str());
            if (i >= 0 && size_t(i) < s.page.rows.size()) at = s.page.rows[size_t(i)].link;
            break;
        }
        if (el->IsClassSet("rtitle")) {
            at = s.at;
            break;
        }
    }
    using refpages::Go;
    if (at.go != Go::Mob && at.go != Go::Item && at.go != Go::Map && at.go != Go::Npc && at.go != Go::Quest)
        return false;
    const std::string name = at.go == Go::Map ? at.map : refpages::titleOf(*s.index, language(), at);
    if (name.empty() || !chatwin::putIntoLine("[" + name + "]")) return false;
    LOG_INFO("reference: Shift+click - «[%s]» into the chat line", name.c_str());
    return true;
}

class Clicks final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& e) override {
        if (GetKeyState(VK_SHIFT) < 0 && shiftClick(state(), e.GetTargetElement())) return;  // 3.0.18-beta.242
        clickOn(state(), e.GetTargetElement());
    }
};
Clicks g_clicks;

bool make(State& s) {
    if (s.win.made()) return true;
    const bool made = s.win.make("reference", ur::rml(), [&s](Rml::DataModelConstructor& c) {
        ur::bind(c, s.model);
        c.BindEventCallback("ref_back", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            goBack(state());
        });
        c.BindEventCallback("ref_home", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            goFirst(state());
        });
        c.BindEventCallback("ref_star", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            go(state(), refpages::Link{refpages::Go::Star, 0, ""});
        });
        c.BindEventCallback("ref_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            close(state());
        });
        c.BindEventCallback("ref_rail", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const int i = a.empty() ? -1 : a[0].Get<int>(-1);
            if (i == 0) goFirst(st);  // «Головна»
            else if (i > 0 && size_t(i) < st.page.rail.size()) go(st, st.page.rail[size_t(i)].link);
        });
        c.BindEventCallback("ref_chip", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            go(state(), refpages::Link{refpages::Go::Section, refpages::kDataSection, ""});  // 3.0.11-beta.69
        });
    });
    if (made) LOG_INFO("reference: the window made on the new interface (uikit/reference_live)");
    return made;
}

// 3.0.18-beta.240: the location's map's box on the target (false: not its page, not laid out)
bool locationBox(State& s, float& x, float& y, float& w, float& h) {
    Rml::Element* e = s.win.doc.byId("mview");
    if (!e || s.locCode.empty()) return false;
    const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Content);
    const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Content);
    x = at.x, y = at.y, w = size.x, h = size.y;
    return w > 0 && h > 0;
}

// The world map's box on the target (false: not its page, not laid out)
bool mapBox(State& s, float& x, float& y, float& w, float& h) {
    Rml::Element* e = s.win.doc.byId("wmap");
    if (!e || !worldPage(s)) return false;
    const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Content);
    const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Content);
    x = at.x, y = at.y, w = size.x, h = size.y;
    return w > 0 && h > 0;
}

void block(State& s, HWND game) {
    RECT client{};
    int bx = 0, by = 0, bw = 0, bh = 0;
    if (!GetClientRect(game, &client) || s.targetW <= 0 || s.targetH <= 0 || !s.win.box(bx, by, bw, bh)) return;
    const RECT r = clientBox(client, s.targetW, s.targetH, float(bx), float(by), float(bw), float(bh));
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, game, &r, false);
}

// The window's box from the screen and its corner (win/window_stack's size for «reference»); narrow below 900 dp
void sizeBox(State& s) {
    const float f = windowstack::sizeOf(winstack::Win::Reference);
    const float most = 0.96f;
    int w = int(std::lround(std::min(kWideW * s.dp, 0.92f * float(s.targetW)) * f));
    int h = int(std::lround(std::min(kWideH * s.dp, 0.90f * float(s.targetH)) * f));
    w = std::clamp(w, std::min(px(560, s), s.targetW), int(most * float(s.targetW)));
    h = std::clamp(h, std::min(px(420, s), s.targetH), int(most * float(s.targetH)));
    const bool narrow = float(w) < kNarrowBelow * s.dp;
    // the world map's box: the body less its padding, the panel at its right, the bar over it
    const int mapW = std::max(px(200, s), w - px(2 * 18 + kPanelW, s) - 2);
    const int mapH = std::max(px(160, s), h - px(56 + 36 + 30 + 42, s) - 2);
    // 3.0.18-beta.240: a location's map - three fifths of the body beside its NPCs (narrow: the body's width), its
    // height the body's less its title and bar
    const int content = w - px(float(narrow ? 52 : 196) + 36, s) - 2;
    const int locW = std::max(px(240, s), narrow ? content : std::min(int(float(content) * 0.6f), px(640, s)));
    const int locH = std::clamp(h - px(56 + 36 + 30 + 34 + 110, s), px(220, s), px(600, s));
    const bool changed = w != s.w || h != s.h;
    if (narrow != s.narrow || (worldPage(s) && (mapW != s.mapW || mapH != s.mapH)) ||
        (locationPage(s) && (locW != s.locW || locH != s.locH)))
        s.wantPage = true;
    s.w = w, s.h = h, s.narrow = narrow, s.mapW = mapW, s.mapH = mapH, s.locW = locW, s.locH = locH;
    if (changed) {
        char b[32];
        std::snprintf(b, sizeof b, "%dpx", w);
        s.model.w = b;
        std::snprintf(b, sizeof b, "%dpx", h);
        s.model.h = b;
        s.win.doc.dirty("w");
        s.win.doc.dirty("h");
    }
}

}  // namespace

// 3.0.18-beta.241 (docs/superpowers/plans/2026-11-13-reference-rewrite.md, stage 4; the concept's pad, «as in other
// windows»): the gold frame walks the body's controls (ur::padTargets) by the D-pad and the left stick, A clicks the
// one framed (a map: the cross takes it), B back, X «Вести сюди», Y the search with the on-screen keyboard, LB / RB
// the sections, the right stick scrolls; a press of the mouse or a key gives the window back to them
panel::Box boxIn(State& s, Rml::Element* e) {  // in the window's own pixels (the ring's)
    Rml::ElementDocument* d = s.win.doc.doc();
    const Rml::Vector2f origin = d ? d->GetAbsoluteOffset(Rml::BoxArea::Border) : Rml::Vector2f(0, 0);
    const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border) - origin;
    const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
    return panel::Box{at.x, at.y, size.x, size.y};
}

bool inView(State& s, const panel::Box& b) {  // its middle inside the scrolled body
    Rml::Element* sc = scroller(s);
    if (!sc) return true;
    const panel::Box v = boxIn(s, sc);
    const float mid = b.y + b.h / 2;
    return mid >= v.y && mid <= v.y + v.h;
}

std::vector<Rml::Element*> padTargets(State& s) { return ur::padTargets(s.win.doc.byId("body")); }

std::string padKeyOf(Rml::Element* e) {  // the control's own name: its id, else its data attribute
    const Rml::String id = e->GetId();
    if (!id.empty()) return "#" + id;
    for (const char* a : {"data-jump", "data-lay", "data-mz"}) {
        const Rml::String v = e->GetAttribute<Rml::String>(a, "");
        if (!v.empty()) return std::string(a) + '=' + v;
    }
    return {};
}

// The framed control's place among the targets: its name's on this page, else the row that led away from it («Назад»
// brought it back), else the first one shown (-1 none)
int padIndex(State& s, const std::vector<Rml::Element*>& t) {
    const std::string page = std::to_string(int(s.at.go)) + ':' + std::to_string(s.at.id) + ':' + s.at.map + '|' +
                             s.query;
    if (page != s.padPageOf) {  // another page: from its first control
        s.padPageOf = page;
        s.padKey.clear();
    }
    const std::string want = !s.padKey.empty() ? s.padKey : s.sel >= 0 ? "#r" + std::to_string(s.sel) : "";
    if (!want.empty())
        for (size_t i = 0; i < t.size(); ++i)
            if (padKeyOf(t[i]) == want && (!s.padKey.empty() || inView(s, boxIn(s, t[i])))) return int(i);
    for (size_t i = 0; i < t.size(); ++i)
        if (inView(s, boxIn(s, t[i]))) return int(i);
    return t.empty() ? -1 : 0;
}

void ringAt(State& s, const panel::Box* b) {  // the gold frame (said again only when it moves)
    char key[96] = "none";
    if (b)
        std::snprintf(key, sizeof key, "%.1f %.1f %.1f %.1f", double(b->x), double(b->y), double(b->w), double(b->h));
    if (s.ringWas == key || !s.win.nav.ring) return;
    s.ringWas = key;
    s.win.nav.ring(b);
}

void crossAt(State& s, bool show, float left, float top) {
    char key[64] = "none";
    if (show) std::snprintf(key, sizeof key, "%.1f %.1f", double(left), double(top));
    if (s.crossWas == key) return;
    s.crossWas = key;
    Rml::Element* c = s.win.doc.byId("rcross");
    if (!c) return;
    c->SetProperty("display", show ? "block" : "none");
    if (!show) return;
    c->SetProperty("left", std::to_string(left) + "px");
    c->SetProperty("top", std::to_string(top) + "px");
}

void padOn(State& s) {
    if (s.padActive) return;
    s.padActive = true;
    fillChrome(s);  // the bottom line: the pad's buttons
}

void padOff(State& s) {  // the mouse or a key took the window back
    if (!s.padActive && !s.padCross) return;
    s.padActive = s.padCross = false;
    s.padKey.clear();
    ringAt(s, nullptr);
    crossAt(s, false, 0, 0);
    fillChrome(s);
}

bool crossBox(State& s, float& x, float& y, float& w, float& h) {
    return s.crossWorld ? mapBox(s, x, y, w, h) : locationBox(s, x, y, w, h);
}

void crossOn(State& s, bool world) {
    s.crossWorld = world;
    float x = 0, y = 0, w = 0, h = 0;
    if (!crossBox(s, x, y, w, h)) return;
    s.padCross = true;
    s.crossX = w / 2;
    s.crossY = h / 2;
    if (Rml::Element* m = s.win.doc.byId(world ? "wmap" : "mview"))
        m->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest));
    ringAt(s, nullptr);
    fillChrome(s);
    LOG_INFO("reference: the pad took the %s map", world ? "world" : "location's");
}

void crossOff(State& s) {
    if (!s.padCross) return;
    s.padCross = false;
    crossAt(s, false, 0, 0);
    fillChrome(s);
}

// A location map's mark opened (the mouse's click, the pad's A at the cross): an NPC's card, a warp's map's
void openMark(State& s, int mark) {
    if (mark < 0 || size_t(mark) >= s.locNpc.size()) return;
    if (s.locNpc[size_t(mark)] >= 0) go(s, refpages::Link{refpages::Go::Npc, s.locNpc[size_t(mark)], s.locCode});
    else if (!s.locDest[size_t(mark)].empty()) go(s, refpages::Link{refpages::Go::Map, 0, s.locDest[size_t(mark)]});
}

// The view moved by (dx, dy) of the map's pixels under the cross (the right stick; the cross pushed past an edge)
void panUnder(State& s, float dx, float dy) {
    if (s.crossWorld) {
        if (!s.world) return;
        const worldmap::Fit v = worldmap::view(s.world->sheet.w, s.world->sheet.h, s.mapW, s.mapH, s.zoom, s.cx, s.cy);
        if (v.s <= 0) return;
        s.cx = (float(s.mapW) / 2 - v.x + dx) / v.s;
        s.cy = (float(s.mapH) / 2 - v.y + dy) / v.s;
        worldChanged(s);
        return;
    }
    minimap::panMapView(s.mv, s.locScene.map, -dx, -dy);
    locationPaint(s);
}

void moveCross(State& s, float dx, float dy) {
    float x = 0, y = 0, w = 0, h = 0;
    if (!crossBox(s, x, y, w, h)) return;
    const float edge = 4 * s.dp;
    float nx = s.crossX + dx, ny = s.crossY + dy, px = 0, py = 0;
    if (nx < edge) px = nx - edge, nx = edge;
    else if (nx > w - edge) px = nx - (w - edge), nx = w - edge;
    if (ny < edge) py = ny - edge, ny = edge;
    else if (ny > h - edge) py = ny - (h - edge), ny = h - edge;
    s.crossX = nx;
    s.crossY = ny;
    if (px != 0 || py != 0) panUnder(s, px, py);
}

// The cross's hover: the mark or the location under it, named as under the pointer
void crossHover(State& s, float w, float h) {
    if (s.crossWorld) {
        if (!s.world) return;
        std::string map;
        const refwin::Under u =
            refwin::worldUnder(worldLayout(s), panel::Box{0, 0, w, h}, worldLook(s, false), s.crossX, s.crossY);
        if (u.part == refwin::Part::Tile && size_t(u.tile) < s.world->sheet.world.size())
            map = s.world->sheet.world[size_t(u.tile)].map;
        if (map != s.worldHover) {
            s.worldHover = map;
            worldPaint(s);
        }
        return;
    }
    const int hover = minimap::mapViewMarkAt(s.mv, s.locScene, s.crossX, s.crossY);
    if (hover != s.locHover) {
        s.locHover = hover;
        locationPaint(s);
    }
}

// Each frame while the pad has the window: the frame on its control, or the cross on its map
void padShow(State& s) {
    if (s.oskOurs && !oskwin::isOpen()) s.oskOurs = false;
    if (s.padCross) {
        float x = 0, y = 0, w = 0, h = 0;
        if (crossBox(s, x, y, w, h)) {
            s.crossX = std::clamp(s.crossX, 0.0f, w);
            s.crossY = std::clamp(s.crossY, 0.0f, h);
            crossHover(s, w, h);
            Rml::ElementDocument* d = s.win.doc.doc();
            const Rml::Vector2f origin = d ? d->GetAbsoluteOffset(Rml::BoxArea::Border) : Rml::Vector2f(0, 0);
            const float half = 13 * s.dp;
            crossAt(s, true, x - origin.x + s.crossX - half, y - origin.y + s.crossY - half);
            return;
        }
        crossOff(s);  // its map gone (another page)
    }
    if (!s.padActive) return;
    if (s.applyIn > 0) {  // a new body not laid out yet
        ringAt(s, nullptr);
        return;
    }
    const std::vector<Rml::Element*> t = padTargets(s);
    const int i = padIndex(s, t);
    if (i < 0) {
        ringAt(s, nullptr);
        return;
    }
    const panel::Box b = boxIn(s, t[size_t(i)]);
    ringAt(s, b.w > 0 && b.h > 0 && inView(s, b) ? &b : nullptr);
}

void padMove(State& s, padnav::Dir d) {
    if (s.padCross) {  // the D-pad: the cross by small steps
        const float step = 16 * s.dp;
        moveCross(s, d == padnav::Dir::Left ? -step : d == padnav::Dir::Right ? step : 0,
                  d == padnav::Dir::Up ? -step : d == padnav::Dir::Down ? step : 0);
        return;
    }
    const std::vector<Rml::Element*> t = padTargets(s);
    int i = padIndex(s, t);
    if (i < 0) return;
    std::vector<panel::Box> boxes;
    boxes.reserve(t.size());
    for (Rml::Element* e : t) boxes.push_back(boxIn(s, e));
    i = padnav::next(boxes, i, d);
    if (i < 0 || size_t(i) >= t.size()) return;
    Rml::Element* e = t[size_t(i)];
    s.padKey = padKeyOf(e);
    e->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest));
    const Rml::String ln = e->GetAttribute<Rml::String>("data-ln", "");  // the keys' pick follows (Enter the same)
    s.sel = ln.empty() ? -1 : std::atoi(ln.c_str());
    markSelected(s, false);
}

void crossPress(State& s) {
    float x = 0, y = 0, w = 0, h = 0;
    if (!crossBox(s, x, y, w, h)) return;
    if (!s.crossWorld) {
        openMark(s, minimap::mapViewMarkAt(s.mv, s.locScene, s.crossX, s.crossY));
        return;
    }
    if (!s.world) return;
    const refwin::Under u =
        refwin::worldUnder(worldLayout(s), panel::Box{0, 0, w, h}, worldLook(s, false), s.crossX, s.crossY);
    if (u.part == refwin::Part::Chain && size_t(u.tile) < s.way.chain.size())
        go(s, refpages::Link{refpages::Go::Map, 0, s.way.chain[size_t(u.tile)].first});
    else if (u.part == refwin::Part::Tile && size_t(u.tile) < s.world->sheet.world.size())
        go(s, refpages::Link{refpages::Go::Map, 0, s.world->sheet.world[size_t(u.tile)].map});
}

void padPress(State& s) {
    if (s.padCross) {
        crossPress(s);
        return;
    }
    const std::vector<Rml::Element*> t = padTargets(s);
    const int i = padIndex(s, t);
    if (i < 0) return;
    Rml::Element* e = t[size_t(i)];
    const Rml::String id = e->GetId();
    if (id == "mview" || id == "wmap") {
        crossOn(s, id == "wmap");
        return;
    }
    s.padKey = padKeyOf(e);
    clickOn(s, e);
}

void crossNext(State& s) {  // X on a location's map: the next NPC or warp round it (as on «⤢»)
    const std::vector<minimap::Mark>& marks = s.locScene.marks;
    std::vector<size_t> order;
    for (size_t i = 0; i < marks.size(); ++i)
        if (marks[i].glyph != minimap::Glyph::Member) order.push_back(i);
    float x = 0, y = 0, w = 0, h = 0;
    if (order.empty() || !crossBox(s, x, y, w, h)) return;
    const int under = minimap::mapViewMarkAt(s.mv, s.locScene, s.crossX, s.crossY);
    size_t at = 0;
    for (size_t k = 0; k < order.size(); ++k)
        if (int(order[k]) == under) at = (k + 1) % order.size();
    const minimap::Cell c = marks[order[at]].cell;
    float px = 0, py = 0;
    minimap::cellToPanel(minimap::viewOf(s.mv, s.locScene.map), s.locScene.map, c, px, py);
    if (px < 0 || py < 0 || px >= w || py >= h) {  // out of the view: the view on it
        s.mv.cx = float(c.x) + 0.5f;
        s.mv.cy = float(c.y) + 0.5f;
        locationPaint(s);
        minimap::cellToPanel(minimap::viewOf(s.mv, s.locScene.map), s.locScene.map, c, px, py);
    }
    s.crossX = px;
    s.crossY = py;
}

void padLead(State& s) {
    if (s.padCross) {
        if (!s.crossWorld) crossNext(s);
        return;
    }
    const refpages::Link* l = refpages::leadOf(s.page);
    if (!l) return;
    const refpages::Link to = *l;  // go() makes the page again
    LOG_INFO("reference: the pad's X - the way (%d, %s)", to.id, to.map.c_str());
    go(s, to);
}

void padSection(State& s, int by) {
    if (s.padCross) {  // LB / RB on a map: its zoom round the cross
        const float f = by > 0 ? 1.25f : 0.8f;
        if (!s.crossWorld) {
            minimap::zoomMapView(s.mv, s.locScene.map, s.crossX, s.crossY, f, minimap::mapViewMost(s.locSharp));
            locationPaint(s);
        } else if (s.world) {
            worldmap::zoomAt(s.world->sheet.w, s.world->sheet.h, s.mapW, s.mapH, s.zoom, s.cx, s.cy, s.crossX,
                             s.crossY, f);
            worldChanged(s);
        }
        return;
    }
    const int i = refpages::railStep(s.page, by);
    if (i == 0) {
        goFirst(s);
    } else if (i > 0) {
        const refpages::Link to = s.page.rail[size_t(i)].link;
        go(s, to);
    }
}

void padSearch(State& s) {  // Y: the field's words by the on-screen keyboard, Start its Enter
    crossOff(s);
    oskwin::Field f;
    f.read = [] { return std::string(state().model.query); };
    f.write = [](const std::string& q) {
        State& st = state();
        st.model.query = q;
        st.win.doc.dirty("query");
    };
    f.enter = [] {
        State& st = state();
        if (st.model.query != st.query) {  // the results first, then the first of them
            st.query = st.model.query;
            typed(st);
        }
        enter(st);
    };
    f.longest = 40;
    oskwin::openFor(f);
    s.oskOurs = true;
}

void padSticksIn(State& s, float lx, float ly, float rx, float ry, float seconds) {
    if (seconds <= 0) return;
    const auto speed = [&](float mag) {  // 80 to 480 dp a second by how far the stick is pushed
        return (80.0f + 400.0f * std::min(1.0f, (mag - 0.2f) / 0.8f)) * s.dp * seconds;
    };
    if (s.padCross) {
        const float lm = std::hypot(lx, ly), rm = std::hypot(rx, ry);
        if (lm > 0.2f) moveCross(s, lx / lm * speed(lm), -ly / lm * speed(lm));  // the stick's up: the cross up
        if (rm > 0.2f) panUnder(s, rx / rm * speed(rm), -ry / rm * speed(rm));  // the right stick: the map under it
        return;
    }
    if (std::fabs(ry) <= 0.2f) return;
    if (Rml::Element* sc = scroller(s)) {  // the right stick scrolls the body; the frame on the first one shown
        const float v = std::min(1.0f, (std::fabs(ry) - 0.2f) / 0.8f);
        sc->SetScrollTop(sc->GetScrollTop() - (ry > 0 ? 1.0f : -1.0f) * (200.0f + 1400.0f * v) * s.dp * seconds);
        s.padKey.clear();
    }
}

// 3.0.18-beta.247 (R-ZW43KT): the new body's icon placeholders (data-pic: the picture's key)
void collectWaiting(State& s) {
    s.waiting.clear();
    Rml::Element* body = s.win.doc.byId("body");
    if (!body) return;
    Rml::ElementList found;
    body->QuerySelectorAll(found, "[data-pic]");
    s.waiting.reserve(found.size());
    for (Rml::Element* e : found) s.waiting.emplace_back(e->GetAttribute<Rml::String>("data-pic", ""), e);
}

// A few icons a frame - those in view and a view round it first (the rest when scrolled to) - each put on every
// placeholder of its key as a decorator: no layout, no markup made again
void loadWaiting(State& s) {
    if (s.waiting.empty() || scrollNow(s) == s.waitingScroll) return;
    Rml::Element* sc = scroller(s);
    float top = -1e9f, bottom = 1e9f;
    if (sc) {
        const panel::Box v = boxIn(s, sc);
        top = v.y - v.h;
        bottom = v.y + 2 * v.h;
    }
    int budget = 8;
    for (size_t i = 0; i < s.waiting.size() && budget > 0;) {
        const panel::Box b = boxIn(s, s.waiting[i].second);
        if (b.y + b.h < top || b.y > bottom) {
            ++i;
            continue;
        }
        const std::string key = s.waiting[i].first;
        loadPicture(s, key);
        --budget;
        const auto it = s.pictures.find(key);
        const std::string src = it == s.pictures.end() || it->second.width <= 0
                                    ? std::string()
                                    : uaui::picture(&it->second, uaui::stamp(&it->second), s.sources[key]);
        for (size_t j = s.waiting.size(); j-- > 0;) {
            if (s.waiting[j].first != key) continue;
            if (!src.empty()) {
                s.waiting[j].second->SetProperty("decorator", "image(\"" + src + "\" contain)");
                s.waiting[j].second->SetClass("none", false);
            }
            s.waiting.erase(s.waiting.begin() + std::ptrdiff_t(j));
        }
        if (i > s.waiting.size()) i = s.waiting.size();
    }
    if (budget > 0) s.waitingScroll = scrollNow(s);  // all in view made: none looked for till the body scrolls
}

void frame(HWND game, bool enabled, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.enabled = enabled;
    if (s.open && !enabled) {
        close(s);
        LOG_INFO("reference: off ([interface] reference = 0) - the window closed");
    }
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !make(s)) {
        if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, nullptr, false);
        s.shown = false;
        s.win.frame(false, 0, 0, -1, nullptr);
        return;
    }
    if (!s.listened)
        if (Rml::Element* b = s.win.doc.byId("body")) {
            b->AddEventListener(Rml::EventId::Click, &g_clicks);
            s.listened = true;
        }
    const std::string here = readGameMap();
    if (here != s.here) {  // 3.0.11-beta.8: «ви тут» and the routes are from the player's map
        s.here = here;
        s.wantPage = true;
    }
    if (float(k) != s.k || targetWidth != s.targetW || targetHeight != s.targetH || uaui::dp() != s.dp) {
        s.k = float(k);
        s.dp = uaui::dp();
        s.targetW = targetWidth;
        s.targetH = targetHeight;
        retirePictures(s);  // their sizes are the dp's
        s.wantPage = true;
    }
    sizeBox(s);
    const auto index = reference::index();
    if (index != s.index) {
        s.index = index;
        s.wantPage = true;
    }
    const auto online = reference::online();
    if (online != s.online) {
        s.online = online;
        s.wantPage = true;
    }
    const auto live = reference::live();  // 3.0.11-beta.12: and the timers' minutes go on
    if (live != s.live || (s.live && GetTickCount64() - s.madeAt >= 60000)) {
        s.live = live;
        s.wantPage = true;
    }
    if (s.at.go == refpages::Go::World) {  // 3.0.11-beta.11: made at the first look at it
        const auto world = reference::world();
        if (world != s.world) {
            s.world = world;
            s.worldPicture = panel::Picture{};
            s.wantPage = true;
        }
    }
    if (s.model.query != s.query) {  // typed in the field (RmlUi's input writes the model)
        s.query = s.model.query;
        typed(s);
    }
    if (s.wantPage) {
        const float keep = scrollNow(s);
        makePage(s);
        if (s.applyIn < 0) newPage(s, keep);  // made again in place (live data, the map): the scroll kept
    }
    // the world map: dragged, and the map under the pointer (its frame and its card)
    float mx = 0, my = 0, mw = 0, mh = 0;
    const bool onMap = mapBox(s, mx, my, mw, mh);
    int cx = 0, cy = 0;
    const bool pointer = readGameCursor(cx, cy);
    const float px0 = float(cx) * float(k) - mx, py0 = float(cy) * float(k) - my;
    if (s.dragging) {
        const int left = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;  // the physical one pressed
        if (GetAsyncKeyState(left) >= 0 || !onMap || !s.world) {
            s.dragging = false;  // let go outside the window
        } else if (pointer) {
            const float dx = px0 + mx - s.dragX, dy = py0 + my - s.dragY;
            if (!s.dragged && std::abs(dx) + std::abs(dy) >= 5 * s.dp) s.dragged = true;
            const worldmap::Fit v = worldmap::view(s.world->sheet.w, s.world->sheet.h, s.mapW, s.mapH, s.zoom, s.cx,
                                                   s.cy);
            if (s.dragged && v.s > 0 && GetTickCount64() - s.worldMadeAt >= 33) {
                s.cx = s.dragCx - dx / v.s;
                s.cy = s.dragCy - dy / v.s;
                worldChanged(s);
            }
        }
    }
    if (onMap && s.world && !(s.padCross && s.crossWorld)) {  // 3.0.18-beta.241: the cross's own hover
        std::string map;
        if (pointer && !s.padCross && !(s.dragging && s.dragged) && px0 >= 0 && py0 >= 0 && px0 < mw && py0 < mh) {
            const refwin::Under u = refwin::worldUnder(worldLayout(s), panel::Box{0, 0, mw, mh}, worldLook(s, false),
                                                       px0, py0);
            if (u.part == refwin::Part::Tile && size_t(u.tile) < s.world->sheet.world.size())
                map = s.world->sheet.world[size_t(u.tile)].map;
        }
        if (map != s.worldHover) {
            s.worldHover = map;
            worldPaint(s);
        }
    }
    // 3.0.18-beta.240: the location's map - dragged, the mark under the pointer (its ring, its words), the player
    // walking on his own map; «Вести сюди»'s cell once the player is on its map
    float lx0 = 0, ly0 = 0, lw = 0, lh = 0;
    if (locationBox(s, lx0, ly0, lw, lh)) {
        if (s.locDrag) {
            const int left = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
            if (GetAsyncKeyState(left) >= 0) {
                s.locDrag = false;
            } else if (pointer) {
                const float dx = float(cx) * float(k) - s.locDragX, dy = float(cy) * float(k) - s.locDragY;
                if (!s.locDragged && std::abs(dx) + std::abs(dy) >= 5 * s.dp) s.locDragged = true;
                if (s.locDragged && (dx != 0 || dy != 0)) {
                    minimap::panMapView(s.mv, s.locScene.map, dx, dy);
                    s.locDragX += dx;
                    s.locDragY += dy;
                    locationPaint(s);
                }
            }
        }
        const float lx = float(cx) * float(k) - lx0, ly = float(cy) * float(k) - ly0;
        if (pointer && !s.padCross && !s.locDragged && lx >= 0 && ly >= 0 && lx < lw && ly < lh) {
            const int hover = minimap::mapViewMarkAt(s.mv, s.locScene, lx, ly);
            if (hover != s.locHover) {
                s.locHover = hover;
                locationPaint(s);
            }
        }
        if (s.locCode == s.here && s.layerHere) {
            std::string members;  // the party's members moved, came or went: the marks made again
            for (const partywin::OnMap& p : partywin::onMap(memberSide(s))) members += membersKeyOf(p);
            if (members != s.locMembers) {
                buildLocationScene(s);
                locationPaint(s);
            }
        }
        if (s.locCode == s.here && s.locScene.showPlayer) {
            int x = -1, y = -1, mw = 0, mh = 0;
            if (readPlayerCell(x, y, mw, mh) && (x != s.locScene.player.x || y != s.locScene.player.y)) {
                s.locScene.player = minimap::Cell{x, y};
                locationPaint(s);
            }
        }
    }
    if (!s.plotMap.empty() && s.here == s.plotMap) {
        minimapproto::plotTo(s.plotX, s.plotY);
        LOG_INFO("reference: on %s - the way to %d, %d", s.plotMap.c_str(), s.plotX, s.plotY);
        s.plotMap.clear();
    }
    if (s.waitingIn > 0 && --s.waitingIn == 0) collectWaiting(s);  // 3.0.18-beta.247
    loadWaiting(s);
    if (s.style != uistyle::generation()) {  // 3.0.11-beta.47: its ground another opacity
        s.style = uistyle::generation();
        fillChrome(s);
    }
    if (const int icons = packpictures::iconsStamp(); icons != s.iconsMade) {  // 3.0.11-beta.74: the pack's icons
        s.iconsMade = icons;
        retirePictures(s);
    }
    if (s.bodyNow || (s.bodyDirty && GetTickCount64() - s.bodyAt >= 150)) renderBody(s);  // the scroll stays
    for (auto it = s.retired.begin(); it != s.retired.end();)  // the pictures let go: away once unseen
        it = --it->first <= 0 ? s.retired.erase(it) : it + 1;
    if (s.applyIn > 0 && --s.applyIn == 0) {  // the new body laid out: its scroll, its picked row
        s.applyIn = -1;
        if (Rml::Element* e = scroller(s)) e->SetScrollTop(s.scrollTo);
        markSelected(s, false);
    }
    const int x = (targetWidth - s.w) / 2, y = (targetHeight - s.h) / 2;
    s.win.frame(true, x, y, -1, nullptr);
    s.shown = s.win.shown;
    if (s.shown) block(s, game);
    if (s.shown && (s.padActive || s.padCross)) padShow(s);  // 3.0.18-beta.241
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0) return;
    s.win.draw(s.shadow, record, renderer, s.dp, s.k);  // its place in the windows' stack, its shadow
}

bool fieldKey(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.open || !s.shown || !fieldFocused(s)) return false;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) padOff(s);  // 3.0.18-beta.241: the keys took it back
    const bool alt = GetKeyState(VK_MENU) < 0;
    if (m == WM_SYSKEYDOWN && wp == VK_LEFT && alt) {  // Alt+← back, the field's words as they are
        goBack(s);
        return true;
    }
    if (m != WM_KEYDOWN) return false;
    switch (wp) {
        case VK_UP: step(s, -1); return true;
        case VK_DOWN: step(s, +1); return true;
        case VK_PRIOR: scrollBy(s, -1); return true;
        case VK_NEXT: scrollBy(s, +1); return true;
        case VK_RETURN: enter(s); return true;
        case VK_ESCAPE:  // the owner's: Esc first clears the field, then closes the window
            if (!s.query.empty()) {
                setQuery(s, "");
                typed(s);
            } else {
                close(s);
            }
            return true;
        case VK_BACK:
            if (s.query.empty() && !s.back.empty()) {
                goBack(s);
                return true;
            }
            return false;
        default: return false;
    }
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (!s.enabled) return false;
    if (s.open && (m == WM_KEYDOWN || m == WM_SYSKEYDOWN)) padOff(s);  // 3.0.18-beta.241: the keys took it back
    const bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
    if ((m == WM_CHAR || m == WM_SYSCHAR) && s.skipChar && wp == s.skipChar) {  // Ctrl+F's or Esc's own character
        s.skipChar = 0;
        return true;
    }
    if (m == WM_KEYDOWN && wp == 'F' && ctrl && !alt) {
        if (s.open) {
            close(s);
        } else {
            openWindow(s, refpages::Link{});
            LOG_INFO("reference: opened by Ctrl+F%s", s.index ? "" : " (no pack read yet)");
        }
        s.skipChar = 6;  // Ctrl+F's WM_CHAR
        return true;
    }
    // 3.0.11-beta.64: Ctrl+M - the world map at once, and away again (the game takes M with Alt alone)
    if (m == WM_KEYDOWN && wp == 'M' && ctrl && !alt) {
        const bool onIt = s.open && s.at.go == refpages::Go::World && s.at.id == 0;
        if (onIt) close(s);
        else if (s.open) go(s, refpages::Link{refpages::Go::World, 0, ""});
        else openWindow(s, refpages::Link{refpages::Go::World, 0, ""});
        LOG_INFO("reference: Ctrl+M - the world map %s", onIt ? "closed" : "opened");
        s.skipChar = 13;  // Ctrl+M's WM_CHAR is a carriage return - the chat's Enter
        return true;
    }
    if (!s.open) return false;
    // 3.0.18-beta.239 (the owner 13.11): the caret not in the field - the hotbar's F-keys and the Alt windows are
    // the game's; the window's own keys and the letters (into the field) its
    if (m == WM_SYSKEYDOWN && wp == VK_LEFT && alt) {
        goBack(s);
        return true;
    }
    if (m == WM_KEYDOWN) {
        switch (wp) {
            case VK_ESCAPE:
                if (!s.query.empty()) {
                    setQuery(s, "");
                    typed(s);
                } else {
                    close(s);
                }
                s.skipChar = 27;
                return true;
            case VK_BACK:
                if (s.at.go != refpages::Go::None || !s.back.empty()) goBack(s);
                s.skipChar = 8;
                return true;
            case VK_DELETE:
                if (!s.query.empty()) {
                    setQuery(s, "");
                    typed(s);
                }
                return true;
            case VK_RETURN:  // the picked row, else the first that leads to a card
                enter(s);
                s.skipChar = 13;
                return true;
            case VK_PRIOR: scrollBy(s, -1); return true;
            case VK_NEXT: scrollBy(s, +1); return true;
            case VK_UP: step(s, -1); return true;
            case VK_DOWN: step(s, +1); return true;
            case VK_HOME:
                if (Rml::Element* e = scroller(s)) e->SetScrollTop(0);
                return true;
            case VK_END:
                if (Rml::Element* e = scroller(s)) e->SetScrollTop(e->GetScrollHeight());
                return true;
            default: break;
        }
        if (ctrl && !alt && wp == 'V') {  // 3.0.18-beta.72: the clipboard - into the field
            if (Rml::Element* f = field(s)) f->Focus();
            return uaui::key(m, wp, lp);
        }
        if (wp >= VK_F1 && wp <= VK_F24) return false;  // the hotbar's
        return !ctrl;  // a letter's key: its character comes as WM_CHAR into the field
    }
    if (m == WM_CHAR && !ctrl && !alt && wp >= 32 && wp != 127) {  // type anywhere: into the field
        if (wp == ' ' && (s.query.empty() || s.query.back() == ' ')) return true;
        if (Rml::Element* f = field(s)) f->Focus();
        return uaui::key(m, wp, lp) || true;
    }
    if (m == WM_KEYUP) return !ctrl && !(wp >= VK_F1 && wp <= VK_F24);  // the hotbar's let go: the game's
    return m == WM_CHAR || m == WM_DEADCHAR;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.open || !s.shown) return false;
    if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_MBUTTONDOWN || m == WM_MOUSEWHEEL) padOff(s);  // .241
    if (m == WM_XBUTTONDOWN || m == WM_XBUTTONUP) {  // the mouse's back button over the window
        int bx = 0, by = 0, bw = 0, bh = 0;
        float tx = 0, ty = 0;
        if (!s.win.box(bx, by, bw, bh) || !pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr) ||
            tx < float(bx) || ty < float(by) || tx >= float(bx + bw) || ty >= float(by + bh))
            return false;
        if (m == WM_XBUTTONDOWN && GET_XBUTTON_WPARAM(wp) == XBUTTON1) goBack(s);
        result = TRUE;
        return true;
    }
    // 3.0.18-beta.240: the location's map - its wheel zooms round the pointer, a press drags it, a click on an NPC
    // opens its card (on a warp: its map's)
    float vx = 0, vy = 0, vw = 0, vh = 0, ux = 0, uy = 0;
    if (locationBox(s, vx, vy, vw, vh) &&
        pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, ux, uy, nullptr)) {
        const float lx = ux - vx, ly = uy - vy;
        const bool in = lx >= 0 && ly >= 0 && lx < vw && ly < vh;
        if (in && m == WM_MOUSEWHEEL) {
            minimap::zoomMapView(s.mv, s.locScene.map, lx, ly,
                                 std::pow(1.25f, float(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA),
                                 minimap::mapViewMost(s.locSharp));
            locationPaint(s);
            result = 0;
            return true;
        }
        if (in && (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK)) {
            s.locDrag = true;
            s.locDragged = false;
            s.locDragX = ux;
            s.locDragY = uy;
            s.locDragMark = minimap::mapViewMarkAt(s.mv, s.locScene, lx, ly);
            result = 0;
            return true;
        }
        if (m == WM_LBUTTONUP && s.locDrag) {
            const bool click = !s.locDragged;
            const int mark = s.locDragMark;
            s.locDrag = s.locDragged = false;
            if (click) openMark(s, mark);
            result = 0;
            return true;
        }
    }
    // 3.0.11-beta.29: the world map - its wheel zooms round the pointer, a press drags it, a click opens a map's card
    float mx = 0, my = 0, mw = 0, mh = 0, tx = 0, ty = 0;
    if (s.world && mapBox(s, mx, my, mw, mh) &&
        pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) {
        const float lx = tx - mx, ly = ty - my;
        const bool in = lx >= 0 && ly >= 0 && lx < mw && ly < mh;
        if (in && m == WM_MOUSEWHEEL) {
            worldmap::zoomAt(s.world->sheet.w, s.world->sheet.h, s.mapW, s.mapH, s.zoom, s.cx, s.cy, lx, ly,
                             std::pow(1.25f, float(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA));
            worldChanged(s);
            result = 0;
            return true;
        }
        if (in && (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK)) {
            const refwin::Under u = refwin::worldUnder(worldLayout(s), panel::Box{0, 0, mw, mh}, worldLook(s, false),
                                                       lx, ly);
            const worldmap::Fit v = worldmap::view(s.world->sheet.w, s.world->sheet.h, s.mapW, s.mapH, s.zoom, s.cx,
                                                   s.cy);
            s.dragging = true;
            s.dragged = false;
            s.dragX = tx;
            s.dragY = ty;
            s.dragCx = v.s > 0 ? (float(s.mapW) / 2 - v.x) / v.s : s.cx;
            s.dragCy = v.s > 0 ? (float(s.mapH) / 2 - v.y) / v.s : s.cy;
            s.dragTile = u.part == refwin::Part::Tile ? u.tile : -1;
            s.dragChain = u.part == refwin::Part::Chain ? u.tile : -1;
            result = 0;
            return true;
        }
        if (m == WM_LBUTTONUP && s.dragging) {
            const bool click = !s.dragged;
            s.dragging = false;
            if (click && s.dragChain >= 0 && size_t(s.dragChain) < s.way.chain.size())  // .64: its card
                go(s, refpages::Link{refpages::Go::Map, 0, s.way.chain[size_t(s.dragChain)].first});
            else if (click && s.dragTile >= 0 && size_t(s.dragTile) < s.world->sheet.world.size())
                go(s, refpages::Link{refpages::Go::Map, 0, s.world->sheet.world[size_t(s.dragTile)].map});
            result = 0;
            return true;
        }
    }
    const int taken = s.win.mouse(game, m, wp, lp);  // RmlUi's: the rows' clicks, the scroll, the drag by the ground
    if (!taken) return false;
    result = 0;
    return true;
}

void openMap(const std::string& code) {
    State& s = state();
    if (!s.enabled || code.empty()) return;
    openWindow(s, refpages::Link{refpages::Go::Map, 0, code});
    LOG_INFO("reference: %s's card from the minimap's line", code.c_str());
}

bool isOpen() { return state().open && state().shown; }

bool on() { return state().enabled; }

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.open) return;
    if (c != PadCommand::Close) padOn(s);
    switch (c) {
        case PadCommand::Up: padMove(s, padnav::Dir::Up); break;
        case PadCommand::Down: padMove(s, padnav::Dir::Down); break;
        case PadCommand::Left: padMove(s, padnav::Dir::Left); break;
        case PadCommand::Right: padMove(s, padnav::Dir::Right); break;
        case PadCommand::Enter: padPress(s); break;
        case PadCommand::Back:
            if (s.padCross) crossOff(s);
            else if (s.back.empty()) close(s);
            else goBack(s);
            break;
        case PadCommand::Close: close(s); break;
        case PadCommand::PageUp: scrollBy(s, -1); break;
        case PadCommand::PageDown: scrollBy(s, +1); break;
        case PadCommand::Lead: padLead(s); break;
        case PadCommand::Search: padSearch(s); break;
        case PadCommand::PrevSection: padSection(s, -1); break;
        case PadCommand::NextSection: padSection(s, +1); break;
    }
}

void padSticks(float lx, float ly, float rx, float ry, float seconds) {
    State& s = state();
    if (!s.open || !s.shown) return;
    if (std::hypot(rx, ry) > 0.5f || (s.padCross && std::hypot(lx, ly) > 0.5f)) padOn(s);
    padSticksIn(s, lx, ly, rx, ry, seconds);
}

bool padCrossOn() {
    const State& s = state();
    return s.open && s.padCross;
}

void toggleFromPad() {
    State& s = state();
    if (!s.enabled) {
        LOG_INFO("reference: a pad button asked for it - off ([interface] reference = 0)");
        return;
    }
    if (s.open) {
        close(s);
    } else {
        openWindow(s, refpages::Link{});
        if (s.open) padOn(s);  // 3.0.18-beta.241: the frame on its first control at once
    }
}

void openMob(int id) {
    State& s = state();
    if (!s.enabled || id <= 0) return;
    openWindow(s, refpages::Link{refpages::Go::Mob, id, ""});
    LOG_INFO("reference: monster %d's card from the minimap's list", id);
}

void openQuest(const std::string& slug) {
    State& s = state();
    const auto ix = reference::index();
    if (!s.enabled || !ix) return;
    for (size_t i = 0; i < ix->quests.size(); ++i)
        if (ix->quests[i].slug == slug) {
            openWindow(s, refpages::Link{refpages::Go::Quest, int(i), ""});
            LOG_INFO("reference: the quest %s's card from the tracker", slug.c_str());
            return;
        }
}

void openItem(int id) {
    State& s = state();
    if (!s.enabled || id <= 0) return;
    openWindow(s, refpages::Link{refpages::Go::Item, id, ""});
    LOG_INFO("reference: item %d's card from the tracker", id);
}

bool box(int& x, int& y, int& w, int& h) {  // 3.0.11-beta.109
    State& s = state();
    if (!s.shown) return false;
    return s.win.box(x, y, w, h);
}

}  // namespace uagfx::win::referencewin
