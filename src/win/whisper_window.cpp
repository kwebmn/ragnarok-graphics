#include "win/card_drag.hpp"
#include "win/game_memory.hpp"
#include "win/whisper_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/whisper_card.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::whisperwin {
namespace {

namespace wc = uagfx::whispercard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the windows' list (a ring of {next, prev, window} at the manager's + 0x38), a whisper window's class,
// SendMsg, place, partner, edit and list box (its words' and colours' firsts and pasts); an edit's words, the
// manager's keys and SetFocus
constexpr uint32_t kWindowMgr = 0x727ED8, kWindowsList = kWindowMgr + 0x38, kWhisperClass = 0x699BC8;
constexpr uint32_t kWhisperSend = 0x4869E0, kSendMsgSlot = 0x94, kRect = 0x14, kPartner = 0x64, kEdit = 0x84;
constexpr uint32_t kLinesBox = 0x88, kWords = 0x58, kColours = 0x94;
constexpr uint32_t kGetText = 0x443300, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr int kButtonMsg = 6, kSend = 0x76, kClose = 0x87, kAway = -4000, kMostWindows = 512, kMostLines = 120;
constexpr ULONGLONG kBlinkMs = 530;
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

struct One {
    int sheetGen = 0;  // 3.0.18-beta.32: its uaUI sheet's (State::sheets, by its window)
    uint32_t window = 0;
    int slot = 0;  // its place in the cascade, kept while it lives
    int oldX = 0;
    bool moved = false;
    CardDrag move;  // 3.0.18-beta.92 (the owner: «не все окна перетаскиваются»): by its empty part
    wc::Card card;
    wc::Layout layout;
    wc::Part hover = wc::Part::None;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    WindowShadow shadow;
};

struct State {
    bool on = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<One> cards;  // the oldest first; the last drawn on top
    // 3.0.18-beta.32: the cards drawn by uaUI - a sheet each, by its place in the cascade (kept apart: a card moves in
    // `cards`; never let go - a uaUI document lives as long as the game)
    std::map<int, std::unique_ptr<uisheet::Sheet>> sheets;
    bool sheetsShown = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string text(const State& s, uint32_t at) {
    char buf[128] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return utf8Of(widened(buf, s.codePage));
}

// The game's whisper windows in its list now
std::vector<uint32_t> gameWhispers() {
    std::vector<uint32_t> out;
    const uint32_t head = read32(kWindowsList);
    size_t seen = 0;
    for (uint32_t node = head ? read32(head) : 0; node && node != head && seen < size_t(kMostWindows);
         node = read32(node)) {
        ++seen;
        const uint32_t w = read32(node + 8);
        if (w && read32(w) == kWhisperClass && read32(kWhisperClass + kSendMsgSlot) == kWhisperSend) out.push_back(w);
    }
    return out;
}

void hold(One& o, bool on) {
    const int32_t x = int32_t(read32(o.window + kRect));
    if (on && x != kAway) {
        o.oldX = x;
        write32(o.window + kRect, uint32_t(kAway));
        o.moved = true;
    } else if (!on && (o.moved || x == kAway)) {
        write32(o.window + kRect, uint32_t(o.moved && o.oldX != kAway ? o.oldX : 40));
        o.moved = false;
    }
}

// The cards follow the game's windows: gone ones out, new ones on top (each in the first free place of the cascade);
// the game's out of sight while the cards are on
void sync(State& s, bool on, bool world) {
    const std::vector<uint32_t> windows = world ? gameWhispers() : std::vector<uint32_t>{};
    std::vector<One> kept;
    for (One& o : s.cards)
        if (std::find(windows.begin(), windows.end(), o.window) != windows.end()) kept.push_back(std::move(o));
    for (const uint32_t w : windows)
        if (std::none_of(kept.begin(), kept.end(), [&](const One& o) { return o.window == w; })) {
            One o;
            o.window = w;
            while (std::any_of(kept.begin(), kept.end(), [&](const One& k) { return k.slot == o.slot; })) ++o.slot;
            kept.push_back(std::move(o));
            LOG_INFO("whispers: a whisper window (%zu now) - %s", windows.size(),
                     on ? "the new card shows it" : "the game's own");
        }
    s.cards = std::move(kept);
    for (One& o : s.cards) hold(o, on && world);
}

void readCard(const State& s, One& o) {
    wc::Card& c = o.card;
    c.name = text(s, read32(o.window + kPartner));
    c.lines.clear();
    const uint32_t box = read32(o.window + kLinesBox);
    if (box) {
        const uint32_t from = read32(box + kWords), past = read32(box + kWords + 4);
        std::vector<uint32_t> colours;
        const uint32_t cf = read32(box + kColours), cp = read32(box + kColours + 4);
        if (cf && cp > cf && (cp - cf) / 4 <= 4096)
            for (uint32_t at = cf; at < cp; at += 4) colours.push_back(read32(at));
        if (from && past > from && (past - from) / 16 <= 4096) {
            const size_t all = (past - from) / 16, first = all > size_t(kMostLines) ? all - size_t(kMostLines) : 0;
            for (size_t i = first; i < all; ++i) {
                const uint32_t bgr = i < colours.size() ? colours[i] : 0;
                const uint32_t rgb = bgr ? (bgr & 0xFF) << 16 | (bgr & 0xFF00) | (bgr >> 16 & 0xFF) : 0;
                c.lines.push_back(wc::Line{text(s, read32(from + uint32_t(i) * 16 + 4)), rgb});
            }
        }
    }
    const uint32_t edit = read32(o.window + kEdit);
    c.input.clear();
    if (edit) {
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
        c.input = text(s, uint32_t(reinterpret_cast<uintptr_t>(t)));
    }
    c.focus = edit && read32(kFocus) == edit;
    c.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
}

wc::Texts texts() {
    wc::Texts t;
    t.with = say(settings::Text::WhisperCardWith);
    t.send = say(settings::Text::WhisperCardSend);
    return t;
}

void render(One& o) {
    ++o.sheetGen;
}

// The cards' boxes as the game's mouse sees them (the topmost one's: the part takes the world's clicks under it)
void block(const State& s, HWND game) {
    RECT client{};
    if (s.cards.empty() || s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    int x0 = 1 << 30, y0 = 1 << 30, x1 = 0, y1 = 0;
    for (const One& o : s.cards) {
        x0 = std::min(x0, o.layout.x);
        y0 = std::min(y0, o.layout.y);
        x1 = std::max(x1, o.layout.x + o.layout.w);
        y1 = std::max(y1, o.layout.y + o.layout.h);
    }
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x1) * client.right / s.targetW + 1), LONG(int64_t(y1) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::Whispers, game, &r, false);
}

void act(State& s, One& o, wc::Part p) {
    switch (p) {
        case wc::Part::Input: {
            const uint32_t edit = read32(o.window + kEdit);
            if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
            break;
        }
        case wc::Part::Send:
            if (s.told++ < 30) LOG_INFO("whispers: sent to %s (the window's 0x76)", o.card.name.c_str());
            reinterpret_cast<SendMsgFn>(uintptr_t(kWhisperSend))(o.window, 0, kButtonMsg, kSend, 0, 0);
            break;
        case wc::Part::Close:
            if (s.told++ < 30) LOG_INFO("whispers: %s's window closed (0x87)", o.card.name.c_str());
            reinterpret_cast<SendMsgFn>(uintptr_t(kWhisperSend))(o.window, 0, kButtonMsg, kClose, 0, 0);
            break;
        default: break;
    }
    o.key.clear();
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        LOG_INFO("whispers: [interface] whispers = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    sync(s, on, world);
    if (!on || s.cards.empty() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        uapad::gamewindow::setBlocked(Blocker::Whispers, nullptr, nullptr, false);
        if (!on) s.cards.clear();
        return;
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    int cx = 0, cy = 0;
    const bool cursor = readHoverCursor(cx, cy);
    bool covered = false;  // the hover only on the topmost card under the cursor
    for (size_t i = s.cards.size(); i-- > 0;) {
        One& o = s.cards[i];
        readCard(s, o);
        o.layout = wc::layout(o.slot, targetWidth, targetHeight, s.scale);
        o.move.apply(o.layout.x, o.layout.y, o.layout.w, o.layout.h, targetWidth, targetHeight, k, 48 * s.scale);
        o.hover = wc::Part::None;
        if (cursor && !covered) {
            o.hover = wc::hitTest(o.layout, float(cx) * float(k) - float(o.layout.x),
                                  float(cy) * float(k) - float(o.layout.y));
            covered = o.hover != wc::Part::None;
        }
        const wc::Card& c = o.card;
        std::string key = c.name + ';' + c.input + (c.focus ? "F" : "") + (c.caret ? "|" : "") + ';' +
                          std::to_string(int(o.hover)) + ';' + std::to_string(o.layout.x) + ',' +
                          std::to_string(o.layout.y) + '#' + std::to_string(uistyle::generation()) + ';' +
                          std::to_string(c.lines.size());
        if (!c.lines.empty()) key += c.lines.back().text;
        if (uaui::ready()) key += 'u';  // 3.0.18-beta.32
        if (key != o.key) {
            o.key = key;
            render(o);
        }
    }
    block(s, game);
}

// 3.0.18-beta.32 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): a whisper's card in
// RmlUi's markup, line by line as core whispercard paints it - its glass, ✕, the lines' ground, the input, «Надіслати»;
// the words where core whispercard places them
static void fillSheet(const One& o, uisheet::Content& c) {
    const wc::Layout& L = o.layout;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kBlack = 0x000000;
    auto& u = c.under;
    c.w = float(L.w), c.h = float(L.h);
    c.glass = uisheet::Box{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * sc, 1.0f * sc, 1, "glass"};
    c.ground = uisheet::Sheet::ground(kCard, uistyle::window());
    const bool closeHot = o.hover == wc::Part::Close;
    c.rect(u, L.close, 6 * sc, kWhite, closeHot ? 0.14f : 0.0f, kWhite, closeHot ? 0.3f : 0.0f, 1.0f * sc);
    c.rect(u, L.lines, 8 * sc, kBlack, 0.25f, kWhite, 0.06f, 1.0f * sc);
    c.rect(u, L.input, 8 * sc, kBlack, 0.35f, o.card.focus ? kGold : kWhite, o.card.focus ? 0.8f : 0.18f, 1.0f * sc);
    c.rect(u, L.send, 8 * sc, kGold, o.hover == wc::Part::Send ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * sc);
    c.say(wc::words(L, o.card, o.hover, texts()));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetsShown = false;
    for (auto& [slot, sheet] : s.sheets) {  // the places no card stands in: hidden
        const int at = slot;
        if (std::none_of(s.cards.begin(), s.cards.end(), [&](const One& o) { return o.slot == at; }))
            sheet->frame("whisper", false, true, 0, 0, 0, [](uisheet::Content&) {});
    }
    for (const One& o : s.cards) {
        auto& sheet = s.sheets[o.slot];
        if (!sheet) sheet = std::make_unique<uisheet::Sheet>();
        const int gen = int(o.window * 2654435761u) ^ o.sheetGen;  // another card in its place: said again
        const bool shown = sheet->frame("whisper", s.k > 0, true, o.layout.x, o.layout.y, gen,
                                        [&](uisheet::Content& c) { fillSheet(o, c); });
        s.sheetsShown = s.sheetsShown || shown;
        if (shown && &o == &s.cards.back())  // 3.0.18-beta.63: the top card walked by the pad (A clicks)
            sheet->nav(int(winstack::Win::Whispers), {o.layout.input, o.layout.send, o.layout.close});
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.cards.empty() || s.k <= 0 || !s.sheetsShown) return;  // the old edition (its pictures) gone
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    for (One& o : s.cards) {  // 3.0.18-beta.43: the shadow the driver's, under uaUI's cards too
        const panel::Box& g = o.layout.glass;
        o.shadow.draw(record, renderer, o.layout.x + int(g.x), o.layout.y + int(g.y), int(g.w), int(g.h),
                      12 * s.scale, s.scale, s.k);
    }
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (s.cards.empty()) return false;
    if (m == WM_LBUTTONUP)
        for (One& o : s.cards) o.move.end();  // 3.0.18-beta.92
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    for (size_t i = s.cards.size(); i-- > 0;) {
        One& o = s.cards[i];
        const float lx = tx - float(o.layout.x), ly = ty - float(o.layout.y);
        if (!o.layout.glass.contains(lx, ly)) continue;
        result = 0;
        if (m == WM_LBUTTONDOWN) {
            const uint32_t window = o.window;
            const wc::Part h = wc::hitTest(o.layout, lx, ly);
            if (h == wc::Part::Body || h == wc::Part::None) o.move.begin(tx, ty);  // 3.0.18-beta.92
            act(s, o, h);
            // the clicked one on top (its place in the cascade kept)
            const auto at =
                std::find_if(s.cards.begin(), s.cards.end(), [&](const One& c) { return c.window == window; });
            if (at != s.cards.end() && at + 1 != s.cards.end()) std::rotate(at, at + 1, s.cards.end());
        }
        return true;
    }
    return false;
}

bool shown() { return !state().cards.empty(); }

bool close() {
    State& s = state();
    if (s.cards.empty()) return false;
    act(s, s.cards.back(), wc::Part::Close);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) sync(s, true, true);  // a window made since the frame out of sight before its draw
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (s.cards.empty()) return false;
    const wc::Layout& L = s.cards.back().layout;
    x = L.x;
    y = L.y;
    w = L.w;
    h = L.h;
    return true;
}

}  // namespace uagfx::win::whisperwin
