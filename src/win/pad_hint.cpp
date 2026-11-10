#include "win/pad_hint.hpp"

#include "win/npc_talk.hpp"
#include "win/shop_window.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "core/pad_glyphs.hpp"
#include "core/pad_hint.hpp"
#include "core/ui_style.hpp"
#include "pad/win/runtime.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::padhintwin {
namespace {

namespace ph = uagfx::padhint;

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.35: the hint drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false, sheetSony = false;
    bool shown = false;
    std::vector<ph::Part> parts;
    ph::Layout layout;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    std::string key;
    float k = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::vector<Words> gdiWords(const std::vector<ph::Words>& in) {
    std::vector<Words> out;
    for (const ph::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ph::Align::Right    ? DT_RIGHT
                                 : w.align == ph::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

}  // namespace

static void frameInner(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    uapad::runtime::setOwnHints(on);
    s.parts.clear();
    // .98: the NPC talk's own footer; .100: the shop's
    // 3.0.11-beta.258: over the talk too while the game's number field is open (its digits' hint)
    const bool talk = npcwin::shown() && !npcwin::numberOpen();
    if (on && game && k > 0 && targetWidth > 0 && targetHeight > 0 && !talk && !shopwin::shown())
        for (const uapad::HintPart& p : uapad::runtime::hints()) {
            ph::Part part{p.before, {}, p.words};
            for (uint16_t b : p.buttons) {
                const padglyph::Button g = padglyph::buttonOf(b);
                if (g != padglyph::Button::kCount) part.buttons.push_back(int(g));
            }
            s.parts.push_back(part);
        }
    if (s.parts.empty()) {
        s.shown = false;
        return;
    }
    const float scale = uistyle::partScale(k);
    s.k = float(k);
    s.layout = ph::layout(s.parts, targetWidth / 2, targetHeight - int(std::lround(104 * scale)), scale);
    const bool sony = uapad::runtime::look().sony;
    std::string key = std::to_string(s.layout.w) + (sony ? "S" : "X");
    for (const ph::Part& p : s.parts) {
        key += p.before + '|' + p.words + '|';
        for (int b : p.buttons) key += std::to_string(b) + ',';
    }
    key += '#' + std::to_string(uistyle::generation()) + (uaui::readyAnywhere() ? "u" : "");
    if (key != s.key) {
        s.key = key;
        s.sheetSony = sony;
        ++s.sheetGen;  // 3.0.18-beta.35: uaUI's line says it all again (no picture then)
        if (!uaui::readyAnywhere() && panel::blank(s.picture, s.layout.w, s.layout.h)) {
            ph::paint(s.picture, s.layout, s.parts, sony);
            drawWords(s.picture, gdiWords(ph::words(s.layout, s.parts, sony)));
            s.upload = true;
        }
        if (s.told++ < 10) LOG_INFO("pad hint: a line of %zu parts in the interface's style", s.parts.size());
    }
    s.shown = true;
}

// 3.0.18-beta.35 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the controller's hint line
// in RmlUi's markup, as core padhint paints it - its pill and each part's buttons; the words where core padhint places
// them. A part of the screen (not a window)
static void fillSheet(State& s, uisheet::Content& c) {
    const ph::Layout& L = s.layout;
    const float sc = L.s;
    c.w = float(L.w), c.h = float(L.h);
    c.rect(c.under, L.glass, L.glass.h / 2, 0x10141C, std::max(0.88f, uistyle::panel()), 0xFFFFFF, 0.10f, 1.0f * sc);
    for (size_t i = 0; i < L.parts.size() && i < s.parts.size(); ++i)
        for (size_t g = 0; g < L.parts[i].glyphs.size() && g < s.parts[i].buttons.size(); ++g) {
            const panel::Box& b = L.parts[i].glyphs[g];
            s.sheet.glyph(c, s.parts[i].buttons[g], s.sheetSony, b.x, b.y, b.h);
        }
    c.say(ph::words(L, s.parts, s.sheetSony));
}

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.top = true;  // 3.0.18-beta.69: over the game's own windows, as its picture was
    s.sheet.anywhere = true;  // 3.0.18-beta.78: the controller's hint on the login's screens too
    s.sheetShown = s.sheet.frame("pad hint", s.shown, false, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.sheetShown || s.k <= 0 || s.picture.width <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
}

}  // namespace uagfx::win::padhintwin
