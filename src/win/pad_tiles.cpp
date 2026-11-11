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
#include "win/merc_card.hpp"
#include "win/minimap_proto.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_window.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

#include "win/options_card.hpp"
#include "win/own_windows.hpp"

namespace uagfx::win::padtileswin {
namespace {

namespace pt = uagfx::padtiles;

// the game's Basic Info window's commands (core/char_block kButton): items, stats, skills, equip, friends, options
constexpr int kItems = 0x80, kStats = 0x7E, kSkills = 0x82, kEquip = 0x81, kFriends = 0x85, kOptions = 0x7F;

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.35: drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false, sheetSony = false;
    std::vector<Words> sheetWords;
    bool open = false, shown = false;
    int focus = 0;
    int count = pt::kTiles;  // 3.0.11-beta.216: ten with a mercenary hired («Найманець»)
    std::vector<pt::Tile> list;  // 3.0.18-beta.124: the tiles in their places (ours switched on, the mercenary)
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
    const bool byUaui = uaui::ready();  // 3.0.18-beta.35: the words kept for uaUI's sheet, no picture
    if (!byUaui && !panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    if (!byUaui) pt::paint(s.picture, s.layout, s.focus, sony, &s.list);
    using settings::Text;
    const settings::Lang l = language();
    const Text byTile[int(pt::Tile::Count)] = {Text::ItemsTitle, Text::StatsTitle, Text::TileSkills, Text::TileEquip,
                                               Text::PartyTitle, Text::TileMap, Text::RefTitle, Text::TileOptions,
                                               Text::PagePad, Text::MercTile, Text::HudPartHunt, Text::InterfaceMvp,
                                               Text::TrackerTitle, Text::InterfaceDeathRecap};
    std::vector<std::string> names;
    for (const pt::Tile t : s.list) names.push_back(settings::tr(l, byTile[int(t)]));
    std::vector<Words> words = gdiWords(pt::words(s.layout, names, settings::tr(l, Text::TilesTitle),
                                            settings::tr(l, Text::TilesOpen), settings::tr(l, Text::TilesClose),
                                            sony));
    if (byUaui) {
        s.sheetWords = std::move(words);
        s.sheetSony = sony;
        ++s.sheetGen;
        return;
    }
    drawWords(s.picture, words);
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
    if (s.focus < 0 || size_t(s.focus) >= s.list.size()) return;
    const pt::Tile tile = s.list[size_t(s.focus)];
    close(s, "a tile chosen");
    bool done = true;
    const char* what = "";
    switch (tile) {
        case pt::Tile::Items: what = "the game's items"; done = charpanel::pressGame(kItems); break;
        case pt::Tile::Stats: what = "the game's status"; done = charpanel::pressGame(kStats); break;
        case pt::Tile::Skills: what = "the game's skills"; done = charpanel::pressGame(kSkills); break;
        case pt::Tile::Equip: what = "the game's equipment"; done = charpanel::pressGame(kEquip); break;
        case pt::Tile::Party: what = "the game's party and friends"; done = charpanel::pressGame(kFriends); break;
        case pt::Tile::Options:  // 3.0.18-beta.124: «Налаштування» (the game's options in the new look)
            what = "«Налаштування»";
            done = optionscardwin::toggleOpen("the pad's tile") || charpanel::pressGame(kOptions);
            break;
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
        case pt::Tile::Merc: what = "the mercenary's card"; done = merccardwin::toggleCard(); break;  // .216
        case pt::Tile::Hunt: what = "«Полювання»"; ownwin::press(closedwin::Own::Hunt, "the pad's tile"); break;
        case pt::Tile::Mvp: what = "«Таймери босів»"; ownwin::press(closedwin::Own::Mvp, "the pad's tile"); break;
        case pt::Tile::Tracker: what = "«Відстеження»"; ownwin::press(closedwin::Own::Tracker, "the pad's tile"); break;
        case pt::Tile::Recap: what = "«Що мене вбило»"; ownwin::press(closedwin::Own::Recap, "the pad's tile"); break;
        default: break;
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
        case Command::Up: s.focus = pt::moved(s.focus, 0, -1, s.count); break;
        case Command::Down: s.focus = pt::moved(s.focus, 0, 1, s.count); break;
        case Command::Left: s.focus = pt::moved(s.focus, -1, 0, s.count); break;
        case Command::Right: s.focus = pt::moved(s.focus, 1, 0, s.count); break;
        case Command::Enter: run(s); break;
        case Command::Close: close(s, "B or Start"); break;
    }
}

static void frameInner(HWND game, double k, int targetWidth, int targetHeight) {
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
    s.list.clear();  // 3.0.18-beta.124: the nine, ours switched on, the mercenary hired
    for (int t = 0; t < pt::kTiles; ++t) s.list.push_back(pt::Tile(t));
    for (const closedwin::Own o : ownwin::row()) {
        if (o == closedwin::Own::Hunt) s.list.push_back(pt::Tile::Hunt);
        if (o == closedwin::Own::Mvp) s.list.push_back(pt::Tile::Mvp);
        if (o == closedwin::Own::Tracker) s.list.push_back(pt::Tile::Tracker);
        if (o == closedwin::Own::Recap) s.list.push_back(pt::Tile::Recap);
    }
    if (merccardwin::hired()) s.list.push_back(pt::Tile::Merc);
    s.count = int(s.list.size());
    if (s.focus >= s.count) s.focus = 0;
    s.layout = pt::layout(targetWidth / 2, targetHeight / 2, scale, s.count);
    const bool sony = uapad::runtime::look().sony;
    const std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' +
                            std::to_string(s.focus) + '/' + std::to_string(s.count) + (sony ? "S" : "X") + '#' +
                            std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
    if (key != s.key) {
        s.key = key;
        render(s, sony);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.35 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the controller's tiles
// in RmlUi's markup, line by line as core padtiles paints them - its glass, the hints' line, the tiles (the focused
// gold) with their icons, A and B; the words as the part lays them out
static void fillSheet(State& s, uisheet::Content& c) {
    const pt::Layout& L = s.layout;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A;
    constexpr float kGlyph = 20;  // core padtiles'
    c.w = float(L.w), c.h = float(L.h);
    c.rect(c.under, L.glass, 14 * sc, kCard, std::max(0.9f, uistyle::panel()), kWhite, 0.10f, 1.0f * sc);
    c.rect(c.under, panel::Box{L.hints.x, L.hints.y - 6 * sc, L.hints.w, 1 * sc}, 0, kWhite, 0.09f);
    for (size_t i = 0; i < L.tiles.size(); ++i) {
        const panel::Box& b = L.tiles[i];
        const bool on = int(i) == s.focus;
        c.rect(c.under, b, 12 * sc, on ? kGold : kWhite, on ? 0.13f : 0.045f, on ? kGold2 : kWhite, on ? 1.0f : 0.10f,
               (on ? 2.0f : 1.0f) * sc);
        const int px = std::max(4, int(std::ceil(36 * sc)));
        const uint32_t rgb = on ? kGold2 : 0xC7CCD6;
        const uiicons::Icon icon = pt::iconOf(i < s.list.size() ? int(s.list[i]) : int(i));
        const std::string src = s.sheet.made("t" + std::to_string(int(icon)) + ':' + std::to_string(px) + ':' +
                                                 std::to_string(rgb),
                                             px, px, [&](panel::Picture& p) {
                                                 uiicons::draw(p, icon, 0, 0, float(px), rgb, 1.0f, 1.8f);
                                             });
        c.icons.push_back(uisheet::Icon{std::round(b.x + (b.w - float(px)) / 2), std::round(b.y + 16 * sc),
                                        float(px), float(px), 1, src});
    }
    const float y = L.hints.y + (L.hints.h - kGlyph * sc) / 2;
    s.sheet.glyph(c, int(padglyph::Button::A), s.sheetSony, L.hints.x, y, kGlyph * sc);
    s.sheet.glyph(c, int(padglyph::Button::B), s.sheetSony, L.hints.x + 130 * sc, y, kGlyph * sc);
    uisheet::sayGdi(c, s.sheetWords);
}

void frame(HWND game, double k, int targetWidth, int targetHeight) {
    frameInner(game, k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.top = true;  // 3.0.18-beta.69: over the game's own windows, as its picture was
    s.sheetShown = s.sheet.frame("pad tiles", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.sheetShown || s.k <= 0 || s.picture.width <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown || (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK && m != WM_LBUTTONUP && m != WM_MOUSEMOVE &&
                      m != WM_RBUTTONDOWN))
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
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && tile >= 0) {
        s.focus = tile;
        run(s);
    } else if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN) {
        if (!s.layout.glass.contains(x, y)) close(s, "a click outside");
    }
    (void)wp;
    return true;  // modal: no click goes through while it is open
}

}  // namespace uagfx::win::padtileswin
