#include "win/ui_live.hpp"

#include <string>

#include "win/game_camera.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::uilive {

bool Window::make(const char* name, const char* rml, const std::function<void(Rml::DataModelConstructor&)>& bind) {
    if (doc.made()) return true;
    if (!(anywhere ? uaui::readyAnywhere() : uaui::ready())) return false;
    doc.window = true;
    doc.anywhere = anywhere;
    if (!doc.make(name, rml, bind)) return false;
    nav.shown = [this] { return shown; };
    nav.ring = [this](const panel::Box* b) {
        Rml::Element* r = doc.byId("ring");
        if (!r) return;
        r->SetProperty("display", b ? "block" : "none");
        if (!b) return;
        const float d = 3 * uaui::dp(), z = doc.zoomed() > 0 ? doc.zoomed() : 1;  // .152: its own pixels
        r->SetProperty("left", std::to_string(b->x / z - d) + "px");
        r->SetProperty("top", std::to_string(b->y / z - d) + "px");
        r->SetProperty("width", std::to_string(b->w / z + 2 * d) + "px");
        r->SetProperty("height", std::to_string(b->h / z + 2 * d) + "px");
    };
    return true;
}

void Window::frame(bool on, int x, int y, int kind, const char* targets) {
    shown = on && doc.made() && (anywhere ? uaui::readyAnywhere() : uaui::ready());
    if (!doc.made()) return;
    // 3.0.18-beta.152 (the owner: every window sized by its corner): its own size (win/window_stack)
    if (kind >= 0 && kind < winstack::kWins) doc.zoom(windowstack::sizeOf(winstack::Win(kind)));
    if (!ownDrag) {  // 3.0.18-beta.92: its own shift by the drag, inside the screen
        int bx = 0, by = 0, bw = 0, bh = 0, tw = 0, th = 0;
        uaui::targetSize(tw, th);
        if (!doc.box(bx, by, bw, bh)) bw = bh = 0;
        drag.apply(x, y, bw, bh, tw, th, uaui::targetScale(), 48 * uaui::dp());
    }
    doc.place(x, y);
    doc.show(shown);
    if (shown && targets) nav.set(kind, doc.boxesOf(targets), x, y);
}

void Window::draw(WindowShadow& shadow, FrameRecord& record, gpu::Renderer& renderer, float scale, float k) {
    if (!shown) return;
    doc.raise(10 + uaui::windowRank());
    doc.mark(uaui::windowMarked());
    int x = 0, y = 0, w = 0, h = 0;
    if (doc.box(x, y, w, h)) shadow.draw(record, renderer, x, y, w, h, 12 * uaui::dp(), scale, k);
}

int Window::mouse(HWND game, UINT m, WPARAM wp, LPARAM lp) {
    if (!shown) return 0;
    if (m == WM_LBUTTONUP) drag.end();
    bool control = false;
    if (!doc.pointer(game, m, wp, lp, control)) return 0;
    if (m == WM_LBUTTONDOWN && !control) {
        if (!ownDrag) {  // 3.0.18-beta.92: its ground pressed - the window follows the cursor
            int tw = 0, th = 0;
            uaui::targetSize(tw, th);
            float tx = 0, ty = 0;
            if (pointerInTarget(game, m, lp, uaui::targetScale(), tw, th, tx, ty, nullptr)) drag.begin(tx, ty);
        }
        return 2;
    }
    return 1;
}

}  // namespace uagfx::win::uilive
