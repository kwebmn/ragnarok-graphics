#pragma once
#include <windows.h>

#include <functional>

#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::uilive {

// 3.0.18-beta.73 (docs/superpowers/plans/2026-11-10-uaui-big-six.md, F: input through the library): a window of the
// driver's as a live uaUI document - its markup laid out by RmlUi (flexbox), pointed at by RCSS, clicked by its data
// events; the part keeps its place, its drag (a press on its ground), the stack's order, its shadow and the blocker.
// Elements of class «ctl» are its controls; the pad walks the elements the `targets` selector picks (a gold ring on
// the element «#ring» of its markup).
class Window {
public:
    // Made once while uaUI is ready (false: not now)
    bool make(const char* name, const char* rml, const std::function<void(Rml::DataModelConstructor&)>& bind);
    bool made() const { return doc.made(); }
    // Each frame: shown at (x, y) when `on`; the pad's targets for the stack's window `kind`
    void frame(bool on, int x, int y, int kind, const char* targets);
    // From the part's draw: its place in the windows' stack, the stack's gold edge, the driver's shadow under it
    void draw(WindowShadow& shadow, FrameRecord& record, gpu::Renderer& renderer, float scale, float k);
    // A mouse message: 0 not over it, 1 taken, 2 taken - a press on its ground (the part starts its drag)
    int mouse(HWND game, UINT m, WPARAM wp, LPARAM lp);
    bool box(int& x, int& y, int& w, int& h) const { return shown && doc.box(x, y, w, h); }

    uaui::Doc doc;
    uisheet::NavSet nav;
    bool shown = false;
};

}  // namespace uagfx::win::uilive
