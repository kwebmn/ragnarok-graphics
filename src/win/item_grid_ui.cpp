#include "win/item_grid_ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::win::itemgridui {
namespace {

namespace iw = uagfx::itemwin;
namespace ig = uagfx::uikit::itemgrid;

std::string hexOf(uint32_t rgb, int alpha) {
    char b[16];
    rgb = uaui::themed(rgb);  // 3.0.18-beta.54: in the theme
    std::snprintf(b, sizeof b, "#%06x%02x", unsigned(rgb & 0xFFFFFF), alpha & 0xFF);
    return b;
}

std::string source(Ui& u, const std::string& key, const panel::Picture* p) {
    if (!p || p->width <= 0) return {};
    return uaui::picture(p, uaui::stamp(p), u.sources[key]);
}

}  // namespace

bool Ui::frame(const char* name, bool on, int x, int y, int generation, const iw::Layout& L,
               const std::vector<iw::Item>& items, const std::vector<int>& shown, const iw::Show& sh,
               const iw::Texts& t, const iw::Measure&) {
    if (!uaui::ready() || !on || L.w <= 0) {
        doc.show(false);
        return false;
    }
    if (!doc.made()) {
        doc.passive = true;  // the window's own mouse, keys and blocker - on RmlUi's boxes (refit)
        doc.window = true;
        if (!doc.make(name, ig::rml(), [&](Rml::DataModelConstructor& c) { ig::bind(c, model); })) return false;
    }
    if (generation != gen) {
        gen = generation;
        model.ground = hexOf(0x10141C, int(std::lround(uistyle::window() * 255)));
        const int side = std::max(8, int(std::lround(15 * uaui::dp())));
        auto it = made.find("search" + std::to_string(side));
        if (it == made.end()) {
            panel::Picture p;
            p.width = p.height = side;
            p.px.assign(size_t(side) * size_t(side), 0u);
            uiicons::draw(p, uiicons::Icon::Search, 0, 0, float(side), 0x9AA3B5, 0.9f, 2.2f);
            it = made.emplace("search" + std::to_string(side), std::move(p)).first;
        }
        model.searchIcon = source(*this, it->first, &it->second);
        ig::fill(model, L, items, shown, sh, t, [&](const panel::Picture* p) {
            return source(*this, "i:" + std::to_string(uintptr_t(p)), p);
        });
        doc.dirtyAll();
    }
    if (model.tip) {  // beside the pointed cell, by the tip's own size as laid out last
        const float tx = model.tipX, ty = model.tipY;
        ig::placeTip(doc.doc(), model, L, sh.hover - sh.scroll * std::max(1, L.columns));
        if (tx != model.tipX || ty != model.tipY) doc.dirty("tipX"), doc.dirty("tipY");
    }
    doc.place(x, y);
    doc.show(true);
    return true;
}

void Ui::refit(iw::Layout& L) const {
    if (doc.shown()) ig::refit(doc.doc(), L);
}

}  // namespace uagfx::win::itemgridui
