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

namespace uagfx::win::padhintwin {
namespace {

namespace ph = uagfx::padhint;

struct State {
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

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    uapad::runtime::setOwnHints(on);
    s.parts.clear();
    // .98: the NPC talk's own footer; .100: the shop's
    if (on && game && k > 0 && targetWidth > 0 && targetHeight > 0 && !npcwin::shown() && !shopwin::shown())
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
    key += '#' + std::to_string(uistyle::generation());
    if (key != s.key) {
        s.key = key;
        if (panel::blank(s.picture, s.layout.w, s.layout.h)) {
            ph::paint(s.picture, s.layout, s.parts, sony);
            drawWords(s.picture, gdiWords(ph::words(s.layout, s.parts, sony)));
            s.upload = true;
        }
        if (s.told++ < 10) LOG_INFO("pad hint: a line of %zu parts in the interface's style", s.parts.size());
    }
    s.shown = true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
}

}  // namespace uagfx::win::padhintwin
