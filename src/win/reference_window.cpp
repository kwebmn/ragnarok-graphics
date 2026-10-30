#include "win/reference_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/layout.hpp"
#include "core/pack_source.hpp"
#include "core/reference_pages.hpp"
#include "core/reference_route.hpp"
#include "core/reference_window.hpp"
#include "core/settings/strings.hpp"
#include "core/sprite_file.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/hotbar.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/monster_picture.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/ui_cost.hpp"
#include "win/updates_hook.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::referencewin {
namespace {

using settings::Text;

constexpr size_t kMostTyped = 40;
constexpr size_t kMostRecent = 8, kMostKept = 40;  // 3.0.11-beta.18: «Нещодавні», «Обране» at most
constexpr size_t kMostPictures = 2000;             // 3.0.11-beta.22: the cards' pictures and icons kept at most

struct Entry {  // a page left for another: what «Назад» brings back
    refpages::Link at;
    std::string query, name;
    int scroll = 0;
    unsigned opened = 0;  // its long sections opened
    int sel = -1;         // 3.0.11-beta.28: the row that led away (picked again on the way back)
};

struct State {
    WindowShadow shadow;  // 3.0.11-beta.49
    bool enabled = false, open = false, shown = false, dirty = true, upload = true, wantPage = true;
    int style = -1;  // 3.0.11-beta.47: core/ui_style's generation painted
    WPARAM skipChar = 0;  // the character of the key that opened or closed it: neither typed nor the game's
    std::string query;    // typed (ASCII: the names are English)
    refpages::Link at;    // the page: Go::None - the search's (the first screen when nothing is typed)
    unsigned opened = 0;  // the card's long sections opened (refpages::Part bits)
    std::vector<Entry> back;
    refpages::Page page;
    std::vector<refwin::Placed> rows;
    int total = 0, scroll = 0;
    refwin::Layout layout;
    refwin::Under hover;
    std::shared_ptr<const refindex::Index> index;
    std::shared_ptr<const reflive::Online> online;  // 3.0.11-beta.10: the players now (null: none fresh)
    std::shared_ptr<const reflive::Live> live;      // 3.0.11-beta.12: MVP timers, the market (null: none fresh)
    ULONGLONG madeAt = 0;                           // the page's minutes: made again each minute with timers
    std::shared_ptr<const reference::World> world;  // 3.0.11-beta.11: the world map (null: not made yet)
    panel::Picture worldPicture;                    // its picture at its row's size (3.0.11-beta.29: at the view)
    std::string worldHover;                         // the map under the pointer there
    // 3.0.11-beta.29 (the concept's v19_route): the world map's view - its zoom and centre (worldmap::view), set on
    // the way or the maps marked when a page of it opens; the drag that moves it (a press not moved: a click)
    float zoom = 1, cx = -1, cy = -1;
    std::string viewOf;            // the page the view was set for
    worldmap::Fit pictureFit;      // the fit the picture was made at
    ULONGLONG worldMadeAt = 0;     // made again at most every 33 ms while dragged
    bool dragging = false, dragged = false;
    float dragX = 0, dragY = 0, dragCx = 0, dragCy = 0;  // where it began (the window's pixels), the centre then
    int dragTile = -1;             // the tile pressed: its card on a click
    refpages::WorldWay way;        // what the map shows of the page's way (made with the page)
    // 3.0.11-beta.64: the dungeon box's pictures - the maps' own minimaps at its side, found once each (none: empty)
    std::map<std::string, panel::Picture> chainPictures;
    int chainSide = 0, chainLogged = 0;
    std::vector<std::pair<std::string, std::string>> labels;  // the towns' names (made with the index)
    const refindex::Index* labelsOf = nullptr;
    // 3.0.11-beta.14: the cards' pictures - the client's sprites and item icons, found once each (none: an empty one)
    std::unique_ptr<gamefiles::Reader> files;
    std::map<std::string, panel::Picture> pictures;
    std::map<int, std::string> itemFiles;  // the client's data\idnum2itemresnametable.txt: item -> its icon's name
    bool itemFilesRead = false;
    unsigned folded = 0;  // 3.0.11-beta.17: the sections the player folded (refpages::Part bits), kept in a file
    bool foldedRead = false;
    // 3.0.11-beta.18: the cards opened lately (the latest first) and the starred ones, kept in files
    std::vector<refpages::Link> recent, favourites;
    int sel = -1;  // 3.0.11-beta.21: the row picked by the arrows (or the pad's D-pad), -1 none
    // 3.0.11-beta.25 (R-77TCM7): the highlights' areas to paint again, and those painted but not on the card yet
    std::vector<Rect> hot, parts;
    refwin::Chrome chrome;  // 3.0.11-beta.26: the header's and the bottom's places, as the picture has them
    std::string here;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, targetW = 0, targetH = 0;
    float k = 0, scale = 0;  // 3.0.11-beta.67: the layout's (uistyle::readingScale)
    std::vector<barhint::Skill> bar;  // 3.0.11-beta.68: the hotbar's skills, read with each page
};

State& state() {
    static State s;
    return s;
}

std::string say(Text t) { return settings::tr(startup().lang, t); }

// 3.0.11-beta.14: a card's picture - a monster's standing frame from its sprite (data\sprite\몬스터 or npc), an
// item's icon (data\texture\유저인터페이스\item) - fitted into the Portrait's square
const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스

bool readFile(State& s, const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // 3.0.11-beta.15: as the game finds it (its zips too)
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!s.files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

// 3.0.11-beta.64 (the concept's v19_route): a map's minimap for the world map's dungeon box - the client's own picture
// (data\texture\유저인터페이스\map\<code>.bmp, as the world map's tiles), `side` square; null: none in its files
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

void loadPicture(State& s, const std::string& key, int side) {
    if (s.pictures.count(key) || !s.index) return;
    if (s.pictures.size() >= kMostPictures) s.pictures.clear();  // 3.0.11-beta.22: a long browse never piles up
    panel::Picture& out = s.pictures[key];  // empty until found: not looked for again
    const bool mobIcon = key.rfind("icon:mob:", 0) == 0;  // 3.0.11-beta.26: a chip's monster, the icon's size
    const int box = mobIcon ? side : std::max(16, side - int(8 * s.layout.s));  // the square less its edge
    if (key.rfind("mob:", 0) == 0 || mobIcon) {
        const auto m = s.index->mobs.find(std::atoi(key.c_str() + (mobIcon ? 9 : 4)));
        if (m == s.index->mobs.end()) return;
        std::string from;
        if (monsterPicture(m->second, box, out, &from)) {  // 3.0.11-beta.38: win/monster_picture
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
        spritefile::Image im;
        im.w = w;
        im.h = h;
        im.argb = std::move(argb);
        out = asPicture(spritefile::fitted(im, box, box));
        return;
    }
    const bool icon = key.rfind("icon:item:", 0) == 0;  // 3.0.11-beta.15: a row's icon, its own size
    if (key.rfind("item:", 0) == 0 || icon) {
        if (!s.itemFilesRead) {  // «501#빨간포션#» a line
            s.itemFilesRead = true;
            std::string text;
            if (readFile(s, "data\\idnum2itemresnametable.txt", text)) {
                size_t at = 0;
                while (at < text.size()) {
                    size_t end = text.find('\n', at);
                    if (end == std::string::npos) end = text.size();
                    const std::string line = text.substr(at, end - at);
                    at = end + 1;
                    const size_t a = line.find('#'), b = a == std::string::npos ? a : line.find('#', a + 1);
                    if (b != std::string::npos && a > 0)
                        s.itemFiles[std::atoi(line.c_str())] = line.substr(a + 1, b - a - 1);
                }
            }
            LOG_INFO("reference: %zu item icons named by the client's idnum2itemresnametable.txt", s.itemFiles.size());
        }
        const auto f = s.itemFiles.find(std::atoi(key.c_str() + (icon ? 10 : 5)));
        if (f == s.itemFiles.end()) return;
        std::string bmp;
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        if (!readFile(s, "data\\texture\\" + kUi + "\\item\\" + f->second + ".bmp", bmp) ||
            !pack::decodeBmp(reinterpret_cast<const uint8_t*>(bmp.data()), bmp.size(), argb, w, h))
            return;
        spritefile::Image im;
        im.w = w;
        im.h = h;
        im.argb = std::move(argb);
        for (uint32_t& c : im.argb)  // the game's magenta: see-through
            if ((c & 0xFFFFFF) == 0xFF00FF) c = 0;
        out = icon ? asPicture(spritefile::fitted(im, side, side, 1))
                   : asPicture(spritefile::fitted(im, std::min(box, 72), std::min(box, 72)));
    }
}

// What a page is made with: the player's place, the minimap's way, the live data, the world map (3.0.11-beta.29:
// the world map's look too)
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
    c.bar = &s.bar;        // 3.0.11-beta.68: the hotbar's skills (read with each page)
    return c;
}

int worldRow(const State& s) {
    for (size_t i = 0; i < s.rows.size() && i < s.page.rows.size(); ++i)
        if (s.page.rows[i].style == refpages::Style::World) return int(i);
    return -1;
}

// 3.0.11-beta.29: the world map's picture at the view (the sheet's pictures at their detail) - made again when the
// view or the row's size changed; false when it is as it was
bool worldPictureMade(State& s) {
    const int row = worldRow(s);
    if (row < 0 || !s.world) return false;
    const refwin::Placed& r = s.rows[size_t(row)];
    const worldmap::Sheet& sh = s.world->sheet;
    const worldmap::Fit v = worldmap::view(sh.w, sh.h, r.w, r.h, s.zoom, s.cx, s.cy);
    if (s.worldPicture.width == r.w && s.worldPicture.height == r.h && v.s == s.pictureFit.s &&
        v.x == s.pictureFit.x && v.y == s.pictureFit.y)
        return false;
    const int d = reference::World::detail;
    worldmap::scaled(s.worldPicture, s.world->argb, sh.w * d, sh.h * d, r.w, r.h, worldmap::Fit{v.s / d, v.x, v.y});
    s.pictureFit = v;
    s.worldMadeAt = GetTickCount64();
    return true;
}

// The view moved or zoomed: its centre kept where the view puts it (no drifting past the sheet's edges), the
// picture made again, the map's row painted again
void worldChanged(State& s) {
    const int row = worldRow(s);
    if (row < 0 || !s.world) return;
    const refwin::Placed& r = s.rows[size_t(row)];
    const worldmap::Sheet& sh = s.world->sheet;
    const worldmap::Fit v = worldmap::view(sh.w, sh.h, r.w, r.h, s.zoom, s.cx, s.cy);
    if (s.zoom > 1 && v.s > 0) {
        s.cx = (float(r.w) / 2 - v.x) / v.s;
        s.cy = (float(r.h) / 2 - v.y) / v.s;
    }
    if (!worldPictureMade(s) || s.dirty) return;
    if (const Rect a = refwin::rowArea(s.layout, s.page, s.rows, s.scroll, row); !a.empty()) s.hot.push_back(a);
}

void makePage(State& s) {
    const settings::Lang l = startup().lang;
    if (!s.index) {
        s.page = refpages::Page{};
        s.page.name = say(Text::RefTitle);
        refpages::Row r;
        r.style = refpages::Style::Note;
        r.left = say(Text::RefNoData);
        s.page.rows.push_back(r);
    } else {
        s.bar = hotbarwin::barSkills();  // 3.0.11-beta.68: a monster's card says the bar's skills on it
        const refpages::Context c = contextOf(s);
        s.page = s.at.go != refpages::Go::None ? refpages::open(*s.index, l, s.at, c, s.opened)
                 : !s.query.empty()            ? refpages::found(*s.index, l, s.query, c, s.opened)
                                               : refpages::home(*s.index, l, c);
    }
    const bool first = s.at.go == refpages::Go::None && s.query.empty();  // 3.0.11-beta.26: the concept's rail
    if (s.index && !first && s.at.go != refpages::Go::World) s.page = refpages::withRail(s.page, *s.index, l, s.at);
    s.rows = refwin::place(s.layout, s.page, s.total);
    s.scroll = refwin::clampScroll(s.scroll, s.total, s.layout.bodyH);
    s.madeAt = GetTickCount64();
    if (s.sel >= int(s.page.rows.size()) || (s.sel >= 0 && s.page.rows[size_t(s.sel)].link.go == refpages::Go::None))
        s.sel = -1;
    if (s.sel < 0 && s.at.go == refpages::Go::None && !s.query.empty())  // 3.0.11-beta.28: the first result picked:
        for (size_t i = 0; i < s.page.rows.size(); ++i)                    // Enter's (the concept's v21_search)
            if (s.page.rows[i].style == refpages::Style::Result) {
                s.sel = int(i);
                break;
            }
    for (const refpages::Row& r : s.page.rows)  // 3.0.11-beta.14
        if (r.style == refpages::Style::Portrait && !r.picture.empty())
            loadPicture(s, r.picture, refwin::portraitSide(s.layout));
    if (const int row = worldRow(s); row >= 0 && s.world && s.index) {  // 3.0.11-beta.11: the world's picture
        const refpages::Context c = contextOf(s);
        s.way = refpages::worldWayOf(*s.index, l, c, s.at.map);  // 3.0.11-beta.29: its way, its view
        const std::string key = refpages::linkText(s.at) + "|" + std::to_string(s.at.id) + "|" + s.at.map;
        if (key != s.viewOf) {  // a page of it opened: the view on its way or its marks, else the whole world
            s.viewOf = key;
            s.zoom = 1;
            s.cx = s.cy = -1;
            float vl = 0, vt = 0, vr = 0, vb = 0;
            const refwin::Placed& r = s.rows[size_t(row)];
            const worldmap::Sheet& sh = s.world->sheet;
            if (refpages::worldFocus(sh, s.way, refpages::worldMarks(*s.index, s.at.map), vl, vt, vr, vb))
                worldmap::viewOn(sh.w, sh.h, r.w, r.h, vl, vt, vr, vb, s.zoom, s.cx, s.cy);
        }
        worldPictureMade(s);
    }
    s.hover = refwin::Under{};
    s.wantPage = false;
    s.dirty = true;
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

void go(State& s, const refpages::Link& to) {
    if (to.go == refpages::Go::Zoom) {  // 3.0.11-beta.29: the world map's «-», «+» round its middle, the whole
        const int row = worldRow(s);
        if (row < 0 || !s.world) return;
        const refwin::Placed& r = s.rows[size_t(row)];
        const worldmap::Sheet& sh = s.world->sheet;
        if (to.id == 0) {
            s.zoom = 1;
            s.cx = s.cy = -1;
        } else {
            worldmap::zoomAt(sh.w, sh.h, r.w, r.h, s.zoom, s.cx, s.cy, float(r.w) / 2, float(r.h) / 2,
                             to.id > 0 ? 1.5f : 1 / 1.5f);
        }
        worldChanged(s);
        return;
    }
    if (to.go == refpages::Go::Expand) {  // «усі N ›»: the section in full, in place
        s.opened |= 1u << unsigned(to.id);
        makePage(s);
        return;
    }
    if (to.go == refpages::Go::Star) {  // 3.0.11-beta.18: the card shown into «Обране» or out of it
        const auto it = std::find_if(s.favourites.begin(), s.favourites.end(),
                                     [&](const refpages::Link& k) { return refpages::sameCard(k, s.at); });
        if (it != s.favourites.end()) s.favourites.erase(it);
        else if (isCard(s.at) && s.favourites.size() < kMostKept) s.favourites.insert(s.favourites.begin(), s.at);
        keepList(s.favourites, layout::kReferenceFavourites);
        makePage(s);
        return;
    }
    if (to.go == refpages::Go::Fold) {  // 3.0.11-beta.17: a section folded or back, in every card, kept for later
        s.folded ^= 1u << unsigned(to.id);
        const std::string path = layout::join(gameDir(), layout::kReferenceFolded);
        const std::string text = std::to_string(s.folded) + "\n";
        if (!writeFileAtomic(path, text.data(), text.size()))
            LOG_INFO("reference: %s cannot be written", path.c_str());
        makePage(s);
        return;
    }
    if (to.go == refpages::Go::Follow) {  // 3.0.11-beta.9: the way on the minimap
        std::string target;
        bool walk = true;
        if (minimapproto::following(target, walk) && target == to.map && walk == (to.id == 0))
            minimapproto::unfollow();
        else
            minimapproto::follow(to.map, to.id == 0);
        // 3.0.11-beta.31 (the owner: «когда выбрать маршрут то справочник закрывается»): the window stays, its page
        // says it is followed («веду · зупинити ›», «Не вести»)
        makePage(s);
        return;
    }
    int from = s.sel;  // 3.0.11-beta.28: a result opened by a click - picked when back at the results
    for (size_t i = 0; i < s.page.rows.size(); ++i)
        if (refpages::sameCard(s.page.rows[i].link, to)) {
            from = int(i);
            break;
        }
    s.back.push_back(Entry{s.at, s.query, s.page.name, s.scroll, s.opened, from});
    s.at = to;
    s.scroll = 0;
    s.opened = 0;
    s.sel = -1;
    noteRecent(s);  // 3.0.11-beta.18
    makePage(s);
}

// 3.0.11-beta.26: the rail's «Усе» - the first screen (the search emptied), the page left kept for «Назад»
void goFirst(State& s) {
    if (s.at.go == refpages::Go::None && s.query.empty()) return;
    s.back.push_back(Entry{s.at, s.query, s.page.name, s.scroll, s.opened});
    s.at = refpages::Link{};
    s.query.clear();
    s.scroll = 0;
    s.opened = 0;
    s.sel = -1;
    makePage(s);
}

// 3.0.11-beta.21: the arrows' (and the pad's D-pad's) row - the next one that leads somewhere, kept in view
// 3.0.11-beta.25 (R-77TCM7: «когда по меню вожу, лагат курсор и фпс падает»): the
// highlights moved - another row under the pointer, another one picked - so their old areas and their new ones are
// painted again, not the whole window (290 ms at 4K for each row the pointer went to)
void highlight(State& s, const refwin::Under& hover, int sel) {
    for (int pass = 0; pass < 2 && !s.dirty; ++pass) {
        refwin::Look look;
        look.hover = s.hover;
        look.chrome = s.chrome;
        look.selected = s.sel;
        for (const Rect& a : refwin::hotAreas(s.layout, s.page, s.rows, s.scroll, look)) s.hot.push_back(a);
        s.hover = hover;
        s.sel = sel;
    }
    s.hover = hover;
    s.sel = sel;
}

void step(State& s, int by) {
    const int n = int(s.page.rows.size());
    int i = s.sel;
    for (int k = 0; k < n; ++k) {
        i = i < 0 ? (by > 0 ? 0 : n - 1) : i + by;
        if (i < 0 || i >= n) return;
        if (s.page.rows[size_t(i)].link.go != refpages::Go::None) break;
    }
    if (i < 0 || i >= n || s.page.rows[size_t(i)].link.go == refpages::Go::None) return;
    const refwin::Placed& r = s.rows[size_t(i)];
    int scroll = s.scroll;
    if (r.y < scroll) scroll = refwin::clampScroll(r.y - r.h, s.total, s.layout.bodyH);
    if (r.y + r.h > scroll + s.layout.bodyH)
        scroll = refwin::clampScroll(r.y + 2 * r.h - s.layout.bodyH, s.total, s.layout.bodyH);
    if (scroll == s.scroll) {
        highlight(s, s.hover, i);
        return;
    }
    s.sel = i;
    s.scroll = scroll;
    s.dirty = true;
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
    s.query = e.query;
    s.scroll = e.scroll;
    s.opened = e.opened;
    s.sel = e.sel;
    makePage(s);
}

void typed(State& s) {  // the query changed: its results (a card left for them comes back by «Назад»)
    if (s.at.go != refpages::Go::None) s.back.push_back(Entry{s.at, "", s.page.name, s.scroll, s.opened});
    s.at = refpages::Link{};
    s.opened = 0;  // 3.0.11-beta.28: the groups closed, the first result picked again
    s.sel = -1;
    s.scroll = 0;
    makePage(s);
}

void scrollBy(State& s, int pixels) {
    const int next = refwin::clampScroll(s.scroll + pixels, s.total, s.layout.bodyH);
    if (next == s.scroll) return;
    s.scroll = next;
    s.hover = refwin::Under{};
    s.dirty = true;
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
    s.query.clear();
    s.back.clear();
    s.at = at;
    s.scroll = 0;
    s.opened = 0;
    s.wantPage = true;
    s.dirty = true;
    noteRecent(s);  // 3.0.11-beta.18: a card from the minimap's lines too
}

// 3.0.11-beta.26: the chips' and the way's words measured as drawWords draws them
float measured(const std::string& utf8, float px, bool bold) { return float(wordsWidth(utf8Wide(utf8), px, bold)); }

void close(State& s) {
    s.open = false;
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, nullptr, false);
    s.shown = false;
}

// 3.0.11-beta.11: what goes over the world map - you, the way followed, the players now, the map under the pointer
// 3.0.11-beta.29: what finds the map under the pointer - the sheet at the view (none off the world map's page)
refwin::WorldLook worldHit(const State& s) {
    refwin::WorldLook w;
    if (!s.world || worldRow(s) < 0) return w;
    w.sheet = &s.world->sheet;
    w.zoom = s.zoom;
    w.cx = s.cx;
    w.cy = s.cy;
    for (const auto& [map, name] : s.way.chain) w.chain.push_back(refwin::ChainMap{map, name, nullptr});  // .64
    w.chainTitle = s.way.chainTitle;
    return w;
}

refwin::WorldLook worldLook(State& s) {
    refwin::WorldLook w;
    if (!s.world || !s.index) return w;
    w.picture = &s.worldPicture;
    w.sheet = &s.world->sheet;
    w.here = s.here;
    w.hover = s.worldHover;
    if (s.at.go == refpages::Go::World) w.marks = refpages::worldMarks(*s.index, s.at.map);  // 3.0.11-beta.19
    if (s.online) w.players = &s.online->maps;
    int x = -1, y = -1, mw = 0, mh = 0;
    const bool cell = readPlayerCell(x, y, mw, mh);
    if (const worldmap::Tile* t = worldmap::tileOf(s.world->sheet, s.here); t && cell)
        worldmap::cellOnSheet(*t, mw, mh, x, y, w.hereX, w.hereY);
    // 3.0.11-beta.29: the concept's v19_route - the view, the way made with the page, the towns' names, the card of
    // the map under the pointer
    w.zoom = s.zoom;
    w.cx = s.cx;
    w.cy = s.cy;
    w.hops = s.way.hops;
    w.target = s.way.target;
    w.targetLabel = s.way.targetLabel;
    w.hereLabel = s.way.hereLabel;
    // 3.0.11-beta.64: a dungeon's way - its maps in a box at the map's bottom left, with their own minimaps
    const int side = refwin::chainSide(s.layout);
    for (const auto& [map, name] : s.way.chain)
        w.chain.push_back(refwin::ChainMap{map, name, chainPicture(s, map, side)});
    w.chainTitle = s.way.chainTitle;
    if (s.labelsOf != s.index.get()) {
        s.labels = refpages::worldLabels(*s.index);
        s.labelsOf = s.index.get();
    }
    w.labels = s.labels;
    if (!s.worldHover.empty()) w.tip = refpages::worldTip(*s.index, startup().lang, s.worldHover, contextOf(s));
    return w;
}

// What the window is painted with - 3.0.11-beta.25: the world map's only on its page (its way found anew each time)
refwin::Look lookOf(State& s) {
    refwin::Look look{s.hover, s.chrome, {}, &s.pictures};
    for (const refpages::Row& r : s.page.rows)
        if (r.style == refpages::Style::World) {
            look.world = worldLook(s);
            break;
        }
    look.selected = s.sel;  // 3.0.11-beta.21
    return look;
}

// 3.0.11-beta.26: the words around the page - the title, «Назад до …», the way here (the pages left but the
// first screen, then this one), the searches', the data's chips (the pack; the players now and how fresh, or no
// link), the keys (a card's or a list's)
refwin::Texts textsOf(const State& s) {
    const settings::Lang l = startup().lang;
    refwin::Texts t;
    t.title = say(Text::RefTitle);
    if (!s.back.empty()) t.back = settings::fmt(settings::tr(l, Text::RefBack), {s.back.back().name});
    for (const Entry& e : s.back)
        if (!e.name.empty() && e.name != t.title) t.crumbs.push_back(e.name);
    if (!s.back.empty() && !s.page.name.empty()) t.crumbs.push_back(s.page.name);
    t.query = s.query;
    t.hint = say(Text::RefSearchHint);
    t.find = say(Text::RefFind);
    if (s.index)
        t.pack = settings::fmt(settings::tr(l, Text::RefFooter),
                               {s.index->date.substr(0, 10), s.index->commit.substr(0, 8)});
    if (s.online) {  // 3.0.11-beta.20: how fresh the players now (and the timers, the market) are
        const long long age = std::max<long long>(0, static_cast<long long>(std::time(nullptr)) - s.online->at);
        t.live = settings::fmt(settings::tr(l, Text::RefLiveNow),
                               {refpages::number(s.online->online, l),
                                refpages::respawn(int(std::max<long long>(age, 60) * 1000), l)});
        t.liveState = refwin::Live::Fresh;
    } else {
        t.live = say(Text::RefLiveOff);
        t.liveState = refwin::Live::Off;
    }
    t.home = s.at.go != refpages::Go::None || !s.query.empty();  // 3.0.11-beta.30: «Головна» but on it
    t.keys = say(isCard(s.at)                                       ? Text::RefKeysCard
                 : s.at.go == refpages::Go::World && s.at.id == 0 ? Text::RefKeysWorld  // 3.0.11-beta.29
                                                                  : Text::RefKeysList);
    return t;
}

std::vector<Words> wordsOf(const State& s, const refwin::Look& look) {
    const refwin::Texts t = textsOf(s);
    std::vector<Words> gdi;
    for (const refwin::Words& w : refwin::words(s.layout, s.page, s.rows, s.scroll, t, look)) {
        const UINT align = w.align == refwin::Align::Right ? DT_RIGHT : w.align == refwin::Align::Center ? DT_CENTER
                                                                                                          : DT_LEFT;
        gdi.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold, align, w.wrap});
    }
    return gdi;
}

void render(State& s) {
    const uicost::Paint cost("«Довідник»", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    s.chrome = refwin::chrome(s.layout, s.page, textsOf(s));  // 3.0.11-beta.26
    const refwin::Look look = lookOf(s);
    refwin::paint(s.picture, s.layout, s.page, s.rows, s.scroll, s.total, look);
    drawWords(s.picture, wordsOf(s, look));
    s.dirty = false;
    s.upload = true;
    s.hot.clear();
    s.parts.clear();
}

// 3.0.11-beta.25: the highlights' areas only (highlight) - painted again, their words, sent to the card by draw
void renderHot(State& s) {
    const uicost::Paint cost("«Довідник»'s rows", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    const refwin::Look look = lookOf(s);
    const std::vector<Words> gdi = wordsOf(s, look);
    for (const Rect& a : s.hot) {
        refwin::repaint(s.picture, s.layout, s.page, s.rows, s.scroll, s.total, look, a);
        drawWords(s.picture, gdi, a);
        s.parts.push_back(a);
    }
    s.hot.clear();
}

bool toWindow(const State& s, HWND game, POINT client, float& wx, float& wy) {
    RECT r{};
    if (!GetClientRect(game, &r) || r.right <= 0 || r.bottom <= 0 || s.targetW <= 0) return false;
    wx = float(client.x) * float(s.targetW) / float(r.right) - float(s.layout.x);  // uaRO.exe is not DPI-aware
    wy = float(client.y) * float(s.targetH) / float(r.bottom) - float(s.layout.y);
    return true;
}

bool inside(const State& s, float wx, float wy) {
    return wx >= 0 && wy >= 0 && wx < float(s.layout.w) && wy < float(s.layout.h);
}

void block(const State& s, HWND game) {
    RECT client{};
    if (!GetClientRect(game, &client) || s.targetW <= 0 || s.targetH <= 0) return;
    const refwin::Layout& L = s.layout;
    const RECT r{LONG(int64_t(L.x) * client.right / s.targetW), LONG(int64_t(L.y) * client.bottom / s.targetH),
                 LONG(int64_t(L.x + L.w) * client.right / s.targetW + 1),
                 LONG(int64_t(L.y + L.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, game, &r, false);
}

// A key's own character for the search (the names are English): letters, digits, the space and a few signs by the
// key's place, whatever the layout types
char letterOf(WPARAM vk, bool shift) {
    if (vk >= 'A' && vk <= 'Z') return char(shift ? vk : vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9' && !shift) return char(vk);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return char('0' + (vk - VK_NUMPAD0));
    switch (vk) {
        case VK_SPACE: return ' ';
        case VK_OEM_MINUS: return shift ? '_' : '-';
        case VK_OEM_7: return '\'';
        case VK_OEM_PERIOD: return '.';
        case VK_OEM_COMMA: return ',';
        default: return 0;
    }
}

}  // namespace

void frame(HWND game, bool enabled, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.enabled = enabled;
    if (s.open && !enabled) {
        close(s);
        LOG_INFO("reference: off ([interface] reference = 0) - the window closed");
    }
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, nullptr, false);
        s.shown = false;
        return;
    }
    const std::string here = readGameMap();
    if (here != s.here) {  // 3.0.11-beta.8: «ви тут» and the routes are from the player's map
        s.here = here;
        s.wantPage = true;
    }
    // 3.0.11-beta.67: [interface] text_size's too - and laid out again when it or [interface] size changes (a new
    // size reached the open window only at its next game resolution)
    const float scale = uistyle::readingScale(k);
    if (float(k) != s.k || targetWidth != s.targetW || targetHeight != s.targetH || scale != s.scale) {
        s.k = float(k);
        s.scale = scale;
        s.targetW = targetWidth;
        s.targetH = targetHeight;
        s.layout = refwin::layout(targetWidth, targetHeight, scale, measured);
        s.pictures.clear();  // 3.0.11-beta.14: their size is the layout's
        s.wantPage = true;
    }
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
    if (s.wantPage) makePage(s);
    int cx = 0, cy = 0;  // the row under the game's cursor (it is drawn over the window)
    refwin::Under hover;
    const bool pointer = readGameCursor(cx, cy);
    const float px = float(cx) * float(k) - float(s.layout.x), py = float(cy) * float(k) - float(s.layout.y);
    if (s.dragging) {  // 3.0.11-beta.29: the world map dragged - the view moves with the pointer
        const int row = worldRow(s);
        const int left = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;  // the physical one pressed
        if (GetAsyncKeyState(left) >= 0 || row < 0 || !s.world) {
            s.dragging = false;  // let go outside the window
        } else if (pointer) {
            const float dx = px - s.dragX, dy = py - s.dragY;
            if (!s.dragged && std::abs(dx) + std::abs(dy) >= 5 * s.layout.s) s.dragged = true;
            const refwin::Placed& r = s.rows[size_t(row)];
            const worldmap::Fit v = worldmap::view(s.world->sheet.w, s.world->sheet.h, r.w, r.h, s.zoom, s.cx, s.cy);
            if (s.dragged && v.s > 0 && GetTickCount64() - s.worldMadeAt >= 33) {
                s.cx = s.dragCx - dx / v.s;
                s.cy = s.dragCy - dy / v.s;
                worldChanged(s);
            }
        }
    }
    const refwin::WorldLook view = worldHit(s);
    if (pointer) hover = refwin::under(s.layout, s.page, s.rows, s.scroll, s.chrome, px, py, &view);
    const std::string map = hover.part == refwin::Part::Tile && s.world && !(s.dragging && s.dragged)
                                ? s.world->sheet.world[size_t(hover.tile)].map
                                : std::string();
    if (map != s.worldHover) {  // the world map's tile under the pointer: its frame and its card over the map
        // 3.0.11-beta.25 (R-77TCM7), 3.0.11-beta.29: only the two maps' frames and cards are painted again (the
        // page stays as it is)
        const int row = worldRow(s);
        const bool clean = !s.dirty && s.world && row >= 0;
        if (clean)
            if (const Rect a = refwin::worldArea(s.layout, s.rows, s.scroll, row, worldLook(s)); !a.empty())
                s.hot.push_back(a);
        s.worldHover = map;
        if (clean)
            if (const Rect a = refwin::worldArea(s.layout, s.rows, s.scroll, row, worldLook(s)); !a.empty())
                s.hot.push_back(a);
    }
    if (hover.part != s.hover.part || hover.row != s.hover.row || hover.tile != s.hover.tile)
        highlight(s, hover, s.sel);
    int budget = 8;  // 3.0.11-beta.15: the icons of the rows in view, a few a frame (a long list never stalls)
    for (size_t i = 0; i < s.rows.size() && i < s.page.rows.size() && budget > 0; ++i) {
        const refpages::Row& r = s.page.rows[i];
        if (r.icon.empty() || r.icon.rfind("ui:", 0) == 0 || !refwin::shown(s.layout, s.rows[i], s.scroll) ||
            s.pictures.count("icon:" + r.icon))  // 3.0.11-beta.26: «ui:» - the core's own drawings
            continue;
        loadPicture(s, "icon:" + r.icon, refwin::iconSide(s.layout));
        --budget;
        // 3.0.11-beta.25 (R-77TCM7): the rows with this icon painted again, not the whole window - a long list's
        // icons come a few a frame
        for (size_t j = i; j < s.rows.size() && j < s.page.rows.size() && !s.dirty; ++j)
            if (s.page.rows[j].icon == r.icon)
                if (const Rect a = refwin::rowArea(s.layout, s.page, s.rows, s.scroll, int(j)); !a.empty())
                    s.hot.push_back(a);
    }
    if (s.style != uistyle::generation()) {  // 3.0.11-beta.47: its ground another opacity
        s.style = uistyle::generation();
        s.dirty = true;
    }
    if (s.dirty)
        render(s);
    else if (!s.hot.empty())
        renderHot(s);
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.picture.width <= 0 || s.k <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    // 3.0.11-beta.25 (R-77TCM7): the areas painted again since the last frame - only they go to the card (all of the
    // picture when its texture is not there yet)
    if (!s.upload && s.texture && s.texW == s.picture.width && s.texH == s.picture.height) {
        std::vector<uint32_t> px;
        for (const Rect& a : s.parts) {
            if (a.empty()) continue;
            px.resize(size_t(a.width()) * size_t(a.height()));
            for (int y = a.top; y < a.bottom; ++y)
                std::copy_n(s.picture.px.begin() + std::ptrdiff_t(size_t(y) * size_t(s.picture.width) + size_t(a.left)),
                            a.width(), px.begin() + std::ptrdiff_t(size_t(y - a.top) * size_t(a.width())));
            record.upload(s.texture, 0, a, px.data());
        }
    } else if (!s.parts.empty()) {
        s.upload = true;
    }
    s.parts.clear();
    const float sc = s.layout.s;  // 3.0.11-beta.49: its shadow under it (its ground the whole picture)
    s.shadow.draw(record, renderer, s.layout.x, s.layout.y, s.picture.width, s.picture.height, 10 * sc, sc, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.enabled) return false;
    const bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0, shift = GetKeyState(VK_SHIFT) < 0;
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
    // 3.0.11-beta.64 (the concept's v19_route: «Карта світу Ctrl+M»): the world map at once, and away again; the game
    // takes M with Alt alone (FUN_004d96e0) - Ctrl+M is free
    if (m == WM_KEYDOWN && wp == 'M' && ctrl && !alt) {
        const bool onIt = s.open && s.at.go == refpages::Go::World && s.at.id == 0;
        if (onIt) {
            close(s);
        } else if (s.open) {
            go(s, refpages::Link{refpages::Go::World, 0, ""});
        } else {
            openWindow(s, refpages::Link{refpages::Go::World, 0, ""});
        }
        LOG_INFO("reference: Ctrl+M - the world map %s", onIt ? "closed" : "opened");
        s.skipChar = 13;  // Ctrl+M's WM_CHAR is a carriage return - the chat's Enter
        return true;
    }
    if (!s.open) return false;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        switch (wp) {
            case VK_ESCAPE:
                close(s);
                s.skipChar = 27;
                return true;
            case VK_LEFT:
                if (alt) goBack(s);
                return true;
            case VK_BACK:
                if (!s.query.empty()) {
                    s.query.pop_back();
                    typed(s);
                } else if (s.at.go != refpages::Go::None || !s.back.empty()) {
                    goBack(s);
                }
                s.skipChar = 8;
                return true;
            case VK_DELETE:
                if (!s.query.empty()) {
                    s.query.clear();
                    typed(s);
                }
                return true;
            case VK_RETURN:  // the picked row, else the first that leads to a card
                enter(s);
                s.skipChar = 13;
                return true;
            case VK_PRIOR: scrollBy(s, -s.layout.bodyH * 9 / 10); return true;
            case VK_NEXT: scrollBy(s, s.layout.bodyH * 9 / 10); return true;
            case VK_UP: step(s, -1); return true;  // 3.0.11-beta.21: a row up, a row down
            case VK_DOWN: step(s, +1); return true;
            case VK_HOME: scrollBy(s, -s.total); return true;
            case VK_END: scrollBy(s, s.total); return true;
            default: break;
        }
        const char c = ctrl || alt ? 0 : letterOf(wp, shift);
        if (c && s.query.size() < kMostTyped && !(c == ' ' && (s.query.empty() || s.query.back() == ' '))) {
            s.query += c;
            typed(s);
        }
        return true;  // every key: the window's while it is open
    }
    return m == WM_KEYUP || m == WM_SYSKEYUP || m == WM_CHAR || m == WM_SYSCHAR || m == WM_DEADCHAR ||
           m == WM_SYSDEADCHAR;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.open || !s.shown) return false;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    float wx = 0, wy = 0;
    if (!toWindow(s, game, p, wx, wy) || !inside(s, wx, wy)) return false;  // outside: the game's
    switch (m) {
        case WM_MOUSEWHEEL: {
            const int row = worldRow(s);  // 3.0.11-beta.29: over the world map - its zoom round the pointer
            if (row >= 0 && s.world) {
                const refwin::Placed& r = s.rows[size_t(row)];
                const float rx = wx - float(r.x), ry = wy - float(s.layout.bodyY + r.y - s.scroll);
                if (rx >= 0 && ry >= 0 && rx < float(r.w) && ry < float(r.h)) {
                    worldmap::zoomAt(s.world->sheet.w, s.world->sheet.h, r.w, r.h, s.zoom, s.cx, s.cy, rx, ry,
                                     std::pow(1.25f, float(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA));
                    worldChanged(s);
                    break;
                }
            }
            scrollBy(s, -GET_WHEEL_DELTA_WPARAM(wp) * std::max(1, s.layout.bodyH / 8) / WHEEL_DELTA);
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            const refwin::WorldLook view = worldHit(s);
            const refwin::Under u = refwin::under(s.layout, s.page, s.rows, s.scroll, s.chrome, wx, wy, &view);
            if (u.part == refwin::Part::Close) close(s);
            if (u.part == refwin::Part::Back) goBack(s);
            if (u.part == refwin::Part::Home) goFirst(s);  // 3.0.11-beta.30: the first screen
            if (u.part == refwin::Part::Star) go(s, refpages::Link{refpages::Go::Star, 0, ""});  // 3.0.11-beta.26
            if (u.part == refwin::Part::Rail && u.row == 0) goFirst(s);
            if (u.part == refwin::Part::Rail && u.row > 0 && size_t(u.row) < s.page.rail.size())
                go(s, s.page.rail[size_t(u.row)].link);
            if (u.part == refwin::Part::Row) go(s, s.page.rows[size_t(u.row)].link);
            if (u.part == refwin::Part::Chain && u.tile >= 0 && size_t(u.tile) < s.way.chain.size())  // .64: its card
                go(s, refpages::Link{refpages::Go::Map, 0, s.way.chain[size_t(u.tile)].first});
            const int row = worldRow(s);  // 3.0.11-beta.29: a press on the world map - a drag, or a click on letting go
            if (row >= 0 && s.world && (u.part == refwin::Part::Tile || u.part == refwin::Part::Body)) {
                const refwin::Placed& r = s.rows[size_t(row)];
                const float ry = wy - float(s.layout.bodyY + r.y - s.scroll);
                if (wx >= float(r.x) && wx < float(r.x + r.w) && ry >= 0 && ry < float(r.h)) {
                    const worldmap::Fit v = worldmap::view(s.world->sheet.w, s.world->sheet.h, r.w, r.h, s.zoom,
                                                           s.cx, s.cy);
                    s.dragging = true;
                    s.dragged = false;
                    s.dragX = wx;
                    s.dragY = wy;
                    s.dragCx = v.s > 0 ? (float(r.w) / 2 - v.x) / v.s : s.cx;
                    s.dragCy = v.s > 0 ? (float(r.h) / 2 - v.y) / v.s : s.cy;
                    s.dragTile = u.part == refwin::Part::Tile ? u.tile : -1;
                }
            }
            break;
        }
        case WM_XBUTTONDOWN:
            if (GET_XBUTTON_WPARAM(wp) == XBUTTON1) goBack(s);
            result = TRUE;
            return true;
        case WM_XBUTTONUP:
            result = TRUE;
            return true;
        case WM_LBUTTONUP:  // 3.0.11-beta.29: a press on a map not dragged - its card
            if (s.dragging && !s.dragged && s.dragTile >= 0 && s.world &&
                size_t(s.dragTile) < s.world->sheet.world.size()) {
                s.dragging = false;
                go(s, refpages::Link{refpages::Go::Map, 0, s.world->sheet.world[size_t(s.dragTile)].map});
                break;
            }
            s.dragging = false;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            break;
        default:
            return false;
    }
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

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.open) return;
    switch (c) {
        case PadCommand::Up: step(s, -1); break;
        case PadCommand::Down: step(s, +1); break;
        case PadCommand::Enter: enter(s); break;
        case PadCommand::Back:
            if (s.back.empty()) close(s);
            else goBack(s);
            break;
        case PadCommand::Close: close(s); break;
        case PadCommand::PageUp: scrollBy(s, -s.layout.bodyH * 9 / 10); break;
        case PadCommand::PageDown: scrollBy(s, s.layout.bodyH * 9 / 10); break;
    }
}

void toggleFromPad() {
    State& s = state();
    if (!s.enabled) {
        LOG_INFO("reference: a pad button asked for it - off ([interface] reference = 0)");
        return;
    }
    if (s.open) close(s);
    else openWindow(s, refpages::Link{});
}

void openMob(int id) {
    State& s = state();
    if (!s.enabled || id <= 0) return;
    openWindow(s, refpages::Link{refpages::Go::Mob, id, ""});
    LOG_INFO("reference: monster %d's card from the minimap's list", id);
}

}  // namespace uagfx::win::referencewin
