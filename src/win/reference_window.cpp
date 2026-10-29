#include "win/reference_window.hpp"

#include <windowsx.h>

#include <algorithm>
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
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::referencewin {
namespace {

using settings::Text;

constexpr size_t kMostTyped = 40;
constexpr size_t kMostRecent = 8, kMostKept = 40;  // 3.0.11-beta.18: «Нещодавні», «Обране» at most

struct Entry {  // a page left for another: what «Назад» brings back
    refpages::Link at;
    std::string query, name;
    int scroll = 0;
    unsigned opened = 0;  // its long sections opened
};

struct State {
    bool enabled = false, open = false, shown = false, dirty = true, upload = true, wantPage = true;
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
    panel::Picture worldPicture;                    // its picture at its row's size
    std::string worldHover;                         // the map under the pointer there
    // 3.0.11-beta.14: the cards' pictures - the client's sprites and item icons, found once each (none: an empty one)
    std::unique_ptr<gamefiles::Reader> files;
    std::map<std::string, panel::Picture> pictures;
    std::map<int, std::string> itemFiles;  // the client's data\idnum2itemresnametable.txt: item -> its icon's name
    bool itemFilesRead = false;
    unsigned folded = 0;  // 3.0.11-beta.17: the sections the player folded (refpages::Part bits), kept in a file
    bool foldedRead = false;
    // 3.0.11-beta.18: the cards opened lately (the latest first) and the starred ones, kept in files
    std::vector<refpages::Link> recent, favourites;
    std::string here;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, targetW = 0, targetH = 0;
    float k = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(Text t) { return settings::tr(startup().lang, t); }

// 3.0.11-beta.14: a card's picture - a monster's standing frame from its sprite (data\sprite\몬스터 or npc), an
// item's icon (data\texture\유저인터페이스\item) - fitted into the Portrait's square
const std::string kMonsters = "\xB8\xF3\xBD\xBA\xC5\xCD";                         // 몬스터
const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스

bool readFile(State& s, const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // 3.0.11-beta.15: as the game finds it (its zips too)
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!s.files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
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
    panel::Picture& out = s.pictures[key];  // empty until found: not looked for again
    const int box = std::max(16, side - int(8 * s.layout.s));  // the square less its edge (paint: 4 each way)
    if (key.rfind("mob:", 0) == 0) {
        const auto m = s.index->mobs.find(std::atoi(key.c_str() + 4));
        if (m == s.index->mobs.end()) return;
        for (const std::string& name : refindex::spriteNames(m->second))
            for (const std::string& folder : {kMonsters, std::string("npc")}) {
                const std::string base = "data\\sprite\\" + folder + "\\" + name;
                std::string spr, act;
                spritefile::Sprite sp;
                spritefile::Act ac;
                spritefile::Image im;
                if (!readFile(s, base + ".spr", spr) || !readFile(s, base + ".act", act) ||
                    !spritefile::readSpr(spr, sp) || !spritefile::readAct(act, ac) ||
                    !spritefile::frameImage(sp, ac, 0, 0, im))
                    continue;
                out = asPicture(spritefile::fitted(im, box, box));
                LOG_INFO("reference: %s's picture - %s.spr (%dx%d)", m->second.name.c_str(), base.c_str(), im.w, im.h);
                return;
            }
        LOG_INFO("reference: no sprite for %s (#%d) among %zu names", m->second.name.c_str(), m->first,
                 refindex::spriteNames(m->second).size());
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
        s.page = s.at.go != refpages::Go::None ? refpages::open(*s.index, l, s.at, c, s.opened)
                 : !s.query.empty()            ? refpages::found(*s.index, l, s.query, c)  // 3.0.11-beta.15: NPCs
                                               : refpages::home(*s.index, l, s.online ? s.online->online : 0, c);
    }
    s.rows = refwin::place(s.layout, s.page, s.total);
    s.scroll = refwin::clampScroll(s.scroll, s.total, s.layout.bodyH);
    s.madeAt = GetTickCount64();
    for (const refpages::Row& r : s.page.rows)  // 3.0.11-beta.14
        if (r.style == refpages::Style::Portrait && !r.picture.empty())
            loadPicture(s, r.picture, refwin::portraitSide(s.layout));
    for (size_t i = 0; i < s.rows.size() && i < s.page.rows.size(); ++i) {  // 3.0.11-beta.11: the world's picture
        const refwin::Placed& r = s.rows[i];
        if (s.page.rows[i].style != refpages::Style::World || !s.world) continue;
        if (s.worldPicture.width != r.w || s.worldPicture.height != r.h) {
            const worldmap::Sheet& sh = s.world->sheet;
            worldmap::scaled(s.worldPicture, s.world->argb, sh.w, sh.h, r.w, r.h, refwin::worldFit(sh, r));
        }
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
    if (to.go == refpages::Go::Follow) {  // 3.0.11-beta.9: the way on the minimap - the window gives way to it
        std::string target;
        bool walk = true;
        if (minimapproto::following(target, walk) && target == to.map && walk == (to.id == 0)) {
            minimapproto::unfollow();
            makePage(s);  // «вести ›» again
        } else {
            minimapproto::follow(to.map, to.id == 0);
            close(s);
        }
        return;
    }
    s.back.push_back(Entry{s.at, s.query, s.page.name, s.scroll, s.opened});
    s.at = to;
    s.scroll = 0;
    s.opened = 0;
    noteRecent(s);  // 3.0.11-beta.18
    makePage(s);
}

void goBack(State& s) {
    if (s.back.empty()) return;
    const Entry e = s.back.back();
    s.back.pop_back();
    s.at = e.at;
    s.query = e.query;
    s.scroll = e.scroll;
    s.opened = e.opened;
    makePage(s);
}

void typed(State& s) {  // the query changed: its results (a card left for them comes back by «Назад»)
    if (s.at.go != refpages::Go::None) {
        s.back.push_back(Entry{s.at, "", s.page.name, s.scroll, s.opened});
        s.at = refpages::Link{};
        s.opened = 0;
    }
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

void close(State& s) {
    s.open = false;
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, false);
    s.shown = false;
}

// 3.0.11-beta.11: what goes over the world map - you, the way followed, the players now, the map under the pointer
refwin::WorldLook worldLook(const State& s) {
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
    std::string target;
    bool walk = true;
    if (minimapproto::following(target, walk)) {
        const refroute::Route r = refroute::find(*s.index, s.here, cell ? x : -1, cell ? y : -1, target, walk);
        w.route.push_back(s.here);
        for (const refroute::Step& st : r.steps)
            if (w.route.back() != st.move.to) w.route.push_back(st.move.to);
    }
    return w;
}

void render(State& s) {
    const settings::Lang l = startup().lang;
    refwin::Look look{s.hover, !s.back.empty(), worldLook(s), &s.pictures};
    refwin::paint(s.picture, s.layout, s.page, s.rows, s.scroll, s.total, look);
    refwin::Texts t;
    t.title = say(Text::RefTitle);
    if (!s.back.empty()) t.back = settings::fmt(settings::tr(l, Text::RefBack), {s.back.back().name});
    t.query = s.query;
    t.hint = say(Text::RefSearchHint);
    if (s.index)
        t.footer = settings::fmt(settings::tr(l, Text::RefFooter),
                                 {s.index->date.substr(0, 10), s.index->commit.substr(0, 8)});
    std::vector<Words> gdi;
    for (const refwin::Words& w : refwin::words(s.layout, s.page, s.rows, s.scroll, t, look)) {
        const UINT align = w.align == refwin::Align::Right ? DT_RIGHT : w.align == refwin::Align::Center ? DT_CENTER
                                                                                                          : DT_LEFT;
        gdi.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold, align, w.wrap});
    }
    drawWords(s.picture, gdi);
    s.dirty = false;
    s.upload = true;
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
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, &r, false);
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
        if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, false);
        s.shown = false;
        return;
    }
    const std::string here = readGameMap();
    if (here != s.here) {  // 3.0.11-beta.8: «ви тут» and the routes are from the player's map
        s.here = here;
        s.wantPage = true;
    }
    if (float(k) != s.k || targetWidth != s.targetW || targetHeight != s.targetH) {
        s.k = float(k);
        s.targetW = targetWidth;
        s.targetH = targetHeight;
        s.layout = refwin::layout(targetWidth, targetHeight, std::clamp(float(k), 0.75f, 6.0f));
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
    if (readGameCursor(cx, cy))
        hover = refwin::under(s.layout, s.page, s.rows, s.scroll, !s.back.empty(),
                              float(cx) * float(k) - float(s.layout.x), float(cy) * float(k) - float(s.layout.y),
                              s.world ? &s.world->sheet : nullptr);
    const std::string map = hover.part == refwin::Part::Tile && s.world ? s.world->sheet.world[size_t(hover.tile)].map
                                                                        : std::string();
    if (map != s.worldHover) {  // the world map's tile under the pointer: told under the map
        s.worldHover = map;
        makePage(s);
    }
    if (hover.part != s.hover.part || hover.row != s.hover.row || hover.tile != s.hover.tile) {
        s.hover = hover;
        s.dirty = true;
    }
    int budget = 8;  // 3.0.11-beta.15: the icons of the rows in view, a few a frame (a long list never stalls)
    for (size_t i = 0; i < s.rows.size() && i < s.page.rows.size() && budget > 0; ++i) {
        const refpages::Row& r = s.page.rows[i];
        if (r.icon.empty() || !refwin::shown(s.layout, s.rows[i], s.scroll) || s.pictures.count("icon:" + r.icon))
            continue;
        loadPicture(s, "icon:" + r.icon, refwin::iconSide(s.layout));
        --budget;
        s.dirty = true;
    }
    if (s.dirty) render(s);
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.picture.width <= 0 || s.k <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
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
    if (!s.open) return false;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        const int line = std::max(1, s.layout.bodyH / 12);
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
            case VK_RETURN:  // the first row that leads somewhere (a heading folds: not that)
                for (const refpages::Row& r : s.page.rows)
                    if (r.link.go != refpages::Go::None && r.link.go != refpages::Go::Fold &&
                        r.link.go != refpages::Go::Expand) {
                        go(s, r.link);
                        break;
                    }
                s.skipChar = 13;
                return true;
            case VK_PRIOR: scrollBy(s, -s.layout.bodyH * 9 / 10); return true;
            case VK_NEXT: scrollBy(s, s.layout.bodyH * 9 / 10); return true;
            case VK_UP: scrollBy(s, -line); return true;
            case VK_DOWN: scrollBy(s, line); return true;
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
        case WM_MOUSEWHEEL:
            scrollBy(s, -GET_WHEEL_DELTA_WPARAM(wp) * std::max(1, s.layout.bodyH / 8) / WHEEL_DELTA);
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            const refwin::Under u = refwin::under(s.layout, s.page, s.rows, s.scroll, !s.back.empty(), wx, wy,
                                                  s.world ? &s.world->sheet : nullptr);
            if (u.part == refwin::Part::Close) close(s);
            if (u.part == refwin::Part::Back) goBack(s);
            if (u.part == refwin::Part::Row) go(s, s.page.rows[size_t(u.row)].link);
            if (u.part == refwin::Part::Tile && s.world)  // 3.0.11-beta.11: a map on the world map - its card
                go(s, refpages::Link{refpages::Go::Map, 0, s.world->sheet.world[size_t(u.tile)].map});
            break;
        }
        case WM_XBUTTONDOWN:
            if (GET_XBUTTON_WPARAM(wp) == XBUTTON1) goBack(s);
            result = TRUE;
            return true;
        case WM_XBUTTONUP:
            result = TRUE;
            return true;
        case WM_LBUTTONUP:
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

void openMob(int id) {
    State& s = state();
    if (!s.enabled || id <= 0) return;
    openWindow(s, refpages::Link{refpages::Go::Mob, id, ""});
    LOG_INFO("reference: monster %d's card from the minimap's list", id);
}

}  // namespace uagfx::win::referencewin
