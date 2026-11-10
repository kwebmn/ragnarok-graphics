#pragma once
#include <functional>
#include <map>
#include <string>

#include "core/ui_style.hpp"
#include "core/window_panel.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win {

// 3.0.18-beta.95 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): a window with drag and drop drawn live - its
// markup and model from uikit (laid out by RmlUi, tested on the host), the window's own input on RmlUi's boxes (the
// module's refit into its core layout). Passive: the window keeps its mouse, keys, blocker and drag.
template <class Model>
struct LivePart {
    // Shown at (x, y) when uaUI is ready and `on`; `fill` when `generation` changed (the model then said again)
    bool frame(const char* name, bool on, int x, int y, int generation, const char* rml,
               const std::function<void(Rml::DataModelConstructor&, Model&)>& bind,
               const std::function<void(Model&)>& fill) {
        if (!uaui::ready() || !on) {
            doc.show(false);
            return false;
        }
        if (!doc.made()) {
            doc.passive = true;
            doc.window = true;
            if (!doc.make(name, rml, [&](Rml::DataModelConstructor& c) { bind(c, model); })) return false;
        }
        if (generation != gen) {
            gen = generation;
            fill(model);
            doc.dirtyAll();
        }
        doc.place(x, y);
        doc.show(true);
        return true;
    }
    void raise() {
        doc.raise(10 + uaui::windowRank());
        doc.mark(uaui::windowMarked());
    }
    bool shown() const { return doc.shown(); }
    std::string icon(const panel::Picture* p) {
        if (!p || p->width <= 0) return {};
        return uaui::picture(p, uaui::stamp(p), sources["i:" + std::to_string(uintptr_t(p))]);
    }
    static std::string ground() { return uisheet::Sheet::ground(0x10141C, uistyle::window()); }

    uaui::Doc doc;
    Model model;
    int gen = -1;
    std::map<std::string, std::string> sources;
};

}  // namespace uagfx::win
