#include "win/pad_assign.hpp"

#include <windowsx.h>

#include <string>
#include <vector>

#include "core/pad_assign.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/hotbar.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::padassignwin {
namespace {

namespace pa = uagfx::padassign;

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.35: drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false, sheetSony = false;
    std::vector<Words> sheetWords;
    bool open = false, shown = false;
    std::string value, what;
    int level = 0;
    int focus = 0;
    std::vector<pa::Row> rows;
    std::vector<int> slots;  // each row's cell of the bar
    pa::Layout layout;
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

std::string slotLabel(int slot) {
    const std::string key = "F" + std::to_string(slot % 9 + 1);
    return slot < 9 ? key : key + " \xC2\xB7 " + std::to_string(slot / 9 + 1);
}

void readRows(State& s) {  // the buttons that use the bar's cells, as the layout in use has them
    s.rows.clear();
    s.slots.clear();
    const uapad::runtime::Look look = uapad::runtime::look();
    for (size_t i = 0; i < look.bindings.size(); ++i)
        if (look.bindings[i].kind == uapad::Action::Kind::Slot) {
            const int slot = int(look.bindings[i].slot);
            s.rows.push_back(pa::Row{int(i), slotLabel(slot), hotbarwin::slotWords(slot)});
            s.slots.push_back(slot);
        }
    if (s.focus >= int(s.rows.size())) s.focus = 0;
}

std::vector<Words> gdiWords(const std::vector<pa::Words>& in) {
    std::vector<Words> out;
    for (const pa::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == pa::Align::Right    ? DT_RIGHT
                                 : w.align == pa::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s, bool sony) {
    using settings::Text;
    const settings::Lang l = language();
    std::vector<Words> words = gdiWords(pa::words(s.layout, s.rows, s.what, settings::tr(l, Text::AssignTitle),
                                            settings::tr(l, Text::AssignPut), settings::tr(l, Text::AssignCancel),
                                            settings::tr(l, Text::AssignEmpty), sony));
    s.sheetWords = std::move(words);  // 3.0.18-beta.35: the words for uaUI's sheet (.158: no picture)
    s.sheetSony = sony;
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const RECT r{LONG(int64_t(s.layout.x) * client.right / s.targetW),
                 LONG(int64_t(s.layout.y) * client.bottom / s.targetH),
                 LONG(int64_t(s.layout.x + s.layout.w) * client.right / s.targetW + 1),
                 LONG(int64_t(s.layout.y + s.layout.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::PadAssign, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::PadAssign, nullptr, nullptr, false);
    s.shown = false;
}

void put(State& s) {
    if (s.focus < 0 || s.focus >= int(s.slots.size())) return;
    const int slot = s.slots[size_t(s.focus)];
    const bool done = hotbarwin::assign(slot, s.value, s.level);
    LOG_INFO("pad assign: %s into %s - %s", s.value.c_str(), slotLabel(slot).c_str(),
             done ? "put" : "not possible now");
    s.open = false;
}

}  // namespace

bool isOpen() { return state().open; }

void command(Command c) {
    State& s = state();
    if (!s.open) return;
    const int n = int(s.rows.size());
    switch (c) {
        case Command::Up: s.focus = n ? (s.focus + n - 1) % n : 0; break;
        case Command::Down: s.focus = n ? (s.focus + 1) % n : 0; break;
        case Command::Enter: put(s); break;
        case Command::Close:
            s.open = false;
            LOG_INFO("pad assign: cancelled");
            break;
    }
}

static void frameInner(HWND game, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    readRows(s);
    s.k = float(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.layout = pa::layout(s.rows.size(), targetWidth / 2, targetHeight / 2, uistyle::partScale(k));
    const bool sony = uapad::runtime::look().sony;
    std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' + std::to_string(s.focus) +
                      (sony ? "S" : "X") + ';' + s.what;
    for (const pa::Row& r : s.rows) key += r.slot + '|' + r.now + ';';
    key += '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
    if (key != s.key) {
        s.key = key;
        render(s, sony);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.35 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the controller's button
// assignment in RmlUi's markup, line by line as core padassign paints it - its glass, the hints' line, the rows (the
// focused gold) with their buttons, A and B; the words as the part lays them out
static void fillSheet(State& s, uisheet::Content& c) {
    const pa::Layout& L = s.layout;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A;
    constexpr float kGlyph = 20;  // core padassign's
    c.w = float(L.w), c.h = float(L.h);
    c.rect(c.under, L.glass, 14 * sc, kCard, std::max(0.92f, uistyle::panel()), kWhite, 0.10f, 1.0f * sc);
    c.rect(c.under, panel::Box{L.hints.x, L.hints.y - 4 * sc, L.hints.w, 1 * sc}, 0, kWhite, 0.09f);
    for (size_t i = 0; i < L.rows.size() && i < s.rows.size(); ++i) {
        const panel::Box& b = L.rows[i];
        const bool on = int(i) == s.focus;
        c.rect(c.under, b, 9 * sc, on ? kGold : kWhite, on ? 0.13f : 0.04f, on ? kGold2 : kWhite, on ? 1.0f : 0.08f,
               (on ? 2.0f : 1.0f) * sc);
        s.sheet.glyph(c, s.rows[i].button, s.sheetSony, b.x + 8 * sc, b.y + (b.h - kGlyph * sc) / 2, kGlyph * sc);
    }
    const float y = L.hints.y + (L.hints.h - kGlyph * sc) / 2;
    s.sheet.glyph(c, int(padglyph::Button::A), s.sheetSony, L.hints.x, y, kGlyph * sc);
    s.sheet.glyph(c, int(padglyph::Button::B), s.sheetSony, L.hints.x + 140 * sc, y, kGlyph * sc);
    uisheet::sayGdi(c, s.sheetWords);
}

void frame(HWND game, double k, int targetWidth, int targetHeight) {
    frameInner(game, k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.top = true;  // 3.0.18-beta.69: over the game's own windows, as its picture was
    s.sheetShown = s.sheet.frame("pad assign", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    state().sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown || (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK && m != WM_LBUTTONUP && m != WM_MOUSEMOVE &&
                      m != WM_RBUTTONDOWN))
        return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, double(s.k), s.targetW, s.targetH, tx, ty)) return false;
    const float x = tx - float(s.layout.x), y = ty - float(s.layout.y);
    const int row = pa::hitTest(s.layout, x, y);
    if (m == WM_MOUSEMOVE) {
        if (row >= 0) s.focus = row;
        return false;
    }
    result = 0;
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && row >= 0) {
        s.focus = row;
        put(s);
    } else if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN) &&
               !s.layout.glass.contains(x, y)) {
        s.open = false;
        LOG_INFO("pad assign: cancelled (a click outside)");
    }
    (void)wp;
    return true;  // modal
}

}  // namespace uagfx::win::padassignwin
