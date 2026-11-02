#include "win/my_shop.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/my_shop.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::myshopwin {
namespace {

namespace ms = uagfx::myshop;
namespace iw = uagfx::itemwin;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the own-shop window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the session's list of the things for sale (a node's ITEM_INFO at +8: the item's number as words +0x30 /
// +0x34, how many +0x10, the price +0x18, known +0x3C, refined +0x40); the shop's sign the game keeps (a char*)
constexpr uint32_t kWindowMgr = 0x727ED8, kShopAt = kWindowMgr + 0x150, kShopClass = 0x69B478, kShopSend = 0x4B0A60;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kSession = 0x771148, kForSale = kSession + 0xF7C, kInfo = 8;
constexpr uint32_t kSign = 0x772D98;
constexpr int kButtonMsg = 6;
constexpr size_t kInfoSize = 0x48, kMostNodes = 100;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<ms::Row> rows;
    std::string sign;
    ms::Show show;
    ms::Layout layout;
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

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

uint32_t gameShop() {
    const uint32_t w = read32(kShopAt);
    return w && read32(w) == kShopClass && read32(read32(w) + kSendMsgSlot) == kShopSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("my shop: the game's own-shop window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

void readShop(State& s) {
    s.rows.clear();
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    const uint32_t head = read32(kForSale);
    if (head) {
        uint32_t node = read32(head);
        for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
            uint8_t raw[kInfoSize] = {};
            if (!readGame(node + kInfo, raw, sizeof raw)) break;
            const uint32_t ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
            char digits[17] = {};
            if (!ptr || !len || len > 16 || !readGame(ptr, digits, len)) continue;
            ms::Row r;
            const std::string id = std::to_string(std::atoi(digits));
            std::string words;
            if (!raw[0x3C]) {
                const auto u = unknown.find(id);
                if (u != unknown.end()) words = u->second;
            }
            if (words.empty()) {
                const auto w = names.find(id);
                if (w != names.end()) words = w->second;
            }
            r.name = words.empty() ? "#" + id : utf8Of(widened(hotbar::spaced(words), s.codePage));
            r.left = int(rd32(raw, 0x10));
            r.price = int32_t(rd32(raw, 0x18));
            const int refine = int(rd32(raw, 0x40));
            r.refine = refine > 0 && refine <= 20 ? refine : 0;
            const auto f = files.find(id);
            if (f != files.end()) r.icon = clienttables::icon(f->second, iw::iconSide(s.scale) * 7 / 8, 4 * s.scale);
            s.rows.push_back(std::move(r));
        }
    }
    const uint32_t sign = read32(kSign);
    char buf[81] = {};
    s.sign = sign && readGame(sign, buf, 80) ? utf8Of(widened(buf, s.codePage)) : "";
}

ms::Texts texts(const State& s) {
    ms::Texts t;
    t.title = s.sign.empty() ? say(settings::Text::MyShopTitle)
                             : settings::fmt(say(settings::Text::MyShopSign), {s.sign});
    t.left = say(settings::Text::MyShopLeft);
    t.worth = say(settings::Text::MyShopWorth);
    t.close = say(settings::Text::MyShopClose);
    t.empty = say(settings::Text::MyShopSold);
    return t;
}

std::vector<Words> gdiWords(const std::vector<ms::Words>& in) {
    std::vector<Words> out;
    for (const ms::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ms::Align::Right    ? DT_RIGHT
                                 : w.align == ms::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("one's own shop", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    ms::paint(s.picture, s.layout, s.rows, s.show);
    drawWords(s.picture, gdiWords(ms::words(s.layout, s.rows, s.show, texts(s))));
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
    uapad::gamewindow::setBlocked(Blocker::MyShop, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::MyShop, nullptr, nullptr, false);
    s.shown = false;
}

void closeShop(State& s) {
    const uint32_t window = gameShop();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("my shop: «Закрити лавку» - the window's 0x87 (the game closes the shop)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kShopSend))(window, 0, kButtonMsg, ms::kCloseButton, 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("my shop: [interface] my_shop = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameShop() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("my shop: the game's own-shop window - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readShop(s);
    s.layout = ms::layout(targetWidth, targetHeight, s.scale, s.rows.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, ms::mostScroll(s.layout, s.rows.size()));
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hoverClose = ms::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                        float(cy) * float(k) - float(s.layout.y))
                                .part == ms::Part::Close;
    std::string key = s.sign + ';' + std::to_string(s.show.scroll) + (s.show.hoverClose ? "x" : "") + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const ms::Row& r : s.rows)
        key += ';' + r.name + ',' + std::to_string(r.left) + ',' + std::to_string(r.price) + (r.icon ? "i" : "");
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
        s.show.scroll = std::clamp(s.show.scroll + by, 0, ms::mostScroll(s.layout, s.rows.size()));
        s.key.clear();
        return true;
    }
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && ms::hitTest(s.layout, lx, ly).part == ms::Part::Close)
        closeShop(s);
    return true;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameShop(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    switch (c) {
        case PadCommand::Up: s.show.scroll = std::max(0, s.show.scroll - 1); break;
        case PadCommand::Down:
            s.show.scroll = std::min(ms::mostScroll(s.layout, s.rows.size()), s.show.scroll + 1);
            break;
        case PadCommand::Close: closeShop(s); break;
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

}  // namespace uagfx::win::myshopwin
