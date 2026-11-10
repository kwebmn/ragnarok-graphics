#include "win/world_signs.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/ui_style.hpp"
#include "core/world_signs.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::worldsigns {
namespace ws = uagfx::worldsigns;
namespace {

constexpr size_t kTitleLetters = 30;  // a longer title cut with an ellipsis
constexpr int kKeepFrames = 240;      // a plate not shown this long: its texture freed
constexpr size_t kMostPlates = 96;

struct Room {
    std::string title;
    int users = 0, limit = 0, type = 1;
};

struct Seen {  // a sign window noted in the game's windows' draw
    int x = 0, y = 0, w = 0, h = 0;
    bool shop = false;
    uint32_t key = 0;  // the merchant's account, the room's id
    int type = 1;      // a room's (its window's + 0x6C)
};

struct Plate {
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int idle = 0;
};

struct Shown {
    std::string key;
    int x = 0, y = 0;
    // 3.0.18-beta.38: uaUI's plate - its kind, words and size
    ws::Kind kind = ws::Kind::Shop;
    std::string title, places;
    int w = 0, h = 0;
};

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.38: the plates drawn by uaUI (the world's layer), said again each frame
    int sheetGen = 0;
    bool sheetShown = false;
    float scale = 1;
    int targetW = 0, targetH = 0;
    bool on = false;
    bool told = false;
    unsigned codePage = 0;
    float k = 0;
    std::unordered_map<uint32_t, std::string> shops;
    std::unordered_map<uint32_t, Room> rooms;
    std::vector<Seen> seen;  // this frame's
    std::unordered_map<std::string, Plate> plates;
    std::vector<Shown> shown;
    int shopsTold = 0, roomsTold = 0, missTold = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

// A plate's measures (3.0.18-beta.38: uaUI's too): its size, the title's and the places' widths
struct Measures {
    float px = 0, pad = 0, dot = 0, gap = 0;
    int titleW = 0, placesW = 0, w = 0, h = 0;
};
Measures measures(const std::wstring& title, const std::wstring& places, float scale) {
    Measures m;
    m.px = 12.5f * scale, m.pad = 9 * scale, m.dot = 7 * scale, m.gap = 6 * scale;
    m.titleW = wordsWidth(title, m.px, true), m.placesW = places.empty() ? 0 : wordsWidth(places, m.px, true);
    m.h = int(std::lround(22 * scale));
    m.w = int(std::lround(m.pad + m.dot + m.gap + float(m.titleW) + (m.placesW ? m.gap + float(m.placesW) : 0) +
                          m.pad));
    return m;
}

// A plate painted: a dark rounded plate with a thin edge and a dot in its kind's colour, the title, a room's places
void paint(Plate& p, ws::Kind kind, const std::wstring& title, const std::wstring& places, float scale) {
    const Measures m = measures(title, places, scale);
    const float px = m.px, pad = m.pad, dot = m.dot, gap = m.gap;
    const int titleW = m.titleW, placesW = m.placesW, h = m.h, w = m.w;
    if (!panel::blank(p.picture, w, h)) return;
    const uint32_t accent = ws::accent(kind);
    panel::roundRect(p.picture, panel::Box{0, 0, float(w), float(h)}, 6 * scale, 0x15181F, 0.86f, accent, 0.55f,
                     std::max(1.0f, scale));
    panel::roundRect(p.picture, panel::Box{pad, (float(h) - dot) / 2, dot, dot}, dot / 2, accent, 1.0f);
    std::vector<Words> words;
    const float tx = pad + dot + gap;
    words.push_back(Words{panel::Box{tx, 0, float(titleW) + 2, float(h)}, title, 0xECEFF5, px, true, DT_LEFT});
    if (placesW)
        words.push_back(Words{panel::Box{tx + float(titleW) + gap, 0, float(placesW) + 2, float(h)}, places, accent,
                              px, true, DT_LEFT});
    drawWords(p.picture, words);
    p.upload = true;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* shop = std::get_if<packets::ShopSign>(&e)) {
        if (shop->title.empty()) s.shops.erase(shop->account);
        else if (s.shops.size() < 1000) s.shops[shop->account] = shop->title;
    } else if (const auto* room = std::get_if<packets::RoomSign>(&e)) {
        if (room->gone) s.rooms.erase(room->room);
        else if (s.rooms.size() < 1000) s.rooms[room->room] = Room{room->title, room->users, room->limit, room->type};
    }
}

bool take(uint32_t window) {
    State& s = state();
    if (!s.on || !window) return false;
    const uint32_t cls = read32(window);
    if (cls != ws::kShopClass && cls != ws::kRoomClass) return false;
    int32_t rect[4] = {};
    if (!readGame(window + ws::kRect, rect, sizeof rect) || rect[2] <= 0 || rect[3] <= 0) return false;
    Seen n;
    n.x = rect[0], n.y = rect[1], n.w = rect[2], n.h = rect[3];
    n.shop = cls == ws::kShopClass;
    n.key = read32(window + (n.shop ? ws::kShopAccount : ws::kRoomId));
    if (!n.shop) n.type = int(read32(window + ws::kRoomType));
    if (s.seen.size() < kMostPlates) s.seen.push_back(n);
    return true;
}

static void frameInner(bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on || !s.told) {
        s.on = on;
        s.told = true;
        LOG_INFO("signs: [interface] signs = %d - %s", on ? 1 : 0,
                 on ? "the shops' and the chat rooms' signs as the new look's plates" : "the game's own");
    }
    s.codePage = codePage;
    s.shown.clear();
    std::vector<Seen> seen;
    seen.swap(s.seen);
    if (!on || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) return;
    s.k = float(k);
    const float scale = uistyle::partScale(k);
    s.scale = scale, s.targetW = targetWidth, s.targetH = targetHeight;
    for (const Seen& n : seen) {
        std::string title, places;
        ws::Kind kind = ws::Kind::Shop;
        if (n.shop) {
            const auto it = s.shops.find(n.key);
            if (it != s.shops.end()) title = it->second;
        } else {
            const auto it = s.rooms.find(n.key);
            kind = ws::roomKind(it != s.rooms.end() ? it->second.type : n.type);
            if (it != s.rooms.end()) {
                title = it->second.title;
                places = ws::places(it->second.users, it->second.limit);
            }
        }
        if (title.empty()) {
            if (s.missTold < 5) {
                ++s.missTold;
                LOG_INFO("signs: a %s's sign (%u) with no title from the server yet - a plate without it",
                         n.shop ? "shop" : "room", n.key);
            }
        }
        const std::string text = ws::shortTitle(utf8Of(widened(title, s.codePage)), kTitleLetters);
        const std::string key = std::to_string(int(kind)) + '|' + text + '|' + places + '|' + std::to_string(scale);
        const float cx = (float(n.x) + float(n.w) / 2) * s.k, cy = (float(n.y) + float(n.h) / 2) * s.k;
        if (uaui::ready()) {  // 3.0.18-beta.38: uaUI's plate - no picture
            const Measures m = measures(utf8Wide(text), utf8Wide(places), scale);
            s.shown.push_back(Shown{key, int(std::lround(cx - float(m.w) / 2)), int(std::lround(cy - float(m.h) / 2)),
                                    kind, text, places, m.w, m.h});
            continue;
        }
        Plate& p = s.plates[key];
        p.idle = 0;
        if (p.picture.width == 0) paint(p, kind, utf8Wide(text), utf8Wide(places), scale);
        if (p.picture.width == 0) continue;
        s.shown.push_back(Shown{key, int(std::lround(cx - float(p.picture.width) / 2)),
                                int(std::lround(cy - float(p.picture.height) / 2)), kind, {}, {}, 0, 0});
        int& told = n.shop ? s.shopsTold : s.roomsTold;
        if (told < 3) {
            ++told;
            LOG_INFO("signs: %s «%s»%s%s at the game's %d, %d (%dx%d) - a %dx%d plate", n.shop ? "a shop" : "a room",
                     text.c_str(), places.empty() ? "" : " ", places.c_str(), n.x, n.y, n.w, n.h,
                     p.picture.width, p.picture.height);
        }
    }
}

// 3.0.18-beta.38 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the shops' and the chat
// rooms' signs in RmlUi's markup, as paint() makes them - a dark plate edged and dotted in its kind's colour, the
// title, a room's places; over the world and under the game's own windows (uaUI's world layer)
static void fillSheet(State& s, uisheet::Content& c) {
    c.w = float(s.targetW), c.h = float(s.targetH);
    const float scale = s.scale;
    char colour[16];
    for (const Shown& sh : s.shown) {
        if (sh.w <= 0) continue;
        const Measures m = measures(utf8Wide(sh.title), utf8Wide(sh.places), scale);
        const uint32_t accent = ws::accent(sh.kind);
        const float x = float(sh.x), y = float(sh.y);
        c.rect(c.under, panel::Box{x, y, float(sh.w), float(sh.h)}, 6 * scale, 0x15181F, 0.86f, accent, 0.55f,
               std::max(1.0f, scale));
        c.rect(c.under, panel::Box{x + m.pad, y + (float(sh.h) - m.dot) / 2, m.dot, m.dot}, m.dot / 2, accent, 1.0f);
        const float tx = x + m.pad + m.dot + m.gap;
        c.words.push_back(uisheet::Word{tx, y, float(m.titleW) + 2, float(sh.h), m.px, float(sh.h), sh.title,
                                        "#eceff5", "word bold"});
        if (m.placesW) {
            std::snprintf(colour, sizeof colour, "#%06x", unsigned(accent & 0xFFFFFF));
            c.words.push_back(uisheet::Word{tx + float(m.titleW) + m.gap, y, float(m.placesW) + 2, float(sh.h), m.px,
                                            float(sh.h), sh.places, colour, "word bold"});
        }
    }
}

void frame(bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.world = true;
    s.sheetShown = s.sheet.frame("signs", uaui::ready() && !s.shown.empty() && s.k > 0, false, 0, 0, ++s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    for (auto it = s.plates.begin(); it != s.plates.end();) {  // the ones not shown of late freed
        if (++it->second.idle > kKeepFrames || (!s.on && s.shown.empty())) {
            if (it->second.texture) renderer.releaseTexture(it->second.texture);
            it = s.plates.erase(it);
        } else {
            ++it;
        }
    }
    if (s.shown.empty() || s.k <= 0 || s.sheetShown) return;
    // where the world ends - under the game's own windows, as its signs were
    const size_t world = record.firstInterface(), before = record.commands().size();
    for (const Shown& sh : s.shown) {
        const auto it = s.plates.find(sh.key);
        if (it == s.plates.end()) continue;
        Plate& p = it->second;
        p.idle = 0;
        drawPart(record, renderer, p.picture, p.texture, p.texW, p.texH, p.upload, sh.x, sh.y, s.k);
    }
    const size_t added = record.commands().size() - before;
    if (added) record.moveLastBefore(added, world);
}

}  // namespace uagfx::win::worldsigns
