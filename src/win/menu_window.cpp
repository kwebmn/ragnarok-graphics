#include "win/menu_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/menu_window.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/code_page.hpp"
#include "win/death_recap_watch.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::menuwin {
namespace {

namespace mw = uagfx::menuwin;

// uaRO.exe's: the windows' manager's three - the menu (UI window 0x30), the death (0x13) and the death with a Token
// of Siegfried (0x88): where each is kept, its class's vtable and its SendMsg (message 6: a button pressed, its id)
struct GameWindow {
    mw::Kind kind;
    uint32_t at, cls, send;
};
constexpr uint32_t kWindowMgr = 0x727ED8;
constexpr GameWindow kWindows[] = {{mw::Kind::Alive, kWindowMgr + 0xD8, 0x699248, 0x47D550},
                                   {mw::Kind::Dead, kWindowMgr + 0xDC, 0x6992E0, 0x47D9F0},
                                   {mw::Kind::DeadToken, kWindowMgr + 0x218, 0x69A8E8, 0x49B990}};
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94;
constexpr int kButtonMsg = 6;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.34: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, hid = false, padNow = false;
    bool recapOn = true;  // 3.0.11-beta.277: [interface] death_recap
    std::string recapTitle;
    std::vector<std::string> recap;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    mw::Kind kind = mw::Kind::Alive;
    uint32_t window = 0;  // the game's, as last found
    mw::Show show;
    mw::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

// The game's menu or death window made now (the death's first: the menu cannot be opened while dead), its kind
uint32_t gameWindow(mw::Kind& kind) {
    for (const GameWindow& g : {kWindows[1], kWindows[2], kWindows[0]}) {
        const uint32_t w = read32(g.at);
        if (w && read32(w) == g.cls && read32(read32(w) + kSendMsgSlot) == g.send) {
            kind = g.kind;
            return w;
        }
    }
    return 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("menu: the game's window hidden for the new menu (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("menu: the game's window back");
    }
}

mw::Texts texts(bool pad) {
    mw::Texts t;
    t.menu = say(settings::Text::MenuTitle);
    t.dead = say(settings::Text::MenuDead);
    t.revive = say(settings::Text::MenuRevive);
    t.savePoint = say(settings::Text::MenuSavePoint);
    t.charSelect = say(settings::Text::MenuCharSelect);
    t.exit = say(settings::Text::MenuExit);
    t.back = say(settings::Text::MenuBack);
    t.enter = pad ? uapad::buttonLabel(0) : "Enter";
    t.esc = pad ? uapad::buttonLabel(1) : "Esc";
    t.recapTitle = state().recapTitle;  // 3.0.11-beta.277
    t.recap = state().recap;
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

std::vector<Words> gdiWords(const std::vector<mw::Words>& in) {
    std::vector<Words> out;
    for (const mw::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == itemwin::Align::Right    ? DT_RIGHT
                                 : w.align == itemwin::Align::Center ? DT_CENTER
                                                                     : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    if (uaui::ready()) {  // 3.0.18-beta.34: uaUI's window says it all again, no picture
        ++s.sheetGen;
        return;
    }
    const uicost::Paint cost("the menu", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    const mw::Texts t = texts(s.padNow);
    mw::paint(s.picture, s.layout, s.kind, s.show, t);
    drawWords(s.picture, gdiWords(mw::words(s.layout, s.kind, s.show, t, measure)));
    s.upload = true;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Menu, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Menu, nullptr, nullptr, false);
    s.shown = false;
}

const char* nameOf(mw::Action a) {
    switch (a) {
        case mw::Action::Revive: return "revive here (Token of Siegfried)";
        case mw::Action::SavePoint: return "the save point";
        case mw::Action::CharSelect: return "character select";
        case mw::Action::Exit: return "exit";
        case mw::Action::Back: break;
    }
    return "back to the game";
}

// Its row pressed: the game's window's own button (message 6, its id) - the game does the rest and deletes its window
void press(State& s, int row) {
    mw::Kind kind = mw::Kind::Alive;
    const uint32_t window = gameWindow(kind);
    const std::vector<mw::Action> as = mw::actions(kind);
    if (!window || row < 0 || row >= int(as.size())) return;
    const int id = mw::buttonId(kind, as[size_t(row)]);
    if (!id) return;
    const uint32_t send = read32(read32(window) + kSendMsgSlot);
    if (s.told++ < 30) LOG_INFO("menu: «%s» pressed - the game's button 0x%X", nameOf(as[size_t(row)]), unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

}  // namespace

void setRecap(bool on) {  // 3.0.11-beta.277
    State& s = state();
    if (on != s.recapOn) LOG_INFO("menu: [interface] death_recap = %d", on ? 1 : 0);
    s.recapOn = on;
}

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("menu: [interface] menu = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    mw::Kind kind = mw::Kind::Alive;
    const uint32_t window = world ? gameWindow(kind) : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window || kind != s.kind) {  // opened: the safe row in focus
        s.show.focus = mw::firstFocus(kind);
        s.recap.clear();  // 3.0.11-beta.277: the death's last hits, taken once as it shows
        if (kind != mw::Kind::Alive && s.recapOn) s.recap = deathrecapwin::lines(s.recapTitle);
        if (s.told++ < 30)
            LOG_INFO("menu: %s - the new menu shows it", kind == mw::Kind::Alive ? "the game's menu (Esc)"
                                                         : kind == mw::Kind::Dead ? "the death"
                                                                                  : "the death (a Token of Siegfried)");
    }
    s.window = window;
    s.kind = kind;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.layout = mw::layout(kind, targetWidth, targetHeight, s.scale, s.recap.size());
    s.show.hover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const int row = mw::buttonAt(s.layout, float(cx) * float(k) - float(s.layout.x),
                                     float(cy) * float(k) - float(s.layout.y));
        if (row >= 0) s.show.hover = row;
    }
    s.padNow = uapad::runtime::look().shown;
    const std::string key = std::to_string(int(kind)) + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ';' + std::to_string(s.scale) + ';' +
                            std::to_string(s.show.hover) + ',' + std::to_string(s.show.focus) +
                            (s.padNow ? "P" : "") + '#' + std::to_string(uistyle::generation());
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.34 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): this window in RmlUi's
// markup, line by line as core menuwin paints it (its boxes, icons, circles); the words where it places them
static void fillSheet(State& s, uisheet::Content& ui) {
    using namespace uagfx::menuwin;
    const auto& L = s.layout;
    const auto& k = s.kind;
    const auto& sh = s.show;
    [[maybe_unused]] constexpr uint32_t kCard = 0x10141C;
    [[maybe_unused]] constexpr uint32_t kWhite = 0xFFFFFF;
    [[maybe_unused]] constexpr uint32_t kGold = 0xC9A55B;
    [[maybe_unused]] constexpr uint32_t kGold2 = 0xE6CB8A;
    [[maybe_unused]] constexpr uint32_t kText = 0xEEF2F8;
    [[maybe_unused]] constexpr uint32_t kMuted = 0x9AA3B5;
    [[maybe_unused]] constexpr uint32_t kDeadRed = 0xFF5A4E;
    [[maybe_unused]] constexpr float kWide = 320;
    [[maybe_unused]] constexpr float kPad = 16;
    [[maybe_unused]] constexpr float kTitleH = 28;
    [[maybe_unused]] constexpr float kTitleGap = 10;
    [[maybe_unused]] constexpr float kButtonH = 38;
    [[maybe_unused]] constexpr float kGap = 8;
    ui.w = float(L.w), ui.h = float(L.h);
    const auto put = [&](const panel::Picture& p, const panel::Box& box, float k = 1.0f) {
        if (p.width > 0)
            ui.icons.push_back(uisheet::Icon{std::round(box.x + (box.w - float(p.width)) / 2),
                                            std::round(box.y + (box.h - float(p.height)) / 2), float(p.width),
                                            float(p.height), k, s.sheet.icon(&p)});
    };
    [[maybe_unused]] const auto circle = [&](float cx, float cy, float r, uint32_t rgb, float a) {
        ui.rect(ui.under, panel::Box{cx - r, cy - r, 2 * r, 2 * r}, r, rgb, a);
    };
    [[maybe_unused]] const auto icon = [&](uiicons::Icon which, float x, float y, float side, uint32_t rgb, float a,
                                           float stroke) {
        const int px = std::max(4, int(std::ceil(side)));
        const std::string key = "u" + std::to_string(int(which)) + ':' + std::to_string(px) + ':' +
                                std::to_string(rgb) + ':' + std::to_string(int(a * 100));
        const std::string src = s.sheet.made(key, px, px, [&](panel::Picture& p) {
            uiicons::draw(p, which, 0, 0, float(px), rgb, a, stroke);
        });
        ui.icons.push_back(uisheet::Icon{x, y, float(px), float(px), 1, src});
    };
    (void)put;
    const mw::Texts t = texts(s.padNow);
    (void)t;
    const float sc = L.s;
    ui.rect(ui.under, L.glass, 12 * sc, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * sc);
    if (L.recap.h > 0) ui.rect(ui.under, L.recap, 8 * sc, 0x000000, 0.25f);  // .277
    for (size_t i = 0; i < L.buttons.size(); ++i) {
        const panel::Box& b = L.buttons[i];
        const bool lit = int(i) == sh.hover || int(i) == sh.focus;
        if (k != Kind::Alive && i == 0)  // core menuwin's primary(): the death's first
            ui.rect(ui.under, b, 8 * sc, kGold, lit ? 0.26f : 0.16f, kGold2, lit ? 0.9f : 0.55f,
                    (lit ? 2.0f : 1.0f) * sc);
        else
            ui.rect(ui.under, b, 8 * sc, kWhite, lit ? 0.12f : 0.05f, lit ? kGold2 : kWhite, lit ? 0.8f : 0.10f,
                    (lit ? 2.0f : 1.0f) * sc);
    }
    ui.say(mw::words(s.layout, s.kind, s.show, t, measure));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetShown = s.sheet.frame("menu", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    if (!s.sheetShown) {  // 3.0.18-beta.34: uaUI draws the window (with the windows' layer)
        s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                      12 * s.scale, s.scale, s.k);
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    }
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;  // the world's
    result = 0;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const int row = mw::buttonAt(s.layout, lx, ly);
        if (row >= 0) {
            s.show.focus = row;
            press(s, row);
        }
    }
    return true;  // the card's: not the world's under it
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (!s.shown || (m != WM_KEYDOWN && m != WM_KEYUP && m != WM_CHAR)) return false;
    if (wp != VK_UP && wp != VK_DOWN && wp != VK_RETURN) return false;  // Esc and the rest: the game's
    if (m == WM_KEYDOWN)
        padCommand(wp == VK_UP ? PadCommand::Up : wp == VK_DOWN ? PadCommand::Down : PadCommand::Press);
    return true;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    mw::Kind kind = mw::Kind::Alive;
    if (const uint32_t window = gameWindow(kind)) hold(s, window, true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int rows = int(mw::actions(s.kind).size());
    switch (c) {
        case PadCommand::Up: s.show.focus = std::max(0, s.show.focus - 1); break;
        case PadCommand::Down: s.show.focus = std::min(rows - 1, s.show.focus + 1); break;
        case PadCommand::Press: press(s, s.show.focus); break;
        case PadCommand::Back: press(s, rows - 1); break;
    }
    s.key.clear();
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::menuwin
