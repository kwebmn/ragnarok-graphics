#include "win/pad_tiles.hpp"

#include <windowsx.h>

#include <cmath>
#include <string>
#include <vector>

#include "core/pad_tiles.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/char_block.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_window.hpp"

namespace uagfx::win::padtileswin {
namespace {

namespace pt = uagfx::padtiles;

// the game's Basic Info window's commands (core/char_block kButton): items, stats, skills, equip, friends, options
constexpr int kItems = 0x80, kStats = 0x7E, kSkills = 0x82, kEquip = 0x81, kFriends = 0x85, kOptions = 0x7F;

struct State {
    bool open = false, shown = false;
    int focus = 0;
    pt::Layout layout;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    std::string key;
    float k = 0;
    int targetW = 0, targetH = 0;
};

State& state() {
    static State s;
    return s;
}

std::vector<Words> gdiWords(const std::vector<pt::Words>& in) {
    std::vector<Words> out;
    for (const pt::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == pt::Align::Right    ? DT_RIGHT
                                 : w.align == pt::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s, bool sony) {
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    pt::paint(s.picture, s.layout, s.focus, sony);
    using settings::Text;
    const settings::Lang l = startup().lang;
    const std::vector<std::string> names = {settings::tr(l, Text::ItemsTitle),   settings::tr(l, Text::StatsTitle),
                                            settings::tr(l, Text::TileSkills),   settings::tr(l, Text::TileEquip),
                                            settings::tr(l, Text::PartyTitle),   settings::tr(l, Text::TileMap),
                                            settings::tr(l, Text::RefTitle),     settings::tr(l, Text::TileOptions),
                                            settings::tr(l, Text::PagePad)};
    drawWords(s.picture, gdiWords(pt::words(s.layout, names, settings::tr(l, Text::TilesTitle),
                                            settings::tr(l, Text::TilesOpen), settings::tr(l, Text::TilesClose),
                                            sony)));
    s.upload = true;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const RECT r{LONG(int64_t(s.layout.x) * client.right / s.targetW),
                 LONG(int64_t(s.layout.y) * client.bottom / s.targetH),
                 LONG(int64_t(s.layout.x + s.layout.w) * client.right / s.targetW + 1),
                 LONG(int64_t(s.layout.y + s.layout.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::PadTiles, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::PadTiles, nullptr, nullptr, false);
    s.shown = false;
}

void close(State& s, const char* why) {
    if (!s.open) return;
    s.open = false;
    LOG_INFO("pad tile: closed (%s)", why);
}

// The framed tile's window: the game's own through its Basic Info window, the new big map, «Довідник», the pad's menu
void run(State& s) {
    const pt::Tile tile = pt::Tile(s.focus);
    close(s, "a tile chosen");
    bool done = true;
    const char* what = "";
    switch (tile) {
        case pt::Tile::Items: what = "the game's items"; done = charpanel::pressGame(kItems); break;
        case pt::Tile::Stats: what = "the game's status"; done = charpanel::pressGame(kStats); break;
        case pt::Tile::Skills: what = "the game's skills"; done = charpanel::pressGame(kSkills); break;
        case pt::Tile::Equip: what = "the game's equipment"; done = charpanel::pressGame(kEquip); break;
        case pt::Tile::Party: what = "the game's party and friends"; done = charpanel::pressGame(kFriends); break;
        case pt::Tile::Options: what = "the game's options"; done = charpanel::pressGame(kOptions); break;
        case pt::Tile::Map:
            what = "the big map";
            done = minimapproto::showBig(true);
            if (!done) {  // the new minimap off: the map's card in «Довідник»
                const std::string map = readGameMap();
                if (!map.empty()) referencewin::openMap(map);
                done = !map.empty();
            }
            break;
        case pt::Tile::Reference: what = "«Довідник»"; referencewin::toggleFromPad(); break;
        case pt::Tile::Pad: what = "the controller's own menu"; uapad::runtime::openPadMenu(); break;
    }
    LOG_INFO("pad tile: %s%s", what, done ? "" : " - not possible now");
}

}  // namespace

bool isOpen() { return state().open; }

void open() {
    State& s = state();
    if (s.open) return;
    s.open = true;
    LOG_INFO("pad tile: opened (Start)");
}

void command(Command c) {
    State& s = state();
    if (!s.open) return;
    switch (c) {
        case Command::Up: s.focus = pt::moved(s.focus, 0, -1); break;
        case Command::Down: s.focus = pt::moved(s.focus, 0, 1); break;
        case Command::Left: s.focus = pt::moved(s.focus, -1, 0); break;
        case Command::Right: s.focus = pt::moved(s.focus, 1, 0); break;
        case Command::Enter: run(s); break;
        case Command::Close: close(s, "B or Start"); break;
    }
}

void frame(HWND game, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || readGameMap().empty()) {
        if (s.open && readGameMap().empty()) close(s, "no world");
        hide(s);
        return;
    }
    const float scale = uistyle::partScale(k);
    s.k = float(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.layout = pt::layout(targetWidth / 2, targetHeight / 2, scale);
    const bool sony = uapad::runtime::look().sony;
    const std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' +
                            std::to_string(s.focus) + (sony ? "S" : "X") + '#' + std::to_string(uistyle::generation());
    if (key != s.key) {
        s.key = key;
        render(s, sony);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown || (m != WM_LBUTTONDOWN && m != WM_LBUTTONUP && m != WM_MOUSEMOVE && m != WM_RBUTTONDOWN))
        return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, double(s.k), s.targetW, s.targetH, tx, ty)) return false;
    const float x = tx - float(s.layout.x), y = ty - float(s.layout.y);
    const int tile = pt::hitTest(s.layout, x, y);
    if (m == WM_MOUSEMOVE) {
        if (tile >= 0) s.focus = tile;
        return false;
    }
    result = 0;
    if (m == WM_LBUTTONDOWN && tile >= 0) {
        s.focus = tile;
        run(s);
    } else if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) {
        if (!s.layout.glass.contains(x, y)) close(s, "a click outside");
    }
    (void)wp;
    return true;  // modal: no click goes through while it is open
}

}  // namespace uagfx::win::padtileswin
