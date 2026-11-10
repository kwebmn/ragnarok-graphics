#include "win/ui_live.hpp"

#include <string>

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
        const float d = 3 * uaui::dp();
        r->SetProperty("left", std::to_string(b->x - d) + "px");
        r->SetProperty("top", std::to_string(b->y - d) + "px");
        r->SetProperty("width", std::to_string(b->w + 2 * d) + "px");
        r->SetProperty("height", std::to_string(b->h + 2 * d) + "px");
    };
    return true;
}

void Window::frame(bool on, int x, int y, int kind, const char* targets) {
    shown = on && doc.made() && (anywhere ? uaui::readyAnywhere() : uaui::ready());
    if (!doc.made()) return;
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
    bool control = false;
    if (!doc.pointer(game, m, wp, lp, control)) return 0;
    return m == WM_LBUTTONDOWN && !control ? 2 : 1;
}

}  // namespace uagfx::win::uilive
