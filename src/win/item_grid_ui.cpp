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

std::string hexOf(uint32_t rgb, int alpha, uitheme::Group g) {
    char b[16];
    rgb = uaui::themed(rgb, g);  // 3.0.18-beta.54: in the theme; 3.0.24-beta.19: of the window's group
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
        const ig::Model before = model;  // 3.0.18-beta.175: what changed alone said to RmlUi
        model.acts = acts;               // 3.0.24-beta.51
        model.ground = hexOf(0x10141C, int(std::lround(uistyle::window() * 255)), uitheme::groupOf(name));
        // the drawn icons at the window's size, in its group's theme (3.0.24-beta.20: the key holds the colour - a
        // new theme draws them again); the star a picture, never a letter (R-WP985Q)
        const uitheme::Group group = uitheme::groupOf(name);
        const auto icon = [&](const char* what, uiicons::Icon i, float px, uint32_t rgb, float stroke) {
            const int side = std::max(8, int(std::lround(px * L.s)));
            const uint32_t c = uaui::themed(rgb, group);
            const std::string key = std::string(what) + std::to_string(side) + ':' + std::to_string(c);
            auto it = made.find(key);
            if (it == made.end()) {
                panel::Picture p;
                p.width = p.height = side;
                p.px.assign(size_t(side) * size_t(side), 0u);
                uiicons::draw(p, i, 0, 0, float(side), c, 0.95f, stroke);
                it = made.emplace(key, std::move(p)).first;
            }
            return source(*this, it->first, &it->second);
        };
        model.searchIcon = icon("search", uiicons::Icon::Search, 15, 0x9AA3B5, 2.2f);
        model.starIcon = icon("star", uiicons::Icon::Star, 13, 0x9AA3B5, 2.0f);
        model.starOnIcon = icon("staron", uiicons::Icon::Star, 13, 0xF3C653, 2.6f);
        // 3.0.24-beta.50 (the audit 28.11, item 6: the turned words hard to read): the tabs' pictures in iw::Tab's
        // order, each in its state's ink as the words were (on, pointed at, the rest)
        static constexpr uiicons::Icon kTabIcons[iw::kTabs] = {uiicons::Icon::Grid, uiicons::Icon::Item,
                                                               uiicons::Icon::Equip, uiicons::Icon::Card,
                                                               uiicons::Icon::Box, uiicons::Icon::Star};
        model.tabIcons.resize(size_t(iw::kTabs));
        for (int i = 0; i < iw::kTabs; ++i) {
            const bool on = i == int(sh.tab), hot = i == sh.hoverTab;
            const uint32_t ink = on ? 0xEEF2F8 : hot ? 0xC9D1DE : 0x7E889A;
            const std::string what = "tab" + std::to_string(i) + (on ? "o" : hot ? "h" : "");
            model.tabIcons[size_t(i)] = icon(what.c_str(), kTabIcons[i], 17, ink, on ? 2.2f : 2.0f);
        }
        ig::fill(model, L, items, shown, sh, t, [&](const panel::Picture* p) {
            return source(*this, "i:" + std::to_string(uintptr_t(p)), p);
        });
        // 3.0.18-beta.175 (R-6PYQQG: ~60 times a second all of it while open - each use's flash a generation)
        for (const char* name : ig::changed(before, model)) doc.dirty(name);
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
